/*
 * xBase3 - dBASE III+ Compatible Database System
 * variables.h - Memory variable management header
 */

#ifndef XBASE3_VARIABLES_H
#define XBASE3_VARIABLES_H

#include "util.h"

/* Forward declaration */
typedef struct Value Value;

/* Variable scope */
typedef enum {
    SCOPE_PUBLIC,
    SCOPE_PRIVATE,
    SCOPE_LOCAL
} VarScope;

/* Initialize variable system */
void var_init(void);

/* Cleanup variable system */
void var_cleanup(void);

/* Set/get variables */
bool var_set(const char *name, const Value *value);
Value *var_get(const char *name);
bool var_exists(const char *name);

/* Declare variables */
bool var_declare_public(const char *name);
bool var_declare_private(const char *name);
bool var_declare_local(const char *name);

/* Declare array */
bool var_declare_array(const char *name, int size);

/* Release variables */
bool var_release(const char *name);
void var_release_all(void);
void var_release_locals(void);

/* Push/pop scope (for procedure calls) */
void var_push_scope(void);
void var_pop_scope(void);

/* List variables (for debugging) */
void var_list(void);

#endif /* XBASE3_VARIABLES_H */
