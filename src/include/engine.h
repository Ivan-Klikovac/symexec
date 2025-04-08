/*  Necessary definitions to call the engine from main.cpp.

    Copyright (C) 2025 Ivan Klikovac
    This program is free software. */

#ifndef SYMEXEC_ENGINE_H
#define SYMEXEC_ENGINE_H

struct function;

void analyze_fn(function* fn);

#endif
