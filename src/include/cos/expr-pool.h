/*  Definitions for the expr_pool structure.

    Copyright (C) 2025 Ivan Klikovac
    This program is free software. */

#ifndef SYMEXEC_COS_EXPRPOOL_H
#define SYMEXEC_COS_EXPRPOOL_H

#include <cos/cos.h>

#include <unordered_map>

// This is used to avoid duplicate expressions.
struct expr_pool
{
    std::unordered_map<string, expr*> pool;

    expr* intern(const value& l, tree_code o, const value& r)
    {
        string key = l.str() + std::to_string(o) + r.str();
        
        auto it = pool.find(key);
        if(it != pool.end()) return it->second;

        expr* e = (expr*) xmalloc(sizeof(expr));
        e->lhs = l;
        e->op = o;
        e->rhs = r;
        pool[key] = e;

        return e;
    }
};

static expr_pool global_expr_pool;

#endif