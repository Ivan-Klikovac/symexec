/*  SYMEXEC - a symbolic execution engine
    Copyright (C) 2025 Ivan Klikovac

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License version 3
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program. If not, see <https://www.gnu.org/licenses/>. */

#include <cstdio>
#include <unordered_map>
#include <string>

#include <engine.h>

#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "context.h"
#include "diagnostic.h"
#include "tree-pass.h"
#include "gimple-pretty-print.h"

extern "C" {

int plugin_is_GPL_compatible = 3;

const pass_data data = {
    GIMPLE_PASS,
    "test_pass",
    OPTGROUP_NONE,
    TV_NONE,
    PROP_gimple_any,
    0,
    0,
    0
};

struct test_pass: gimple_opt_pass
{
    test_pass(gcc::context* ctx): gimple_opt_pass(data, ctx) {}

    unsigned int execute(function* fn) override
    {
        printf("function %s\n", function_name(fn));
        //analyze_fn(fn);

        return 0;
    }
};

int plugin_init(struct plugin_name_args* plugin_info, struct plugin_gcc_version* version)
{
    if(!plugin_default_version_check(version, &gcc_version)) {
        printf("Expected GCC 14");
        return 1;
    }

    printf("loading %s...\n", plugin_info->base_name);

    register_pass_info info;
    info.pass = new test_pass(g);
    info.reference_pass_name = "optimized";
    info.ref_pass_instance_number = 1;
    info.pos_op = PASS_POS_INSERT_AFTER;

    register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, 0, &info);

    return 0;
}
}