/*  The main engine logic.

    Copyright (C) 2025 Ivan Klikovac
    This program is free software. */

#include <cos/cos.h>

#include <set>
#include <unordered_map>

#include "basic-block.h"
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "pretty-print.h"
#include "tree-core.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "context.h"
#include "diagnostic.h"
#include "tree-pass.h"
#include "gimple-pretty-print.h"

extern "C" {

function* current_fn;

// The possible symbolic values that exist in a given basic block.
std::unordered_map<basic_block, state> states = {};

// States that are yet unexplored.
std::vector<state> pending_states;

void add_branch(const state& s1, const state& s2)
{
    // feed into the engine
}

// Decide which state is to be explored next.
void sched()
{
    //pending_states.back();
}

// Recursively dive into `e` and store the dependencies of `sym` in `deps`.
// This code currently is, and likely will remain, unused.
void find_deps_recursive(std::set<symbolic>& deps, symbolic& sym, state& s, expr* e)
{
    if(!e) return;

    if(e->lhs.is_symbolic() && e->lhs.get_symbolic() == sym && e->rhs.is_symbolic())
        deps.insert(e->rhs.get_symbolic());

    if(e->rhs.is_symbolic() && e->rhs.get_symbolic() == sym && e->lhs.is_symbolic())
        deps.insert(e->lhs.get_symbolic());

    if(e->lhs.is_expr()) 
        find_deps_recursive(deps, sym, s, std::get<expr*>(e->lhs.content));

    if(e->rhs.is_expr())
        find_deps_recursive(deps, sym, s, std::get<expr*>(e->rhs.content));
}

// Return the set of symbols involved in the value of the given symbol.
std::set<symbolic> find_dependencies(symbolic& sym, state& s)
{
    std::set<symbolic> deps;

    for(const auto& inner: s.pc.ors) {
        for(const auto& term: inner.ands) {
            if(term.lhs.is_symbolic() && term.lhs.get_symbolic() == sym) {
                if(term.rhs.is_symbolic()) deps.insert(term.rhs.get_symbolic());
                if(term.rhs.is_expr()) 
                    find_deps_recursive(deps, sym, s, std::get<expr*>(term.rhs.content));
            }
            if(term.rhs.is_symbolic() && term.rhs.get_symbolic() == sym) {
                if(term.lhs.is_symbolic()) deps.insert(term.rhs.get_symbolic());
                if(term.rhs.is_expr())
                    find_deps_recursive(deps, sym, s, std::get<expr*>(term.rhs.content));
            }
        }
    }

    return deps;
}

void process_arithmetic(tree lhs, tree_code op, tree rhs1, tree rhs2, state& new_state)
{
    value lhs_val(lhs);
    value rhs1_val = term::from_tree(rhs1);
    value rhs2_val = term::from_tree(rhs2);

    switch(op) {
        case PLUS_EXPR: {
            // lhs = rhs1 + rhs2
            expr* sum = (expr*) xmalloc(sizeof(expr));
            sum->lhs = rhs1_val;
            sum->op = PLUS_EXPR;
            sum->rhs = rhs2_val;
            term eq_term = {lhs_val, EQ_EXPR, sum};
            new_state.add_constraint(eq_term);
        }
        break;

        case MINUS_EXPR: {
            // lhs = rhs1 - rhs2;
            expr* diff = (expr*) xmalloc(sizeof(expr));
            diff->lhs = rhs1_val;
            diff->op = MINUS_EXPR;
            diff->rhs = rhs2_val;
            term eq_term = {lhs_val, EQ_EXPR, diff};
            new_state.add_constraint(eq_term);
        }
        break;

        case MULT_EXPR: {
            // lhs = rhs1 * rhs2
            expr* prod = new expr(rhs1_val, MULT_EXPR, rhs2_val);
            printf("allocated %p\n", prod);
            term eq_term = {lhs_val, EQ_EXPR, prod};
            new_state.add_constraint(eq_term);
        }
        break;

        default: break;
    }
}

void process_assign(gassign* assign, state& s)
{
    tree lhs = gimple_assign_lhs(assign);
    if(TREE_CODE(lhs) != SSA_NAME) return;

    tree_code op = gimple_assign_rhs_code(assign);

    switch(op) {
        case INTEGER_CST:
        case REAL_CST: {
            // direct assignment of a constant
            tree rhs = gimple_assign_rhs1(assign);
            value lhs_val(lhs);
            value rhs_val = term::from_tree(rhs);
            term eq_term(lhs_val, EQ_EXPR, rhs_val);
            s.add_constraint(eq_term);

            break;
        }

        case SSA_NAME: {
            // copying of variables
            tree rhs = gimple_assign_rhs1(assign);
            value lhs_val(lhs);
            value rhs_val(rhs);
            term eq_term(lhs_val, EQ_EXPR, rhs_val);
            s.add_constraint(eq_term);
            break;
        }

        case PLUS_EXPR:
        case MINUS_EXPR:
        case MULT_EXPR: {
            tree rhs1 = gimple_assign_rhs1(assign);
            tree rhs2 = gimple_assign_rhs2(assign);
            process_arithmetic(lhs, op, rhs1, rhs2, s);
            break;
        }

        default: break;
    }
}

void process_cond(gcond* cond, basic_block bb, state& s)
{
    tree lhs = gimple_cond_lhs(cond);
    tree rhs = gimple_cond_rhs(cond);
    tree_code op = gimple_cond_code(cond);

    term condition(lhs, op, rhs);

    // leaving this basic block, update its state
    states[bb] = s;

    // todo: add satisfiability checks and only branch if unknown
    
    // branch into two states
    // one if the condition is true (the same state can be reused), // can it?
    // and one it it's false
    
    state if_true = s;
    state if_false = s;

    if_true.add_constraint(condition);
    if_false.add_constraint(!condition);

    // now go through the cfg to find which bbs to branch into
    // the branches from the gcond are in this block's successors

    basic_block bb_if_true = nullptr;
    basic_block bb_if_false = nullptr;

    edge e;
    edge_iterator ei;
    FOR_EACH_EDGE(e, ei, bb->succs) {
        if(e->flags & EDGE_TRUE_VALUE) bb_if_true = e->dest;
        if(e->flags & EDGE_FALSE_VALUE) bb_if_false = e->dest;
    }

    if(bb_if_true) states[bb_if_true] = if_true;
    if(bb_if_false) states[bb_if_false] = if_false;

    pending_states.push_back(if_true);
    pending_states.push_back(if_false);
    sched();
}

void analyze_stmt(basic_block bb, gimple* stmt, state& s)
{
    switch(gimple_code(stmt)) {
        case GIMPLE_ASSIGN: process_assign(as_a<gassign*>(stmt), s); break;
        case GIMPLE_COND:   process_cond(as_a<gcond*>(stmt), bb, s); break;
        default: break;
    }
}

void analyze_bb(basic_block bb)
{
    if(bb == EXIT_BLOCK_PTR_FOR_FN(current_fn) || bb == ENTRY_BLOCK_PTR_FOR_FN(current_fn)) return;

    state s;

    gimple_stmt_iterator gsi;
    for(gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
        gimple* stmt = gsi_stmt(gsi);
        analyze_stmt(bb, stmt, s);
    }

    //states[bb] = s;
}

void analyze_fn()
{
    basic_block bb;
    
    FOR_EACH_BB_FN(bb, current_fn) {
        analyze_bb(bb);
    }

    for(const auto& pair: states) {
        printf("<%p> %s\n", pair.first, pair.second.pc.str().c_str());
    }

    if(!pending_states.empty()) {
        state& s = pending_states.back();
        pending_states.pop_back();
    }
}

}