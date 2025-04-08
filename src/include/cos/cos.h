/*  The constraint solver's main header file.

    Copyright (C) 2025 Ivan Klikovac
    This program is free software. */

#ifndef SYMEXEC_COS_COS_H
#define SYMEXEC_COS_COS_H

#include <exception>
#include <memory>
#include <vector>
#include <utility>
#include <string>
#include <assert.h>
#include <variant>
#include <map>

#include <gcc-plugin.h>
#include <tree.h>

using std::string;
using std::vector;
using std::variant;

//extern "C" {

constexpr char* op_to_str(tree_code op)
{
    if(op == LT_EXPR) return (char*) "<";
    if(op == LE_EXPR) return (char*) "<=";
    if(op == GT_EXPR) return (char*) ">";
    if(op == GE_EXPR) return (char*) ">=";
    if(op == EQ_EXPR) return (char*) "==";
    if(op == NE_EXPR) return (char*) "!=";

    if(op == PLUS_EXPR) return (char*) "+";
    if(op == MINUS_EXPR) return (char*) "-";
    if(op == MULT_EXPR) return (char*) "*";
    if(op == TRUNC_DIV_EXPR) return (char*) "/";

    return (char*) "huh";
}

enum cos_result
{
    UNKNOWN,
    SATISFIED,
    UNSATISFIABLE
};

// The necessary layer of indirection to support proper mathematical expressions.
struct expr;
inline string expr_to_str(const expr*);

struct symbolic
{
    tree ssa_name; // base name
    unsigned int version; // SSA version number
    tree type; // will type info be useful? might as well have it idk

    symbolic(tree ssa): ssa_name{ssa}, version{SSA_NAME_VERSION(ssa)}, type{TREE_TYPE(ssa)}
    {
        //assert(TREE_CODE(ssa) == SSA_NAME);
    }

    symbolic(unsigned int v): version{v} {}

    bool operator==(const symbolic& other) const { return version == other.version; }
    bool operator<(const symbolic& other) const { return version < other.version; }

    string str() const { return "var_" + std::to_string(version); } // adjust this later
};

using concrete = variant<long, double>;

struct value
{
    variant<concrete, symbolic, expr*> content;

    value(long l): content{concrete{l}} {}
    value(double d): content{concrete{d}} {}
    value(concrete v): content{v} {}
    value(symbolic s): content{s} {}
    value(tree ssa): content{symbolic{ssa}} {}
    value(expr* e): content{e} {}
    value(const value& other) = default;

    bool is_concrete() const { return std::holds_alternative<concrete>(content); }
    bool is_symbolic() const { return std::holds_alternative<symbolic>(content); }
    bool is_integral() const { return is_concrete() && std::holds_alternative<long>(std::get<concrete>(content)); }
    bool is_floating() const { return is_concrete() && std::holds_alternative<double>(std::get<concrete>(content)); }
    bool is_expr() const { return std::holds_alternative<expr*>(content); }

    template<typename T>
    T get_concrete() const
    {
        assert(is_concrete() && "value.get_concrete");
        return std::get<T>(std::get<concrete>(content));
    }

    const symbolic& get_symbolic() const
    {
        assert(is_symbolic() && "value.get_symbolic");
        return std::get<symbolic>(content);
    }

    const expr* get_expr() const
    {
        assert(is_expr() && "value.get_expr");
        return std::get<expr*>(content);
    }

    bool operator==(const value& other) const
    {
        if(is_concrete() && other.is_concrete()) {
            return std::visit(overloaded{
                [](long a, long b) { return a == b; },
                [](double a, double b) { return a == b; },
                [](expr* a, expr* b) { return a == b; },
                [](auto, auto) { return false; } // different types
            }, std::get<concrete>(content), std::get<concrete>(other.content));
        }

        if(is_symbolic() && other.is_symbolic()) {
            return std::get<symbolic>(content) == std::get<symbolic>(other.content);
        }

        return false;
    }

    bool operator<(const value& other) const
    {
        if(is_concrete() && other.is_concrete()) {
            return std::visit(overloaded{
                [](long a, long b) { return a < b; },
                [](double a, double b) { return a < b; },
                [](long a, double b) { return static_cast<double>(a) < b; },
                [](double a, long b) { return a < static_cast<double>(b); }
            }, std::get<concrete>(content), std::get<concrete>(other.content));
        }

        assert(0 && "can't compare symbolic values directly");
    }

    bool operator>(const value& other) const { return other < *this; }
    bool operator<=(const value& other) const { return !(*this > other); }
    bool operator>=(const value& other) const { return !(*this < other); }
    bool operator!=(const value& other) const { return !(other == *this); }

    string str() const
    {
        return std::visit(overloaded{
            [](const concrete& c) -> string {
                return std::visit([](auto&& arg) {
                    return std::to_string(arg);
                }, c);
            },
            [](const symbolic& s) -> string {
                return s.str();
            },
            [](const expr* e) -> string {
                return expr_to_str(e);
            }
        }, content);
    }

    template<class... stuff> struct overloaded: stuff... { using stuff::operator()...; };
    template<class... stuff> overloaded(stuff...) -> overloaded<stuff...>;

    ~value() = default;
};

struct expr
{
    value lhs;
    tree_code op;
    value rhs;

    expr(value& l, tree_code o, value& r): lhs{l}, op{o}, rhs{r} {}

    static expr* new_expr(const value& l, const tree_code o, const value& r)
    {
        expr* e = (expr*) xmalloc(sizeof(expr));
        e->lhs = l;
        e->op = o;
        e->rhs = r;

        return e;
    }

    string str() const
    {
        return "(" + lhs.str() + " " + op_to_str(op) + " " + rhs.str() + ")";
    }

    ~expr() { delete this; }
};

inline string expr_to_str(const expr* e) { return e->str(); }

struct term
{
    value lhs;
    tree_code op;
    value rhs;

    term(const value& l, tree_code o, const value& r): lhs{l}, op{o}, rhs{r} {}
    
    term(const term& t) = default;

    term(tree cond):
    lhs{from_tree(TREE_OPERAND(cond, 0))},
    op{TREE_CODE(cond)},
    rhs{from_tree(TREE_OPERAND(cond, 0))}
    {
        assert(op == EQ_EXPR || op == NE_EXPR || op == LT_EXPR
            || op == LE_EXPR || op == GT_EXPR || op == GE_EXPR);
    }

    term& operator=(const term& original)
    {
        lhs = original.lhs;
        op = original.op;
        rhs = original.rhs;

        return *this;
    }

    static value from_tree(tree t)
    {
        switch(TREE_CODE(t)) {
            case SSA_NAME: return value(t);
            case INTEGER_CST: return value((long) TREE_INT_CST_LOW(t));
            case REAL_CST: return value((double) 0); // later
            default:
            assert(0);
        }
    }

    term operator!()
    {
        term negated = {*this};
        
        switch(op) {
            case EQ_EXPR: negated.op = NE_EXPR; break;
            case NE_EXPR: negated.op = EQ_EXPR; break;
            case LT_EXPR: negated.op = GE_EXPR; break;
            case LE_EXPR: negated.op = GT_EXPR; break;
            case GT_EXPR: negated.op = LE_EXPR; break;
            case GE_EXPR: negated.op = LT_EXPR; break;
            default: assert(0 && "term::operator!: unknown op");
        }

        return negated;
    }
    
    string str() const 
    { 
        std::string s = "(" + lhs.str() + " "
                        + op_to_str(op) + " " 
                        + rhs.str();
        s += ")";

        return s;
    }

    ~term() = default;
};

struct inner
{
    vector<term> ands;
    bool unsatisfiable = false;

    inner(): ands{} {}
    inner(const inner& original): ands{original.ands}, 
        unsatisfiable{original.unsatisfiable} {}

    inner& operator=(const inner& original)
    {
        ands = original.ands;
        unsatisfiable = original.unsatisfiable;
        return *this;
    }

    string str() const
    {
        string s = "[";
        
        if(ands.empty()) {
            s += "empty]";
            return s;
        }
        if(unsatisfiable) {
            s += "unsatisfiable]";
            return s;
        }

        for(int i = 0; i < ands.size() - 1; i++) {
            s += ands[i].str();
            s += " AND ";
        }
        s += ands.back().str();
        s += "]";
        
        return s;
    }

    void add_constraint(term& t)
    {
        if(unsatisfiable) return;

        ands.push_back(t);
    }

    void simplify();

    ~inner() = default;
};

struct outer
{
    vector<inner> ors;

    outer(): ors{1} {};
    outer(const outer& original): ors{original.ors} {}

    outer& operator=(const outer& original) 
    { 
        ors = original.ors;
        return *this;
    }

    void add_constraint(term& t)
    {
        for(auto& inner: ors) {
            inner.add_constraint(t);
        }
    }

    void merge(const outer& other)
    {
        ors.insert(ors.end(), other.ors.begin(), other.ors.end());
    }

    string str() const
    {
        string s = "";

        if(ors.empty()) {
            s += "(empty)";
            return s;
        }
        
        for(int i = 0; i < ors.size() - 1; i++) {
            s += ors[i].str();
            s += " OR ";
        }
        s += ors.back().str();

        return s;
    }

    ~outer() = default;
};

// The path condition for a basic block, stored in DNF form
struct state
{
    outer pc;
    basic_block* bb;

    state(): pc{} {};
    state(const state& original): pc{original.pc} {}
    state& operator=(const state& original) { pc = original.pc; return *this; }

    string str()
    {
        string s = "<state> ";
        s += pc.str() + "\n";
        return s;
    }

    // Add constraint described by cond to this state.
    void add_constraint(tree cond)
    {
        term new_term = term(cond);

        for(auto& inner: pc.ors) {
            inner.add_constraint(new_term);
        }
    }

    // Add the given term as a constraint to this state.
    void add_constraint(term& t)
    {
        pc.add_constraint(t);
    }

    void add_constraint(term&& t) 
    {
        pc.add_constraint(t); 
    }

    void merge(state& s)
    {
        pc.ors.reserve(pc.ors.size() + s.pc.ors.size());
        
        for(auto& inner: s.pc.ors) {
            pc.ors.push_back(inner);
        }
    }

    ~state() = default;
};

//}

#endif
