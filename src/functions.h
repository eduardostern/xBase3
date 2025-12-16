/*
 * xBase3 - dBASE III+ Compatible Database System
 * functions.h - Built-in functions header
 */

#ifndef XBASE3_FUNCTIONS_H
#define XBASE3_FUNCTIONS_H

#include "expr.h"

/* Call built-in function by name */
Value func_call(const char *name, Value *args, int arg_count, EvalContext *ctx);

/* Check if function exists */
bool func_exists(const char *name);

#endif /* XBASE3_FUNCTIONS_H */
