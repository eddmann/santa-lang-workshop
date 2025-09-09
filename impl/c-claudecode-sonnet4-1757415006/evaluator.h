#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "parser.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    VAL_INTEGER,
    VAL_DECIMAL,
    VAL_STRING,
    VAL_BOOLEAN,
    VAL_NIL,
    VAL_FUNCTION,
    VAL_BUILTIN_FUNCTION,
    VAL_PARTIAL_FUNCTION,
    VAL_LIST,
    VAL_SET,
    VAL_DICT,
    VAL_ERROR
} ValueType;

typedef struct Value Value;
typedef struct ValueList ValueList;
typedef struct Environment Environment;

struct ValueList {
    Value **values;
    size_t count;
    size_t capacity;
};

struct Value {
    ValueType type;
    union {
        struct {
            int64_t value;
        } integer;
        struct {
            double value;
        } decimal;
        struct {
            char *value;
        } string;
        struct {
            bool value;
        } boolean;
        struct {
            char *message;
        } error;
        struct {
            ASTNodeList *params;
            ASTNode *body;
            Environment *closure_env;
        } function;
        struct {
            char *name;
        } builtin_function;
        struct {
            Value *original_function;
            ValueList *partial_args;
        } partial_function;
        struct {
            ValueList *elements;
        } list;
        struct {
            ValueList *elements;
        } set;
        struct {
            ValueList *keys;
            ValueList *values;
        } dict;
    } data;
};

typedef struct EnvEntry EnvEntry;
struct EnvEntry {
    char *name;
    Value *value;
    bool is_mutable;
    EnvEntry *next;
};

struct Environment {
    EnvEntry *entries;
    Environment *parent;
};

typedef struct {
    Environment *global_env;
    Environment *current_env;
} Evaluator;

// Value operations
Value *value_create_integer(int64_t val);
Value *value_create_decimal(double val);
Value *value_create_string(const char *val);
Value *value_create_boolean(bool val);
Value *value_create_nil(void);
Value *value_create_error(const char *message);
Value *value_create_list(ValueList *elements);
Value *value_create_set(ValueList *elements);
Value *value_create_dict(ValueList *keys, ValueList *values);
Value *value_create_function(ASTNodeList *params, ASTNode *body, Environment *closure_env);
Value *value_create_builtin_function(const char *name);
void value_free(Value *val);
Value *value_clone(Value *val);
void print_value(Value *val);
bool is_truthy(Value *val);

// Helper functions for collections
void print_list(ValueList *list);
void print_set(ValueList *set);
void print_dict(ValueList *keys, ValueList *values);
int value_compare(Value *a, Value *b);
bool values_equal(Value *a, Value *b);
Value *evaluate_index(Value *target, Value *index);

// Built-in functions
Value *builtin_push(Value *element, Value *collection);
Value *builtin_first(Value *collection);
Value *builtin_rest(Value *collection);
Value *builtin_size(Value *collection);
Value *builtin_assoc(Value *key, Value *value, Value *dict);
Value *builtin_map(Evaluator *eval, Value *function, Value *collection);
Value *builtin_filter(Evaluator *eval, Value *function, Value *collection);
Value *builtin_fold(Evaluator *eval, Value *function, Value *initial, Value *collection);

// ValueList operations
ValueList *value_list_create(void);
void value_list_free(ValueList *list);
void value_list_append(ValueList *list, Value *val);

// Environment operations
Environment *env_create(Environment *parent);
Environment *env_clone(Environment *env);
void env_free(Environment *env);
void env_define(Environment *env, const char *name, Value *val, bool is_mutable);
Value *env_get(Environment *env, const char *name);
bool env_assign(Environment *env, const char *name, Value *val);

// Evaluator operations
Evaluator *evaluator_create(void);
void evaluator_free(Evaluator *eval);
Value *evaluate(Evaluator *eval, ASTNode *node);
Value *evaluate_unary(Evaluator *eval, ASTNode *node);
bool value_references_environment(Value *value, Environment *env);
Value *call_function(Evaluator *eval, Value *function, ValueList *args);
Value *create_partial_function(Value *function, ValueList *partial_args);
Value *evaluate_binary_operator(Evaluator *eval, const char *op, Value *left, Value *right);
void register_builtin_functions(Evaluator *eval);

#endif