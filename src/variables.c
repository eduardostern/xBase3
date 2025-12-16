/*
 * xBase3 - dBASE III+ Compatible Database System
 * variables.c - Memory variable management implementation
 */

#include "variables.h"
#include "expr.h"
#include <string.h>
#include <stdio.h>

/* Maximum variables and scopes */
#define MAX_VARIABLES   1000
#define MAX_SCOPE_DEPTH 50

/* Variable entry */
typedef struct {
    char name[MAX_FIELD_NAME];
    Value value;
    VarScope scope;
    int scope_level;
    bool in_use;
} Variable;

/* Variable storage */
static Variable g_variables[MAX_VARIABLES];
static int g_var_count = 0;
static int g_scope_level = 0;

void var_init(void) {
    memset(g_variables, 0, sizeof(g_variables));
    g_var_count = 0;
    g_scope_level = 0;
}

void var_cleanup(void) {
    for (int i = 0; i < g_var_count; i++) {
        if (g_variables[i].in_use) {
            value_free(&g_variables[i].value);
        }
    }
    g_var_count = 0;
}

/* Find variable by name */
static Variable *find_var(const char *name) {
    for (int i = 0; i < g_var_count; i++) {
        if (g_variables[i].in_use &&
            str_casecmp(g_variables[i].name, name) == 0) {
            return &g_variables[i];
        }
    }
    return NULL;
}

/* Find or create variable slot */
static Variable *get_or_create_var(const char *name) {
    Variable *var = find_var(name);
    if (var) return var;

    /* Find free slot */
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (!g_variables[i].in_use) {
            var = &g_variables[i];
            memset(var, 0, sizeof(Variable));
            strncpy(var->name, name, MAX_FIELD_NAME - 1);
            str_upper(var->name);
            var->in_use = true;
            var->scope = SCOPE_PRIVATE;
            var->scope_level = g_scope_level;
            if (i >= g_var_count) g_var_count = i + 1;
            return var;
        }
    }

    error_set(ERR_OUT_OF_MEMORY, "Too many variables");
    return NULL;
}

bool var_set(const char *name, const Value *value) {
    if (!name || !value) return false;

    Variable *var = get_or_create_var(name);
    if (!var) return false;

    /* Free old value */
    value_free(&var->value);

    /* Copy new value */
    var->value = value_copy(value);

    return true;
}

Value *var_get(const char *name) {
    Variable *var = find_var(name);
    return var ? &var->value : NULL;
}

bool var_exists(const char *name) {
    return find_var(name) != NULL;
}

bool var_declare_public(const char *name) {
    Variable *var = get_or_create_var(name);
    if (!var) return false;
    var->scope = SCOPE_PUBLIC;
    var->scope_level = 0;
    return true;
}

bool var_declare_private(const char *name) {
    Variable *var = get_or_create_var(name);
    if (!var) return false;
    var->scope = SCOPE_PRIVATE;
    var->scope_level = g_scope_level;
    return true;
}

bool var_declare_local(const char *name) {
    Variable *var = get_or_create_var(name);
    if (!var) return false;
    var->scope = SCOPE_LOCAL;
    var->scope_level = g_scope_level;
    return true;
}

bool var_declare_array(const char *name, int size) {
    if (size <= 0 || size > 65535) {
        error_set(ERR_OVERFLOW, "Invalid array size");
        return false;
    }

    Variable *var = get_or_create_var(name);
    if (!var) return false;

    /* Free old value */
    value_free(&var->value);

    /* Create array */
    var->value.type = VAL_ARRAY;
    var->value.data.array.count = size;
    var->value.data.array.elements = xcalloc((size_t)size, sizeof(Value));

    /* Initialize elements to .F. */
    for (int i = 0; i < size; i++) {
        var->value.data.array.elements[i] = value_logical(false);
    }

    return true;
}

bool var_release(const char *name) {
    Variable *var = find_var(name);
    if (!var) return false;

    value_free(&var->value);
    var->in_use = false;

    return true;
}

void var_release_all(void) {
    for (int i = 0; i < g_var_count; i++) {
        if (g_variables[i].in_use) {
            value_free(&g_variables[i].value);
            g_variables[i].in_use = false;
        }
    }
    g_var_count = 0;
}

void var_release_locals(void) {
    for (int i = 0; i < g_var_count; i++) {
        if (g_variables[i].in_use &&
            g_variables[i].scope == SCOPE_LOCAL &&
            g_variables[i].scope_level >= g_scope_level) {
            value_free(&g_variables[i].value);
            g_variables[i].in_use = false;
        }
    }
}

void var_push_scope(void) {
    if (g_scope_level < MAX_SCOPE_DEPTH - 1) {
        g_scope_level++;
    }
}

void var_pop_scope(void) {
    if (g_scope_level > 0) {
        /* Release locals at current scope */
        for (int i = 0; i < g_var_count; i++) {
            if (g_variables[i].in_use &&
                g_variables[i].scope == SCOPE_LOCAL &&
                g_variables[i].scope_level == g_scope_level) {
                value_free(&g_variables[i].value);
                g_variables[i].in_use = false;
            }
        }
        g_scope_level--;
    }
}

void var_list(void) {
    printf("Memory Variables:\n");
    for (int i = 0; i < g_var_count; i++) {
        if (g_variables[i].in_use) {
            char buf[256];
            value_to_string(&g_variables[i].value, buf, sizeof(buf));
            const char *scope_name = "PUBLIC";
            if (g_variables[i].scope == SCOPE_PRIVATE) scope_name = "PRIVATE";
            if (g_variables[i].scope == SCOPE_LOCAL) scope_name = "LOCAL";
            printf("  %-10s = %-20s (%s)\n",
                   g_variables[i].name, buf, scope_name);
        }
    }
}
