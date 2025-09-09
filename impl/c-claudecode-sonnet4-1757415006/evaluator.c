#include "evaluator.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>

// Value operations
Value *value_create_integer(int64_t val) {
    Value *value = malloc(sizeof(Value));
    value->type = VAL_INTEGER;
    value->data.integer.value = val;
    return value;
}

Value *value_create_decimal(double val) {
    Value *value = malloc(sizeof(Value));
    value->type = VAL_DECIMAL;
    value->data.decimal.value = val;
    return value;
}

Value *value_create_string(const char *val) {
    Value *value = malloc(sizeof(Value));
    value->type = VAL_STRING;
    value->data.string.value = malloc(strlen(val) + 1);
    strcpy(value->data.string.value, val);
    return value;
}

Value *value_create_boolean(bool val) {
    Value *value = malloc(sizeof(Value));
    value->type = VAL_BOOLEAN;
    value->data.boolean.value = val;
    return value;
}

Value *value_create_nil(void) {
    Value *value = malloc(sizeof(Value));
    value->type = VAL_NIL;
    return value;
}

Value *value_create_error(const char *message) {
    Value *value = malloc(sizeof(Value));
    value->type = VAL_ERROR;
    value->data.error.message = malloc(strlen(message) + 1);
    strcpy(value->data.error.message, message);
    return value;
}

Value *value_create_list(ValueList *elements) {
    Value *value = malloc(sizeof(Value));
    value->type = VAL_LIST;
    value->data.list.elements = elements;
    return value;
}

Value *value_create_set(ValueList *elements) {
    Value *value = malloc(sizeof(Value));
    value->type = VAL_SET;
    value->data.set.elements = elements;
    return value;
}

Value *value_create_dict(ValueList *keys, ValueList *values) {
    Value *value = malloc(sizeof(Value));
    value->type = VAL_DICT;
    value->data.dict.keys = keys;
    value->data.dict.values = values;
    return value;
}

Value *value_create_function(ASTNodeList *params, ASTNode *body, Environment *closure_env) {
    Value *value = malloc(sizeof(Value));
    value->type = VAL_FUNCTION;
    value->data.function.params = params;
    value->data.function.body = body;
    value->data.function.closure_env = closure_env;
    return value;
}

Value *value_create_builtin_function(const char *name) {
    Value *value = malloc(sizeof(Value));
    value->type = VAL_BUILTIN_FUNCTION;
    value->data.builtin_function.name = malloc(strlen(name) + 1);
    strcpy(value->data.builtin_function.name, name);
    return value;
}

Value *value_create_partial_function(Value *original_function, ValueList *partial_args) {
    Value *value = malloc(sizeof(Value));
    value->type = VAL_PARTIAL_FUNCTION;
    value->data.partial_function.original_function = original_function;
    value->data.partial_function.partial_args = partial_args;
    return value;
}

void value_free(Value *val) {
    if (!val) return;
    
    switch (val->type) {
        case VAL_STRING:
            free(val->data.string.value);
            break;
        case VAL_ERROR:
            free(val->data.error.message);
            break;
        case VAL_LIST:
            value_list_free(val->data.list.elements);
            break;
        case VAL_SET:
            value_list_free(val->data.set.elements);
            break;
        case VAL_DICT:
            value_list_free(val->data.dict.keys);
            value_list_free(val->data.dict.values);
            break;
        case VAL_FUNCTION:
            // Don't free closure_env here since it might be shared (e.g., global environment)
            // Environment cleanup is handled by the conditional preservation logic
            break;
        case VAL_BUILTIN_FUNCTION:
            free(val->data.builtin_function.name);
            break;
        case VAL_PARTIAL_FUNCTION:
            value_free(val->data.partial_function.original_function);
            value_list_free(val->data.partial_function.partial_args);
            break;
        default:
            break;
    }
    free(val);
}

Value *value_clone(Value *val) {
    if (!val) return NULL;
    
    switch (val->type) {
        case VAL_INTEGER:
            return value_create_integer(val->data.integer.value);
        case VAL_DECIMAL:
            return value_create_decimal(val->data.decimal.value);
        case VAL_STRING:
            return value_create_string(val->data.string.value);
        case VAL_BOOLEAN:
            return value_create_boolean(val->data.boolean.value);
        case VAL_NIL:
            return value_create_nil();
        case VAL_ERROR:
            return value_create_error(val->data.error.message);
        case VAL_LIST: {
            ValueList *orig_list = val->data.list.elements;
            ValueList *new_list = value_list_create();
            for (size_t i = 0; i < orig_list->count; i++) {
                value_list_append(new_list, value_clone(orig_list->values[i]));
            }
            return value_create_list(new_list);
        }
        case VAL_SET: {
            ValueList *orig_set = val->data.set.elements;
            ValueList *new_set = value_list_create();
            for (size_t i = 0; i < orig_set->count; i++) {
                value_list_append(new_set, value_clone(orig_set->values[i]));
            }
            return value_create_set(new_set);
        }
        case VAL_DICT: {
            ValueList *orig_keys = val->data.dict.keys;
            ValueList *orig_vals = val->data.dict.values;
            ValueList *new_keys = value_list_create();
            ValueList *new_vals = value_list_create();
            for (size_t i = 0; i < orig_keys->count; i++) {
                value_list_append(new_keys, value_clone(orig_keys->values[i]));
                value_list_append(new_vals, value_clone(orig_vals->values[i]));
            }
            return value_create_dict(new_keys, new_vals);
        }
        case VAL_FUNCTION: {
            // Simple function clone - share closure env to avoid complex memory management
            return value_create_function(val->data.function.params, 
                                        val->data.function.body,
                                        val->data.function.closure_env);
        }
        case VAL_BUILTIN_FUNCTION: {
            return value_create_builtin_function(val->data.builtin_function.name);
        }
        case VAL_PARTIAL_FUNCTION: {
            ValueList *cloned_args = value_list_create();
            for (size_t i = 0; i < val->data.partial_function.partial_args->count; i++) {
                value_list_append(cloned_args, value_clone(val->data.partial_function.partial_args->values[i]));
            }
            return value_create_partial_function(value_clone(val->data.partial_function.original_function), cloned_args);
        }
        default:
            return value_create_error("Cannot clone complex value");
    }
}

bool is_truthy(Value *val) {
    switch (val->type) {
        case VAL_NIL:
            return false;
        case VAL_BOOLEAN:
            return val->data.boolean.value;
        case VAL_INTEGER:
            return val->data.integer.value != 0;
        case VAL_DECIMAL:
            return val->data.decimal.value != 0.0;
        case VAL_STRING:
            return strlen(val->data.string.value) > 0;
        default:
            return true;
    }
}

void print_value(Value *val) {
    switch (val->type) {
        case VAL_INTEGER:
            printf("%lld", (long long)val->data.integer.value);
            break;
        case VAL_DECIMAL: {
            double d = val->data.decimal.value;
            if (d == floor(d)) {
                printf("%.0f", d);
            } else {
                // Try shorter format first, fall back to full precision if needed
                char buf[64];
                snprintf(buf, sizeof(buf), "%g", d);
                double parsed = strtod(buf, NULL);
                if (parsed == d) {
                    printf("%s", buf);
                } else {
                    printf("%.15f", d);
                }
            }
            break;
        }
        case VAL_STRING:
            printf("\"%s\"", val->data.string.value);
            break;
        case VAL_BOOLEAN:
            printf("%s", val->data.boolean.value ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_ERROR:
            printf("[Error] %s", val->data.error.message);
            break;
        case VAL_LIST:
            print_list(val->data.list.elements);
            break;
        case VAL_SET:
            print_set(val->data.set.elements);
            break;
        case VAL_DICT:
            print_dict(val->data.dict.keys, val->data.dict.values);
            break;
        case VAL_FUNCTION:
            printf("Function");
            break;
        case VAL_BUILTIN_FUNCTION:
            printf("BuiltinFunction(%s)", val->data.builtin_function.name);
            break;
        default:
            printf("UnknownValue");
            break;
    }
}

// ValueList operations
ValueList *value_list_create(void) {
    ValueList *list = malloc(sizeof(ValueList));
    list->values = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

void value_list_free(ValueList *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        value_free(list->values[i]);
    }
    free(list->values);
    free(list);
}

void value_list_append(ValueList *list, Value *val) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->values = realloc(list->values, list->capacity * sizeof(Value*));
    }
    list->values[list->count++] = val;
}

// Environment operations
Environment *env_create(Environment *parent) {
    Environment *env = malloc(sizeof(Environment));
    env->entries = NULL;
    env->parent = parent;
    return env;
}

// Create a deep copy of an environment chain for closures
Environment *env_clone(Environment *env) {
    if (!env) return NULL;
    
    Environment *clone = malloc(sizeof(Environment));
    clone->entries = NULL;
    clone->parent = env_clone(env->parent);  // Recursively clone parent environments
    
    // Clone all entries in this environment
    for (EnvEntry *entry = env->entries; entry; entry = entry->next) {
        EnvEntry *new_entry = malloc(sizeof(EnvEntry));
        new_entry->name = malloc(strlen(entry->name) + 1);
        strcpy(new_entry->name, entry->name);
        new_entry->value = value_clone(entry->value);  // Deep copy the value
        new_entry->is_mutable = entry->is_mutable;
        new_entry->next = clone->entries;
        clone->entries = new_entry;
    }
    
    return clone;
}

void env_free(Environment *env) {
    if (!env) return;
    
    EnvEntry *entry = env->entries;
    while (entry) {
        EnvEntry *next = entry->next;
        free(entry->name);
        value_free(entry->value);
        free(entry);
        entry = next;
    }
    free(env);
}

void env_define(Environment *env, const char *name, Value *val, bool is_mutable) {
    EnvEntry *entry = malloc(sizeof(EnvEntry));
    entry->name = malloc(strlen(name) + 1);
    strcpy(entry->name, name);
    entry->value = val;
    entry->is_mutable = is_mutable;
    entry->next = env->entries;
    env->entries = entry;
}

Value *env_get(Environment *env, const char *name) {
    for (EnvEntry *entry = env->entries; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            return entry->value;
        }
    }
    
    if (env->parent) {
        return env_get(env->parent, name);
    }
    
    return NULL;
}

bool env_assign(Environment *env, const char *name, Value *val) {
    for (EnvEntry *entry = env->entries; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            if (!entry->is_mutable) {
                return false;
            }
            value_free(entry->value);
            entry->value = value_clone(val);
            return true;
        }
    }
    
    if (env->parent) {
        return env_assign(env->parent, name, val);
    }
    
    return false;
}

void env_set(Environment *env, const char *name, Value *val) {
    for (EnvEntry *entry = env->entries; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            value_free(entry->value);
            entry->value = val; // Note: takes ownership, no clone needed
            return;
        }
    }
    
    if (env->parent) {
        env_set(env->parent, name, val);
    }
}

// Built-in functions
Value *builtin_puts(Evaluator *eval, ValueList *args) {
    (void)eval; // Unused parameter
    
    for (size_t i = 0; i < args->count; i++) {
        print_value(args->values[i]);
        if (i < args->count - 1) {
            printf(" ");
        }
    }
    printf(" \n");
    
    return value_create_nil();
}

Value *builtin_push(Value *element, Value *collection) {
    if (collection->type == VAL_LIST) {
        ValueList *orig_list = collection->data.list.elements;
        ValueList *new_list = value_list_create();
        
        // Copy all existing elements
        for (size_t i = 0; i < orig_list->count; i++) {
            value_list_append(new_list, value_clone(orig_list->values[i]));
        }
        // Add new element
        value_list_append(new_list, value_clone(element));
        
        return value_create_list(new_list);
    } else if (collection->type == VAL_SET) {
        ValueList *orig_set = collection->data.set.elements;
        ValueList *new_set = value_list_create();
        
        // Copy all existing elements
        for (size_t i = 0; i < orig_set->count; i++) {
            value_list_append(new_set, value_clone(orig_set->values[i]));
        }
        
        // Add new element only if it doesn't already exist
        bool found = false;
        for (size_t i = 0; i < new_set->count; i++) {
            if (values_equal(element, new_set->values[i])) {
                found = true;
                break;
            }
        }
        if (!found) {
            value_list_append(new_set, value_clone(element));
        }
        
        return value_create_set(new_set);
    } else {
        return value_create_error("push can only be used with List or Set");
    }
}

Value *builtin_first(Value *collection) {
    if (collection->type == VAL_LIST) {
        ValueList *list = collection->data.list.elements;
        if (list->count == 0) {
            return value_create_nil();
        }
        return value_clone(list->values[0]);
    } else if (collection->type == VAL_STRING) {
        const char *str = collection->data.string.value;
        if (strlen(str) == 0) {
            return value_create_nil();
        }
        char first[2] = {str[0], '\0'};
        return value_create_string(first);
    } else if (collection->type == VAL_SET) {
        ValueList *set = collection->data.set.elements;
        if (set->count == 0) {
            return value_create_nil();
        }
        return value_clone(set->values[0]);
    } else {
        return value_create_nil();
    }
}

Value *builtin_rest(Value *collection) {
    if (collection->type == VAL_LIST) {
        ValueList *orig_list = collection->data.list.elements;
        ValueList *new_list = value_list_create();
        
        // Copy all elements except the first
        for (size_t i = 1; i < orig_list->count; i++) {
            value_list_append(new_list, value_clone(orig_list->values[i]));
        }
        
        return value_create_list(new_list);
    } else if (collection->type == VAL_STRING) {
        const char *str = collection->data.string.value;
        size_t len = strlen(str);
        if (len <= 1) {
            return value_create_string("");
        }
        return value_create_string(str + 1);
    } else if (collection->type == VAL_SET) {
        ValueList *orig_set = collection->data.set.elements;
        ValueList *new_set = value_list_create();
        
        // Copy all elements except the first
        for (size_t i = 1; i < orig_set->count; i++) {
            value_list_append(new_set, value_clone(orig_set->values[i]));
        }
        
        return value_create_set(new_set);
    } else {
        return value_create_nil();
    }
}

Value *builtin_size(Value *collection) {
    if (collection->type == VAL_LIST) {
        return value_create_integer((int64_t)collection->data.list.elements->count);
    } else if (collection->type == VAL_STRING) {
        return value_create_integer((int64_t)strlen(collection->data.string.value));
    } else if (collection->type == VAL_SET) {
        return value_create_integer((int64_t)collection->data.set.elements->count);
    } else if (collection->type == VAL_DICT) {
        return value_create_integer((int64_t)collection->data.dict.keys->count);
    } else {
        return value_create_nil();
    }
}

Value *builtin_assoc(Value *key, Value *value, Value *dict) {
    if (dict->type != VAL_DICT) {
        return value_create_error("assoc can only be used with Dictionary");
    }
    
    ValueList *orig_keys = dict->data.dict.keys;
    ValueList *orig_vals = dict->data.dict.values;
    ValueList *new_keys = value_list_create();
    ValueList *new_vals = value_list_create();
    
    bool found = false;
    
    // Copy all existing entries, updating if key already exists
    for (size_t i = 0; i < orig_keys->count; i++) {
        if (values_equal(key, orig_keys->values[i])) {
            // Update existing key with new value
            value_list_append(new_keys, value_clone(orig_keys->values[i]));
            value_list_append(new_vals, value_clone(value));
            found = true;
        } else {
            // Keep existing entry
            value_list_append(new_keys, value_clone(orig_keys->values[i]));
            value_list_append(new_vals, value_clone(orig_vals->values[i]));
        }
    }
    
    // If key wasn't found, add it as new entry
    if (!found) {
        value_list_append(new_keys, value_clone(key));
        value_list_append(new_vals, value_clone(value));
    }
    
    return value_create_dict(new_keys, new_vals);
}

// Evaluator operations
Evaluator *evaluator_create(void) {
    Evaluator *eval = malloc(sizeof(Evaluator));
    eval->global_env = env_create(NULL);
    eval->current_env = eval->global_env;
    return eval;
}

void evaluator_free(Evaluator *eval) {
    if (!eval) return;
    env_free(eval->global_env);
    free(eval);
}

void register_builtin_functions(Evaluator *eval) {
    // Register puts as a placeholder
    env_define(eval->global_env, "puts", value_create_nil(), false);
    
    // Register operator functions as builtin functions
    env_define(eval->global_env, "+", value_create_builtin_function("+"), false);
    env_define(eval->global_env, "-", value_create_builtin_function("-"), false);
    env_define(eval->global_env, "*", value_create_builtin_function("*"), false);
    env_define(eval->global_env, "/", value_create_builtin_function("/"), false);
    env_define(eval->global_env, ">", value_create_builtin_function(">"), false);
    env_define(eval->global_env, "<", value_create_builtin_function("<"), false);
    env_define(eval->global_env, ">=", value_create_builtin_function(">="), false);
    env_define(eval->global_env, "<=", value_create_builtin_function("<="), false);
    env_define(eval->global_env, "==", value_create_builtin_function("=="), false);
    env_define(eval->global_env, "!=", value_create_builtin_function("!="), false);
    env_define(eval->global_env, "push", value_create_builtin_function("push"), false);
    env_define(eval->global_env, "fold", value_create_builtin_function("fold"), false);
    env_define(eval->global_env, "map", value_create_builtin_function("map"), false);
    env_define(eval->global_env, "filter", value_create_builtin_function("filter"), false);
    env_define(eval->global_env, "size", value_create_builtin_function("size"), false);
    env_define(eval->global_env, "assoc", value_create_builtin_function("assoc"), false);
}

Value *evaluate_infix(Evaluator *eval, ASTNode *node) {
    Value *left = evaluate(eval, node->data.infix.left);
    if (left->type == VAL_ERROR) return left;
    
    const char *op = node->data.infix.operator;
    
    // Handle short-circuiting logical operators
    if (strcmp(op, "&&") == 0) {
        if (!is_truthy(left)) {
            value_free(left);
            return value_create_boolean(false);
        }
        Value *right = evaluate(eval, node->data.infix.right);
        bool result = is_truthy(right);
        value_free(left);
        value_free(right);
        return value_create_boolean(result);
    }
    
    if (strcmp(op, "||") == 0) {
        if (is_truthy(left)) {
            value_free(left);
            return value_create_boolean(true);
        }
        Value *right = evaluate(eval, node->data.infix.right);
        bool result = is_truthy(right);
        value_free(left);
        value_free(right);
        return value_create_boolean(result);
    }
    
    Value *right = evaluate(eval, node->data.infix.right);
    if (right->type == VAL_ERROR) {
        value_free(left);
        return right;
    }
    
    // Arithmetic operations
    if (strcmp(op, "+") == 0) {
        if (left->type == VAL_INTEGER && right->type == VAL_INTEGER) {
            int64_t result = left->data.integer.value + right->data.integer.value;
            value_free(left);
            value_free(right);
            return value_create_integer(result);
        } else if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
                   (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double l_val = left->type == VAL_INTEGER ? (double)left->data.integer.value : left->data.decimal.value;
            double r_val = right->type == VAL_INTEGER ? (double)right->data.integer.value : right->data.decimal.value;
            double result = l_val + r_val;
            value_free(left);
            value_free(right);
            return value_create_decimal(result);
        } else if (left->type == VAL_STRING && right->type == VAL_STRING) {
            size_t left_len = strlen(left->data.string.value);
            size_t right_len = strlen(right->data.string.value);
            char *result = malloc(left_len + right_len + 1);
            strcpy(result, left->data.string.value);
            strcat(result, right->data.string.value);
            value_free(left);
            value_free(right);
            Value *val = value_create_string(result);
            free(result);
            return val;
        } else if (left->type == VAL_STRING && right->type == VAL_INTEGER) {
            char num_str[32];
            snprintf(num_str, sizeof(num_str), "%" PRId64, right->data.integer.value);
            size_t left_len = strlen(left->data.string.value);
            size_t num_len = strlen(num_str);
            char *result = malloc(left_len + num_len + 1);
            strcpy(result, left->data.string.value);
            strcat(result, num_str);
            value_free(left);
            value_free(right);
            Value *val = value_create_string(result);
            free(result);
            return val;
        } else if (left->type == VAL_STRING && right->type == VAL_DECIMAL) {
            char num_str[64];
            snprintf(num_str, sizeof(num_str), "%.15g", right->data.decimal.value);
            size_t left_len = strlen(left->data.string.value);
            size_t num_len = strlen(num_str);
            char *result = malloc(left_len + num_len + 1);
            strcpy(result, left->data.string.value);
            strcat(result, num_str);
            value_free(left);
            value_free(right);
            Value *val = value_create_string(result);
            free(result);
            return val;
        } else if (left->type == VAL_INTEGER && right->type == VAL_STRING) {
            char num_str[32];
            snprintf(num_str, sizeof(num_str), "%" PRId64, left->data.integer.value);
            size_t num_len = strlen(num_str);
            size_t right_len = strlen(right->data.string.value);
            char *result = malloc(num_len + right_len + 1);
            strcpy(result, num_str);
            strcat(result, right->data.string.value);
            value_free(left);
            value_free(right);
            Value *val = value_create_string(result);
            free(result);
            return val;
        } else if (left->type == VAL_DECIMAL && right->type == VAL_STRING) {
            char num_str[64];
            snprintf(num_str, sizeof(num_str), "%.15g", left->data.decimal.value);
            size_t num_len = strlen(num_str);
            size_t right_len = strlen(right->data.string.value);
            char *result = malloc(num_len + right_len + 1);
            strcpy(result, num_str);
            strcat(result, right->data.string.value);
            value_free(left);
            value_free(right);
            Value *val = value_create_string(result);
            free(result);
            return val;
        } else if (left->type == VAL_LIST && right->type == VAL_LIST) {
            // List concatenation
            ValueList *left_list = left->data.list.elements;
            ValueList *right_list = right->data.list.elements;
            ValueList *result = value_list_create();
            
            // Add all elements from left list
            for (size_t i = 0; i < left_list->count; i++) {
                value_list_append(result, value_clone(left_list->values[i]));
            }
            // Add all elements from right list  
            for (size_t i = 0; i < right_list->count; i++) {
                value_list_append(result, value_clone(right_list->values[i]));
            }
            
            value_free(left);
            value_free(right);
            return value_create_list(result);
        } else if (left->type == VAL_SET && right->type == VAL_SET) {
            // Set union
            ValueList *left_set = left->data.set.elements;
            ValueList *right_set = right->data.set.elements;
            ValueList *result = value_list_create();
            
            // Add all elements from left set
            for (size_t i = 0; i < left_set->count; i++) {
                value_list_append(result, value_clone(left_set->values[i]));
            }
            // Add elements from right set that aren't already in result
            for (size_t i = 0; i < right_set->count; i++) {
                bool found = false;
                for (size_t j = 0; j < result->count; j++) {
                    if (values_equal(right_set->values[i], result->values[j])) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    value_list_append(result, value_clone(right_set->values[i]));
                }
            }
            
            value_free(left);
            value_free(right);
            return value_create_set(result);
        } else if (left->type == VAL_DICT && right->type == VAL_DICT) {
            // Dictionary merge (right-biased)
            ValueList *left_keys = left->data.dict.keys;
            ValueList *left_vals = left->data.dict.values;
            ValueList *right_keys = right->data.dict.keys;
            ValueList *right_vals = right->data.dict.values;
            
            ValueList *result_keys = value_list_create();
            ValueList *result_vals = value_list_create();
            
            // Add all entries from left dict
            for (size_t i = 0; i < left_keys->count; i++) {
                value_list_append(result_keys, value_clone(left_keys->values[i]));
                value_list_append(result_vals, value_clone(left_vals->values[i]));
            }
            
            // Add entries from right dict, overriding if key exists
            for (size_t i = 0; i < right_keys->count; i++) {
                bool found = false;
                for (size_t j = 0; j < result_keys->count; j++) {
                    if (values_equal(right_keys->values[i], result_keys->values[j])) {
                        // Override value
                        value_free(result_vals->values[j]);
                        result_vals->values[j] = value_clone(right_vals->values[i]);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    value_list_append(result_keys, value_clone(right_keys->values[i]));
                    value_list_append(result_vals, value_clone(right_vals->values[i]));
                }
            }
            
            value_free(left);
            value_free(right);
            return value_create_dict(result_keys, result_vals);
        }
    } else if (strcmp(op, "-") == 0) {
        if (left->type == VAL_INTEGER && right->type == VAL_INTEGER) {
            int64_t result = left->data.integer.value - right->data.integer.value;
            value_free(left);
            value_free(right);
            return value_create_integer(result);
        } else if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
                   (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double l_val = left->type == VAL_INTEGER ? (double)left->data.integer.value : left->data.decimal.value;
            double r_val = right->type == VAL_INTEGER ? (double)right->data.integer.value : right->data.decimal.value;
            double result = l_val - r_val;
            value_free(left);
            value_free(right);
            return value_create_decimal(result);
        }
    } else if (strcmp(op, "*") == 0) {
        if (left->type == VAL_INTEGER && right->type == VAL_INTEGER) {
            int64_t result = left->data.integer.value * right->data.integer.value;
            value_free(left);
            value_free(right);
            return value_create_integer(result);
        } else if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
                   (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double l_val = left->type == VAL_INTEGER ? (double)left->data.integer.value : left->data.decimal.value;
            double r_val = right->type == VAL_INTEGER ? (double)right->data.integer.value : right->data.decimal.value;
            double result = l_val * r_val;
            value_free(left);
            value_free(right);
            return value_create_decimal(result);
        } else if (left->type == VAL_STRING && right->type == VAL_INTEGER) {
            int64_t count = right->data.integer.value;
            if (count < 0) {
                value_free(left);
                value_free(right);
                return value_create_error("Unsupported operation: String * Integer (< 0)");
            }
            if (count == 0) {
                value_free(left);
                value_free(right);
                return value_create_string("");
            }
            
            size_t str_len = strlen(left->data.string.value);
            char *result = malloc(str_len * count + 1);
            result[0] = '\0';
            for (int64_t i = 0; i < count; i++) {
                strcat(result, left->data.string.value);
            }
            value_free(left);
            value_free(right);
            Value *val = value_create_string(result);
            free(result);
            return val;
        } else if (left->type == VAL_STRING && right->type == VAL_DECIMAL) {
            value_free(left);
            value_free(right);
            return value_create_error("Unsupported operation: String * Decimal");
        }
    } else if (strcmp(op, "/") == 0) {
        if (left->type == VAL_INTEGER && right->type == VAL_INTEGER) {
            if (right->data.integer.value == 0) {
                value_free(left);
                value_free(right);
                return value_create_error("Division by zero");
            }
            // Truncating division toward zero
            int64_t result = left->data.integer.value / right->data.integer.value;
            value_free(left);
            value_free(right);
            return value_create_integer(result);
        } else if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
                   (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double l_val = left->type == VAL_INTEGER ? (double)left->data.integer.value : left->data.decimal.value;
            double r_val = right->type == VAL_INTEGER ? (double)right->data.integer.value : right->data.decimal.value;
            if (r_val == 0.0) {
                value_free(left);
                value_free(right);
                return value_create_error("Division by zero");
            }
            double result = l_val / r_val;
            value_free(left);
            value_free(right);
            return value_create_decimal(result);
        }
    } else if (strcmp(op, "==") == 0) {
        // Structural equality
        bool result = values_equal(left, right);
        value_free(left);
        value_free(right);
        return value_create_boolean(result);
    }
    
    // Comparison operations
    if (strcmp(op, ">") == 0) {
        if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
            (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double left_val = left->type == VAL_INTEGER ? (double)left->data.integer.value : left->data.decimal.value;
            double right_val = right->type == VAL_INTEGER ? (double)right->data.integer.value : right->data.decimal.value;
            bool result = left_val > right_val;
            value_free(left);
            value_free(right);
            return value_create_boolean(result);
        }
    } else if (strcmp(op, "<") == 0) {
        if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
            (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double left_val = left->type == VAL_INTEGER ? (double)left->data.integer.value : left->data.decimal.value;
            double right_val = right->type == VAL_INTEGER ? (double)right->data.integer.value : right->data.decimal.value;
            bool result = left_val < right_val;
            value_free(left);
            value_free(right);
            return value_create_boolean(result);
        }
    } else if (strcmp(op, ">=") == 0) {
        if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
            (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double left_val = left->type == VAL_INTEGER ? (double)left->data.integer.value : left->data.decimal.value;
            double right_val = right->type == VAL_INTEGER ? (double)right->data.integer.value : right->data.decimal.value;
            bool result = left_val >= right_val;
            value_free(left);
            value_free(right);
            return value_create_boolean(result);
        }
    } else if (strcmp(op, "<=") == 0) {
        if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
            (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double left_val = left->type == VAL_INTEGER ? (double)left->data.integer.value : left->data.decimal.value;
            double right_val = right->type == VAL_INTEGER ? (double)right->data.integer.value : right->data.decimal.value;
            bool result = left_val <= right_val;
            value_free(left);
            value_free(right);
            return value_create_boolean(result);
        }
    } else if (strcmp(op, "==") == 0) {
        bool result = values_equal(left, right);
        value_free(left);
        value_free(right);
        return value_create_boolean(result);
    } else if (strcmp(op, "!=") == 0) {
        bool result = !values_equal(left, right);
        value_free(left);
        value_free(right);
        return value_create_boolean(result);
    }
    
    // Default error for unsupported operations
    const char *left_type = left->type == VAL_INTEGER ? "Integer" : 
                          left->type == VAL_DECIMAL ? "Decimal" : 
                          left->type == VAL_STRING ? "String" : 
                          left->type == VAL_BOOLEAN ? "Boolean" :
                          left->type == VAL_LIST ? "List" :
                          left->type == VAL_SET ? "Set" :
                          left->type == VAL_DICT ? "Dict" : "Unknown";
    const char *right_type = right->type == VAL_INTEGER ? "Integer" : 
                           right->type == VAL_DECIMAL ? "Decimal" : 
                           right->type == VAL_STRING ? "String" : 
                           right->type == VAL_BOOLEAN ? "Boolean" :
                           right->type == VAL_LIST ? "List" :
                           right->type == VAL_SET ? "Set" :
                           right->type == VAL_DICT ? "Dict" : "Unknown";
    
    char *error_msg = malloc(256);
    snprintf(error_msg, 256, "Unsupported operation: %s %s %s", left_type, op, right_type);
    
    value_free(left);
    value_free(right);
    Value *error = value_create_error(error_msg);
    free(error_msg);
    return error;
}

// Function call handling with built-in functions
Value *evaluate_unary(Evaluator *eval, ASTNode *node) {
    Value *operand = evaluate(eval, node->data.unary.operand);
    if (operand->type == VAL_ERROR) return operand;
    
    const char *op = node->data.unary.operator;
    
    if (strcmp(op, "-") == 0) {
        if (operand->type == VAL_INTEGER) {
            int64_t result = -operand->data.integer.value;
            value_free(operand);
            return value_create_integer(result);
        } else if (operand->type == VAL_DECIMAL) {
            double result = -operand->data.decimal.value;
            value_free(operand);
            return value_create_decimal(result);
        } else {
            value_free(operand);
            return value_create_error("Unsupported unary operation");
        }
    }
    
    value_free(operand);
    return value_create_error("Unknown unary operator");
}

Value *evaluate_call(Evaluator *eval, ASTNode *node) {
    // Check if it's a built-in function call
    if (node->data.call.function->type == AST_IDENTIFIER) {
        const char *func_name = node->data.call.function->data.identifier.name;
        
        if (strcmp(func_name, "puts") == 0) {
            ValueList *args = value_list_create();
            for (size_t i = 0; i < node->data.call.arguments->count; i++) {
                Value *arg = evaluate(eval, node->data.call.arguments->nodes[i]);
                if (arg->type == VAL_ERROR) {
                    value_list_free(args);
                    return arg;
                }
                value_list_append(args, arg);
            }
            Value *result = builtin_puts(eval, args);
            value_list_free(args);
            return result;
        }
        
        if (strcmp(func_name, "first") == 0) {
            if (node->data.call.arguments->count != 1) {
                return value_create_error("first requires exactly 1 argument");
            }
            Value *collection = evaluate(eval, node->data.call.arguments->nodes[0]);
            if (collection->type == VAL_ERROR) return collection;
            Value *result = builtin_first(collection);
            value_free(collection);
            return result;
        }
        
        if (strcmp(func_name, "rest") == 0) {
            if (node->data.call.arguments->count != 1) {
                return value_create_error("rest requires exactly 1 argument");
            }
            Value *collection = evaluate(eval, node->data.call.arguments->nodes[0]);
            if (collection->type == VAL_ERROR) return collection;
            Value *result = builtin_rest(collection);
            value_free(collection);
            return result;
        }
        
        // Check for operator function calls (+, -, *, /)
        if ((strcmp(func_name, "+") == 0 || strcmp(func_name, "-") == 0 ||
             strcmp(func_name, "*") == 0 || strcmp(func_name, "/") == 0) &&
            node->data.call.arguments->count == 2) {
            
            // Create a synthetic infix expression
            ASTNode *infix_node = malloc(sizeof(ASTNode));
            infix_node->type = AST_INFIX;
            infix_node->data.infix.left = node->data.call.arguments->nodes[0];
            infix_node->data.infix.operator = malloc(strlen(func_name) + 1);
            strcpy(infix_node->data.infix.operator, func_name);
            infix_node->data.infix.right = node->data.call.arguments->nodes[1];
            
            Value *result = evaluate_infix(eval, infix_node);
            
            free(infix_node->data.infix.operator);
            free(infix_node);
            return result;
        }
        
        // Try to resolve as a regular variable that might be a function
        Value *func_value = env_get(eval->current_env, func_name);
        if (!func_value) {
            char *error_msg = malloc(128);
            snprintf(error_msg, 128, "Identifier can not be found: %s", func_name);
            Value *error = value_create_error(error_msg);
            free(error_msg);
            return error;
        }
        
        if (func_value->type == VAL_FUNCTION || func_value->type == VAL_BUILTIN_FUNCTION || func_value->type == VAL_PARTIAL_FUNCTION) {
            // Evaluate arguments
            ValueList *args = value_list_create();
            for (size_t i = 0; i < node->data.call.arguments->count; i++) {
                Value *arg = evaluate(eval, node->data.call.arguments->nodes[i]);
                if (arg->type == VAL_ERROR) {
                    for (size_t j = 0; j < args->count; j++) {
                        value_free(args->values[j]);
                    }
                    value_list_free(args);
                    return arg;
                }
                value_list_append(args, arg);
            }
            
            Value *result = call_function(eval, func_value, args);
            value_list_free(args);
            return result;
        }
        
        return value_create_error("Expected a Function, found: Integer");
    }
    
    // Handle general function calls (non-identifier functions)
    Value *func_value = evaluate(eval, node->data.call.function);
    if (func_value->type == VAL_ERROR) return func_value;
    
    if (func_value->type == VAL_FUNCTION || func_value->type == VAL_BUILTIN_FUNCTION || func_value->type == VAL_PARTIAL_FUNCTION) {
        // Evaluate arguments
        ValueList *args = value_list_create();
        for (size_t i = 0; i < node->data.call.arguments->count; i++) {
            Value *arg = evaluate(eval, node->data.call.arguments->nodes[i]);
            if (arg->type == VAL_ERROR) {
                for (size_t j = 0; j < args->count; j++) {
                    value_free(args->values[j]);
                }
                value_list_free(args);
                value_free(func_value);
                return arg;
            }
            value_list_append(args, arg);
        }
        
        Value *result = call_function(eval, func_value, args);
        value_list_free(args);
        value_free(func_value);
        return result;
    }
    
    value_free(func_value);
    return value_create_error("Expected a Function, found: Integer");
}

// Helper functions for collections
void print_list(ValueList *list) {
    printf("[");
    for (size_t i = 0; i < list->count; i++) {
        if (i > 0) printf(", ");
        print_value(list->values[i]);
    }
    printf("]");
}

void print_set(ValueList *set) {
    printf("{");
    // Create a sorted copy of indices for deterministic output
    size_t *sorted_indices = malloc(set->count * sizeof(size_t));
    for (size_t i = 0; i < set->count; i++) {
        sorted_indices[i] = i;
    }
    
    // Sort indices by value (simple bubble sort for small collections)
    for (size_t i = 0; i < set->count; i++) {
        for (size_t j = i + 1; j < set->count; j++) {
            if (value_compare(set->values[sorted_indices[i]], set->values[sorted_indices[j]]) > 0) {
                size_t temp = sorted_indices[i];
                sorted_indices[i] = sorted_indices[j];
                sorted_indices[j] = temp;
            }
        }
    }
    
    for (size_t i = 0; i < set->count; i++) {
        if (i > 0) printf(", ");
        print_value(set->values[sorted_indices[i]]);
    }
    printf("}");
    free(sorted_indices);
}

void print_dict(ValueList *keys, ValueList *values) {
    printf("#{");
    // Create a sorted copy of indices for deterministic output
    size_t *sorted_indices = malloc(keys->count * sizeof(size_t));
    for (size_t i = 0; i < keys->count; i++) {
        sorted_indices[i] = i;
    }
    
    // Sort indices by key (simple bubble sort for small collections)  
    for (size_t i = 0; i < keys->count; i++) {
        for (size_t j = i + 1; j < keys->count; j++) {
            if (value_compare(keys->values[sorted_indices[i]], keys->values[sorted_indices[j]]) > 0) {
                size_t temp = sorted_indices[i];
                sorted_indices[i] = sorted_indices[j];
                sorted_indices[j] = temp;
            }
        }
    }
    
    for (size_t i = 0; i < keys->count; i++) {
        if (i > 0) printf(", ");
        print_value(keys->values[sorted_indices[i]]);
        printf(": ");
        print_value(values->values[sorted_indices[i]]);
    }
    printf("}");
    free(sorted_indices);
}

int value_compare(Value *a, Value *b) {
    // Compare values for sorting - returns < 0 if a < b, 0 if equal, > 0 if a > b
    if (a->type != b->type) {
        // Sort by type first: integers < decimals < strings < booleans < others
        return (int)a->type - (int)b->type;
    }
    
    switch (a->type) {
        case VAL_INTEGER:
            if (a->data.integer.value < b->data.integer.value) return -1;
            if (a->data.integer.value > b->data.integer.value) return 1;
            return 0;
        case VAL_DECIMAL:
            if (a->data.decimal.value < b->data.decimal.value) return -1;
            if (a->data.decimal.value > b->data.decimal.value) return 1;
            return 0;
        case VAL_STRING:
            return strcmp(a->data.string.value, b->data.string.value);
        case VAL_BOOLEAN:
            // false < true
            if (a->data.boolean.value == b->data.boolean.value) return 0;
            return a->data.boolean.value ? 1 : -1;
        default:
            return 0; // For complex types, consider equal for sorting
    }
}

bool values_equal(Value *a, Value *b) {
    if (a->type != b->type) return false;
    
    switch (a->type) {
        case VAL_INTEGER:
            return a->data.integer.value == b->data.integer.value;
        case VAL_DECIMAL:
            return a->data.decimal.value == b->data.decimal.value;
        case VAL_STRING:
            return strcmp(a->data.string.value, b->data.string.value) == 0;
        case VAL_BOOLEAN:
            return a->data.boolean.value == b->data.boolean.value;
        case VAL_NIL:
            return true;
        case VAL_LIST: {
            ValueList *la = a->data.list.elements;
            ValueList *lb = b->data.list.elements;
            if (la->count != lb->count) return false;
            for (size_t i = 0; i < la->count; i++) {
                if (!values_equal(la->values[i], lb->values[i])) return false;
            }
            return true;
        }
        case VAL_SET: {
            ValueList *sa = a->data.set.elements;
            ValueList *sb = b->data.set.elements;
            if (sa->count != sb->count) return false;
            // For sets, order doesn't matter, so check each element in a is in b
            for (size_t i = 0; i < sa->count; i++) {
                bool found = false;
                for (size_t j = 0; j < sb->count; j++) {
                    if (values_equal(sa->values[i], sb->values[j])) {
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
            return true;
        }
        case VAL_DICT: {
            ValueList *ka = a->data.dict.keys;
            ValueList *va = a->data.dict.values;
            ValueList *kb = b->data.dict.keys;
            ValueList *vb = b->data.dict.values;
            if (ka->count != kb->count) return false;
            // For dicts, check each key-value pair in a is in b
            for (size_t i = 0; i < ka->count; i++) {
                bool found = false;
                for (size_t j = 0; j < kb->count; j++) {
                    if (values_equal(ka->values[i], kb->values[j])) {
                        if (!values_equal(va->values[i], vb->values[j])) return false;
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
            return true;
        }
        default:
            return false;
    }
}

Value *evaluate_index(Value *target, Value *index) {
    switch (target->type) {
        case VAL_LIST: {
            if (index->type != VAL_INTEGER) {
                char *type_name = index->type == VAL_DECIMAL ? "Decimal" : 
                                 index->type == VAL_BOOLEAN ? "Boolean" : "Unknown";
                char error_msg[100];
                snprintf(error_msg, sizeof(error_msg), "Unable to perform index operation, found: List[%s]", type_name);
                return value_create_error(error_msg);
            }
            
            ValueList *list = target->data.list.elements;
            int64_t idx = index->data.integer.value;
            
            // Handle negative indexing
            if (idx < 0) {
                idx = (int64_t)list->count + idx;
            }
            
            if (idx < 0 || idx >= (int64_t)list->count) {
                return value_create_nil();
            }
            
            return value_clone(list->values[idx]);
        }
        
        case VAL_STRING: {
            if (index->type != VAL_INTEGER) {
                char *type_name = index->type == VAL_DECIMAL ? "Decimal" : 
                                 index->type == VAL_BOOLEAN ? "Boolean" : "Unknown";
                char error_msg[100];
                snprintf(error_msg, sizeof(error_msg), "Unable to perform index operation, found: String[%s]", type_name);
                return value_create_error(error_msg);
            }
            
            const char *str = target->data.string.value;
            int64_t len = (int64_t)strlen(str);
            int64_t idx = index->data.integer.value;
            
            // Handle negative indexing
            if (idx < 0) {
                idx = len + idx;
            }
            
            if (idx < 0 || idx >= len) {
                return value_create_nil();
            }
            
            char result[2] = {str[idx], '\0'};
            return value_create_string(result);
        }
        
        case VAL_DICT: {
            ValueList *keys = target->data.dict.keys;
            ValueList *values = target->data.dict.values;
            
            for (size_t i = 0; i < keys->count; i++) {
                if (values_equal(index, keys->values[i])) {
                    return value_clone(values->values[i]);
                }
            }
            return value_create_nil();
        }
        
        default:
            return value_create_error("Cannot index this type");
    }
}

Value *evaluate(Evaluator *eval, ASTNode *node) {
    if (!node) return value_create_nil();
    
    switch (node->type) {
        case AST_INTEGER: {
            // Parse integer, removing underscores
            int64_t val = 0;
            const char *str = node->data.integer.value;
            for (size_t i = 0; str[i]; i++) {
                if (str[i] != '_') {
                    val = val * 10 + (str[i] - '0');
                }
            }
            return value_create_integer(val);
        }
        
        case AST_DECIMAL: {
            // Parse decimal, removing underscores
            char *clean_str = malloc(strlen(node->data.decimal.value) + 1);
            size_t j = 0;
            for (size_t i = 0; node->data.decimal.value[i]; i++) {
                if (node->data.decimal.value[i] != '_') {
                    clean_str[j++] = node->data.decimal.value[i];
                }
            }
            clean_str[j] = '\0';
            double val = atof(clean_str);
            free(clean_str);
            return value_create_decimal(val);
        }
        
        case AST_STRING:
            return value_create_string(node->data.string.value);
            
        case AST_BOOLEAN:
            return value_create_boolean(node->data.boolean.value);
            
        case AST_NIL:
            return value_create_nil();
            
        case AST_IDENTIFIER: {
            const char *name = node->data.identifier.name;
            Value *val = env_get(eval->current_env, name);
            if (!val) {
                char *error_msg = malloc(128);
                snprintf(error_msg, 128, "Identifier can not be found: %s", name);
                Value *error = value_create_error(error_msg);
                free(error_msg);
                return error;
            }
            return value_clone(val);
        }
        
        case AST_LET: {
            const char *name = node->data.let.name->data.identifier.name;
            
            // Special handling for recursive functions
            if (node->data.let.value->type == AST_FUNCTION) {
                // Create a placeholder for self-reference
                Value *placeholder = value_create_nil();
                env_define(eval->current_env, name, placeholder, false);
                
                // Evaluate the function with self-reference available
                Value *val = evaluate(eval, node->data.let.value);
                if (val->type == VAL_ERROR) return val;
                
                // Replace the placeholder with the actual function
                env_set(eval->current_env, name, value_clone(val));
                
                return val;
            } else {
                // Normal let binding
                Value *val = evaluate(eval, node->data.let.value);
                if (val->type == VAL_ERROR) return val;
                
                env_define(eval->current_env, name, value_clone(val), false);
                return val;
            }
        }
        
        case AST_MUTABLE_LET: {
            Value *val = evaluate(eval, node->data.mutable_let.value);
            if (val->type == VAL_ERROR) return val;
            
            const char *name = node->data.mutable_let.name->data.identifier.name;
            env_define(eval->current_env, name, value_clone(val), true);
            return val;
        }
        
        case AST_ASSIGNMENT: {
            const char *name = node->data.assignment.target->data.identifier.name;
            Value *val = evaluate(eval, node->data.assignment.value);
            if (val->type == VAL_ERROR) return val;
            
            if (!env_assign(eval->current_env, name, val)) {
                char *error_msg = malloc(128);
                snprintf(error_msg, 128, "Variable '%s' is not mutable", name);
                value_free(val);
                Value *error = value_create_error(error_msg);
                free(error_msg);
                return error;
            }
            
            return val;
        }
        
        case AST_INFIX:
            return evaluate_infix(eval, node);
            
        case AST_UNARY:
            return evaluate_unary(eval, node);
            
        case AST_CALL:
            return evaluate_call(eval, node);
            
        case AST_PROGRAM: {
            Value *last_val = value_create_nil();
            for (size_t i = 0; i < node->data.program.statements->count; i++) {
                ASTNode *stmt = node->data.program.statements->nodes[i];
                
                // Skip comments - they don't contribute to the program result
                if (stmt->type == AST_COMMENT) {
                    continue;
                }
                
                value_free(last_val);
                last_val = evaluate(eval, stmt);
                if (last_val->type == VAL_ERROR) {
                    return last_val;
                }
            }
            return last_val;
        }
        
        case AST_STATEMENT_EXPRESSION:
            return evaluate(eval, node->data.statement_expression.value);
            
        case AST_COMMENT:
            // Comments evaluate to nil and are essentially ignored
            return value_create_nil();
            
        case AST_LIST: {
            ValueList *list = value_list_create();
            ASTNodeList *ast_list = node->data.list.elements;
            for (size_t i = 0; i < ast_list->count; i++) {
                Value *elem = evaluate(eval, ast_list->nodes[i]);
                if (elem->type == VAL_ERROR) {
                    value_list_free(list);
                    return elem;
                }
                value_list_append(list, elem);
            }
            return value_create_list(list);
        }
        
        case AST_SET: {
            ValueList *set = value_list_create();
            ASTNodeList *ast_list = node->data.set.elements;
            for (size_t i = 0; i < ast_list->count; i++) {
                Value *elem = evaluate(eval, ast_list->nodes[i]);
                if (elem->type == VAL_ERROR) {
                    value_list_free(set);
                    return elem;
                }
                // Check for dict in set - not allowed
                if (elem->type == VAL_DICT) {
                    value_free(elem);
                    value_list_free(set);
                    return value_create_error("Unable to include a Dictionary within a Set");
                }
                // Check for duplicates - sets don't contain duplicates
                bool found = false;
                for (size_t j = 0; j < set->count; j++) {
                    if (values_equal(elem, set->values[j])) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    value_list_append(set, elem);
                } else {
                    value_free(elem);
                }
            }
            return value_create_set(set);
        }
        
        case AST_DICT: {
            ValueList *keys = value_list_create();
            ValueList *values = value_list_create();
            ASTNodeList *entries = node->data.dict.items;
            
            for (size_t i = 0; i < entries->count; i++) {
                ASTNode *entry = entries->nodes[i];
                if (entry->type != AST_DICT_ENTRY) {
                    value_list_free(keys);
                    value_list_free(values);
                    return value_create_error("Expected dictionary entry");
                }
                
                Value *key = evaluate(eval, entry->data.dict_entry.key);
                if (key->type == VAL_ERROR) {
                    value_list_free(keys);
                    value_list_free(values);
                    return key;
                }
                
                // Check if key is a dictionary - not allowed as dict key
                if (key->type == VAL_DICT) {
                    value_free(key);
                    value_list_free(keys);
                    value_list_free(values);
                    return value_create_error("Unable to use a Dictionary as a Dictionary key");
                }
                
                Value *val = evaluate(eval, entry->data.dict_entry.value);
                if (val->type == VAL_ERROR) {
                    value_free(key);
                    value_list_free(keys);
                    value_list_free(values);
                    return val;
                }
                
                value_list_append(keys, key);
                value_list_append(values, val);
            }
            return value_create_dict(keys, values);
        }
        
        case AST_INDEX: {
            Value *target = evaluate(eval, node->data.index.object);
            if (target->type == VAL_ERROR) return target;
            
            Value *index = evaluate(eval, node->data.index.index);
            if (index->type == VAL_ERROR) {
                value_free(target);
                return index;
            }
            
            Value *result = evaluate_index(target, index);
            value_free(target);
            value_free(index);
            return result;
        }
        
        case AST_FUNCTION: {
            // Capture the current environment as closure environment
            // Use direct reference to support both recursion and shared mutable state
            return value_create_function(node->data.function.parameters, node->data.function.body, eval->current_env);
        }
        
        case AST_FUNCTION_COMPOSITION: {
            // Function composition a >> b >> c creates a function that applies a, then b, then c
            ASTNodeList *functions = node->data.function_composition.functions;
            if (functions->count == 0) {
                return value_create_error("Empty function composition");
            }
            
            if (functions->count == 1) {
                // Single function, no composition needed
                return evaluate(eval, functions->nodes[0]);
            }
            
            // Create a lambda function that composes all the functions
            // The composition f >> g becomes |x| g(f(x))
            // For multiple functions f >> g >> h, it becomes |x| h(g(f(x)))
            
            // Create parameter list (single parameter 'x')
            ASTNodeList *params = ast_node_list_create();
            ASTNode *param = ast_node_create(AST_IDENTIFIER);
            param->data.identifier.name = strdup("x");
            ast_node_list_append(params, param);
            
            // Create the body: rightmost(second_rightmost(...(leftmost(x))...))
            // Start with x
            ASTNode *body = ast_node_create(AST_IDENTIFIER);
            body->data.identifier.name = strdup("x");
            
            // Apply each function from left to right
            for (size_t i = 0; i < functions->count; i++) {
                ASTNode *call = ast_node_create(AST_CALL);
                call->data.call.function = functions->nodes[i]; // Don't clone, just reference
                call->data.call.arguments = ast_node_list_create();
                ast_node_list_append(call->data.call.arguments, body);
                body = call;
            }
            
            return value_create_function(params, body, eval->current_env);
        }
        
        case AST_FUNCTION_THREAD: {
            // Function threading a |> b |> c applies b(a), then c(result)
            Value *current = evaluate(eval, node->data.function_thread.initial);
            if (current->type == VAL_ERROR) return current;
            
            ASTNodeList *functions = node->data.function_thread.functions;
            for (size_t i = 0; i < functions->count; i++) {
                Value *func = evaluate(eval, functions->nodes[i]);
                if (func->type == VAL_ERROR) {
                    value_free(current);
                    return func;
                }
                
                // Call the function with current as argument
                ValueList *args = value_list_create();
                value_list_append(args, current);
                current = call_function(eval, func, args);
                value_free(func);
                
                if (current->type == VAL_ERROR) {
                    free(args->values);
                    free(args);
                    return current;
                }
                free(args->values);
                free(args);
            }
            
            return current;
        }
        
        case AST_BLOCK: {
            // Execute all statements in the block and return the last value
            Value *result = value_create_nil();
            ASTNodeList *statements = node->data.block.statements;
            for (size_t i = 0; i < statements->count; i++) {
                value_free(result);
                result = evaluate(eval, statements->nodes[i]);
                if (result->type == VAL_ERROR) {
                    return result;
                }
            }
            return result;
        }
        
        case AST_IF: {
            Value *condition = evaluate(eval, node->data.if_expr.condition);
            if (condition->type == VAL_ERROR) return condition;
            
            // Check truthiness: false and nil are falsy, everything else is truthy
            bool is_truthy = !(condition->type == VAL_BOOLEAN && !condition->data.boolean.value) &&
                           condition->type != VAL_NIL;
            
            value_free(condition);
            
            if (is_truthy) {
                return evaluate(eval, node->data.if_expr.then_branch);
            } else if (node->data.if_expr.else_branch) {
                return evaluate(eval, node->data.if_expr.else_branch);
            } else {
                return value_create_nil();
            }
        }
            
        default:
            return value_create_error("Unimplemented AST node type");
    }
}

// Check if a value contains functions that reference a specific environment
bool value_references_environment(Value *value, Environment *env) {
    if (!value || !env) return false;
    
    switch (value->type) {
        case VAL_FUNCTION:
            return value->data.function.closure_env == env;
        case VAL_PARTIAL_FUNCTION:
            return value_references_environment(value->data.partial_function.original_function, env);
        case VAL_LIST:
            for (size_t i = 0; i < value->data.list.elements->count; i++) {
                if (value_references_environment(value->data.list.elements->values[i], env)) {
                    return true;
                }
            }
            return false;
        case VAL_SET:
            for (size_t i = 0; i < value->data.set.elements->count; i++) {
                if (value_references_environment(value->data.set.elements->values[i], env)) {
                    return true;
                }
            }
            return false;
        case VAL_DICT:
            for (size_t i = 0; i < value->data.dict.keys->count; i++) {
                if (value_references_environment(value->data.dict.keys->values[i], env) ||
                    value_references_environment(value->data.dict.values->values[i], env)) {
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

// Function call implementation
Value *call_function(Evaluator *eval, Value *function, ValueList *args) {
    if (function->type == VAL_BUILTIN_FUNCTION) {
        // Handle builtin functions
        const char *name = function->data.builtin_function.name;
        
        if (strcmp(name, "push") == 0) {
            if (args->count == 2) {
                return builtin_push(args->values[0], args->values[1]);
            } else if (args->count == 1) {
                // Create partial application for push
                ValueList *partial_args = value_list_create();
                value_list_append(partial_args, value_clone(args->values[0]));
                return value_create_partial_function(value_create_builtin_function(name), partial_args);
            }
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), "push requires exactly 2 arguments, got %zu", args->count);
            return value_create_error(error_msg);
        } else if (strcmp(name, "fold") == 0) {
            if (args->count == 3) {
                return builtin_fold(eval, args->values[1], args->values[0], args->values[2]); // Note: order is (initial, function, collection)
            } else if (args->count == 2) {
                // Create partial application for fold(initial, function) -> waiting for collection
                ValueList *partial_args = value_list_create();
                value_list_append(partial_args, value_clone(args->values[0]));
                value_list_append(partial_args, value_clone(args->values[1]));
                return value_create_partial_function(value_create_builtin_function(name), partial_args);
            } else if (args->count == 1) {
                // Create partial application for fold(initial) -> waiting for function and collection
                ValueList *partial_args = value_list_create();
                value_list_append(partial_args, value_clone(args->values[0]));
                return value_create_partial_function(value_create_builtin_function(name), partial_args);
            }
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), "fold requires exactly 3 arguments, got %zu", args->count);
            return value_create_error(error_msg);
        } else if (strcmp(name, "map") == 0) {
            if (args->count == 2) {
                return builtin_map(eval, args->values[0], args->values[1]);
            } else if (args->count == 1) {
                // Create partial application for map
                ValueList *partial_args = value_list_create();
                value_list_append(partial_args, value_clone(args->values[0]));
                return value_create_partial_function(value_create_builtin_function(name), partial_args);
            }
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), "map requires exactly 2 arguments, got %zu", args->count);
            return value_create_error(error_msg);
        } else if (strcmp(name, "filter") == 0) {
            if (args->count == 2) {
                return builtin_filter(eval, args->values[0], args->values[1]);
            } else if (args->count == 1) {
                // Create partial application for filter
                ValueList *partial_args = value_list_create();
                value_list_append(partial_args, value_clone(args->values[0]));
                return value_create_partial_function(value_create_builtin_function(name), partial_args);
            }
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), "filter requires exactly 2 arguments, got %zu", args->count);
            return value_create_error(error_msg);
        } else if (strcmp(name, "size") == 0) {
            if (args->count == 1) {
                return builtin_size(args->values[0]);
            }
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), "size requires exactly 1 argument, got %zu", args->count);
            return value_create_error(error_msg);
        } else if (strcmp(name, "assoc") == 0) {
            if (args->count == 3) {
                return builtin_assoc(args->values[0], args->values[1], args->values[2]);
            } else if (args->count == 2) {
                // Create partial application for assoc(dict, key) -> waiting for value
                ValueList *partial_args = value_list_create();
                value_list_append(partial_args, value_clone(args->values[0]));
                value_list_append(partial_args, value_clone(args->values[1]));
                return value_create_partial_function(value_create_builtin_function(name), partial_args);
            } else if (args->count == 1) {
                // Create partial application for assoc(dict) -> waiting for key and value
                ValueList *partial_args = value_list_create();
                value_list_append(partial_args, value_clone(args->values[0]));
                return value_create_partial_function(value_create_builtin_function(name), partial_args);
            }
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), "assoc requires exactly 3 arguments, got %zu", args->count);
            return value_create_error(error_msg);
        } else {
            // Handle operator functions
            if (args->count == 2) {
                return evaluate_binary_operator(eval, name, args->values[0], args->values[1]);
            } else if (args->count == 1) {
                // Create partial application for builtin functions
                ValueList *partial_args = value_list_create();
                value_list_append(partial_args, value_clone(args->values[0]));
                return value_create_partial_function(value_create_builtin_function(name), partial_args);
            }
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), "%s requires exactly 2 arguments, got %zu", name, args->count);
            return value_create_error(error_msg);
        }
    }
    
    if (function->type == VAL_PARTIAL_FUNCTION) {
        // Handle partial function calls - combine args with stored args
        ValueList *combined_args = value_list_create();
        
        // Add stored partial args first
        for (size_t i = 0; i < function->data.partial_function.partial_args->count; i++) {
            value_list_append(combined_args, value_clone(function->data.partial_function.partial_args->values[i]));
        }
        
        // Add new args
        for (size_t i = 0; i < args->count; i++) {
            value_list_append(combined_args, value_clone(args->values[i]));
        }
        
        // Call the original function with combined args
        Value *result = call_function(eval, function->data.partial_function.original_function, combined_args);
        value_list_free(combined_args);
        return result;
    }
    
    if (function->type != VAL_FUNCTION) {
        const char* type_name = (function->type == VAL_INTEGER) ? "Integer" :
                               (function->type == VAL_DECIMAL) ? "Decimal" :
                               (function->type == VAL_STRING) ? "String" :
                               (function->type == VAL_BOOLEAN) ? "Boolean" :
                               (function->type == VAL_NIL) ? "Nil" :
                               (function->type == VAL_BUILTIN_FUNCTION) ? "BuiltinFunction" :
                               (function->type == VAL_LIST) ? "List" :
                               (function->type == VAL_SET) ? "Set" :
                               (function->type == VAL_DICT) ? "Dict" : "Unknown";
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Expected a Function, found: %s", type_name);
        return value_create_error(error_msg);
    }
    
    // Create new environment for function execution
    Environment *func_env = env_create(function->data.function.closure_env ? 
                                     function->data.function.closure_env : 
                                     eval->global_env);
    Environment *old_env = eval->current_env;
    eval->current_env = func_env;
    
    // Bind parameters to arguments
    ASTNodeList *param_list = function->data.function.params;
    if (param_list) {
        // Handle partial application for user-defined functions
        if (args->count < param_list->count) {
            // Create a partial function with the provided arguments
            eval->current_env = old_env;
            env_free(func_env);
            
            ValueList *partial_args = value_list_create();
            for (size_t i = 0; i < args->count; i++) {
                value_list_append(partial_args, value_clone(args->values[i]));
            }
            return value_create_partial_function(value_clone(function), partial_args);
        }
        
        if (args->count > param_list->count) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "Function expects %zu arguments, got %zu", 
                    param_list->count, args->count);
            eval->current_env = old_env;
            env_free(func_env);
            return value_create_error(error_msg);
        }
        
        // Bind available parameters (exactly matching count)
        size_t min_count = args->count < param_list->count ? args->count : param_list->count;
        for (size_t i = 0; i < min_count; i++) {
            if (param_list->nodes[i]->type == AST_IDENTIFIER) {
                env_define(func_env, param_list->nodes[i]->data.identifier.name, 
                          value_clone(args->values[i]), false);
            }
        }
    }
    
    // Execute function body
    Value *result = evaluate(eval, function->data.function.body);
    
    // Restore environment
    eval->current_env = old_env;
    
    // Only free the function environment if the returned value doesn't reference it
    // This prevents use-after-free for closures while still cleaning up in normal cases
    if (!value_references_environment(result, func_env)) {
        env_free(func_env);
    }
    
    return result;
}

// Binary operator evaluation (for builtin operator functions)
Value *evaluate_binary_operator(Evaluator *eval, const char *op, Value *left, Value *right) {
    (void)eval; // Unused for now
    
    if (strcmp(op, "+") == 0) {
        if (left->type == VAL_INTEGER && right->type == VAL_INTEGER) {
            int64_t result = left->data.integer.value + right->data.integer.value;
            return value_create_integer(result);
        } else if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
                   (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double left_val = (left->type == VAL_INTEGER) ? (double)left->data.integer.value : left->data.decimal.value;
            double right_val = (right->type == VAL_INTEGER) ? (double)right->data.integer.value : right->data.decimal.value;
            return value_create_decimal(left_val + right_val);
        }
    } else if (strcmp(op, "*") == 0) {
        if (left->type == VAL_INTEGER && right->type == VAL_INTEGER) {
            int64_t result = left->data.integer.value * right->data.integer.value;
            return value_create_integer(result);
        } else if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
                   (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double left_val = (left->type == VAL_INTEGER) ? (double)left->data.integer.value : left->data.decimal.value;
            double right_val = (right->type == VAL_INTEGER) ? (double)right->data.integer.value : right->data.decimal.value;
            return value_create_decimal(left_val * right_val);
        }
    } else if (strcmp(op, "-") == 0) {
        if (left->type == VAL_INTEGER && right->type == VAL_INTEGER) {
            int64_t result = left->data.integer.value - right->data.integer.value;
            return value_create_integer(result);
        } else if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
                   (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double left_val = (left->type == VAL_INTEGER) ? (double)left->data.integer.value : left->data.decimal.value;
            double right_val = (right->type == VAL_INTEGER) ? (double)right->data.integer.value : right->data.decimal.value;
            return value_create_decimal(left_val - right_val);
        }
    } else if (strcmp(op, "/") == 0) {
        if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
            (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double left_val = (left->type == VAL_INTEGER) ? (double)left->data.integer.value : left->data.decimal.value;
            double right_val = (right->type == VAL_INTEGER) ? (double)right->data.integer.value : right->data.decimal.value;
            if (right_val == 0.0) {
                return value_create_error("Division by zero");
            }
            return value_create_decimal(left_val / right_val);
        }
    } else if (strcmp(op, ">") == 0) {
        if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
            (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double left_val = (left->type == VAL_INTEGER) ? (double)left->data.integer.value : left->data.decimal.value;
            double right_val = (right->type == VAL_INTEGER) ? (double)right->data.integer.value : right->data.decimal.value;
            return value_create_boolean(left_val > right_val);
        }
    } else if (strcmp(op, "<") == 0) {
        if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
            (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double left_val = (left->type == VAL_INTEGER) ? (double)left->data.integer.value : left->data.decimal.value;
            double right_val = (right->type == VAL_INTEGER) ? (double)right->data.integer.value : right->data.decimal.value;
            return value_create_boolean(left_val < right_val);
        }
    } else if (strcmp(op, ">=") == 0) {
        if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
            (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double left_val = (left->type == VAL_INTEGER) ? (double)left->data.integer.value : left->data.decimal.value;
            double right_val = (right->type == VAL_INTEGER) ? (double)right->data.integer.value : right->data.decimal.value;
            return value_create_boolean(left_val >= right_val);
        }
    } else if (strcmp(op, "<=") == 0) {
        if ((left->type == VAL_INTEGER || left->type == VAL_DECIMAL) &&
            (right->type == VAL_INTEGER || right->type == VAL_DECIMAL)) {
            double left_val = (left->type == VAL_INTEGER) ? (double)left->data.integer.value : left->data.decimal.value;
            double right_val = (right->type == VAL_INTEGER) ? (double)right->data.integer.value : right->data.decimal.value;
            return value_create_boolean(left_val <= right_val);
        }
    } else if (strcmp(op, "==") == 0) {
        return value_create_boolean(values_equal(left, right));
    } else if (strcmp(op, "!=") == 0) {
        return value_create_boolean(!values_equal(left, right));
    }
    
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg), "Unsupported operation: %s", op);
    return value_create_error(error_msg);
}

// Partial application disabled for now
Value *create_partial_function(Value *function, ValueList *partial_args) {
    // Free the partial args since we're not using them
    for (size_t i = 0; i < partial_args->count; i++) {
        value_free(partial_args->values[i]);
    }
    value_list_free(partial_args);
    // Just return the original function for now
    return value_clone(function);
}

// Higher-order function implementations
Value *builtin_map(Evaluator *eval, Value *function, Value *collection) {
    if (function->type != VAL_FUNCTION && function->type != VAL_PARTIAL_FUNCTION && function->type != VAL_BUILTIN_FUNCTION) {
        char error_msg[100];
        snprintf(error_msg, sizeof(error_msg), "Unexpected argument: map(%s, List)", 
                function->type == VAL_INTEGER ? "Integer" : "Unknown");
        return value_create_error(error_msg);
    }
    
    if (collection->type != VAL_LIST) {
        return value_create_error("Unexpected argument: map(Function, Integer)");
    }
    
    ValueList *input = collection->data.list.elements;
    ValueList *output = value_list_create();
    
    // Use the provided evaluator for function calls
    
    for (size_t i = 0; i < input->count; i++) {
        ValueList *args = value_list_create();
        value_list_append(args, input->values[i]);
        
        Value *result = call_function(eval, function, args);
        if (result->type == VAL_ERROR) {
            free(args->values);
            free(args);
            value_list_free(output);
            return result;
        }
        
        value_list_append(output, result);
        free(args->values);
        free(args);
    }
    
    return value_create_list(output);
}

Value *builtin_filter(Evaluator *eval, Value *function, Value *collection) {
    if (function->type != VAL_FUNCTION && function->type != VAL_PARTIAL_FUNCTION && function->type != VAL_BUILTIN_FUNCTION) {
        return value_create_error("Unexpected argument: filter(Integer, List)");
    }
    
    if (collection->type != VAL_LIST) {
        return value_create_error("Unexpected argument: filter(Function, Integer)");
    }
    
    ValueList *input = collection->data.list.elements;
    ValueList *output = value_list_create();
    
    // Use the provided evaluator for function calls
    
    for (size_t i = 0; i < input->count; i++) {
        ValueList *args = value_list_create();
        value_list_append(args, input->values[i]);
        
        Value *result = call_function(eval, function, args);
        if (result->type == VAL_ERROR) {
            free(args->values);
            free(args);
            value_list_free(output);
            return result;
        }
        
        if (is_truthy(result)) {
            value_list_append(output, value_clone(input->values[i]));
        }
        
        value_free(result);
        free(args->values);
        free(args);
    }
    
    return value_create_list(output);
}

Value *builtin_fold(Evaluator *eval, Value *function, Value *initial, Value *collection) {
    if (function->type != VAL_FUNCTION && function->type != VAL_BUILTIN_FUNCTION) {
        return value_create_error("Unexpected argument: fold(Integer, Integer, List)");
    }
    
    if (collection->type != VAL_LIST) {
        return value_create_error("Unexpected argument: fold(Function, Value, Integer)");
    }
    
    ValueList *input = collection->data.list.elements;
    Value *accumulator = value_clone(initial);
    
    // Use the provided evaluator for function calls
    
    for (size_t i = 0; i < input->count; i++) {
        ValueList *args = value_list_create();
        value_list_append(args, accumulator);
        value_list_append(args, input->values[i]);
        
        Value *result = call_function(eval, function, args);
        if (result->type == VAL_ERROR) {
            value_free(accumulator);
            free(args->values);
            free(args);
            return result;
        }
        
        value_free(accumulator);
        accumulator = result;
        free(args->values);
        free(args);
    }
    
    return accumulator;
}