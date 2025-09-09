#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include <stdbool.h>

typedef enum {
    AST_INTEGER,
    AST_DECIMAL,
    AST_STRING,
    AST_BOOLEAN,
    AST_NIL,
    AST_IDENTIFIER,
    AST_LET,
    AST_MUTABLE_LET,
    AST_INFIX,
    AST_UNARY,
    AST_FUNCTION,
    AST_BLOCK,
    AST_LIST,
    AST_SET,
    AST_DICT,
    AST_DICT_ENTRY,
    AST_INDEX,
    AST_IF,
    AST_CALL,
    AST_ASSIGNMENT,
    AST_FUNCTION_COMPOSITION,
    AST_FUNCTION_THREAD,
    AST_COMMENT,
    AST_PROGRAM,
    AST_STATEMENT_EXPRESSION
} ASTNodeType;

typedef struct ASTNode ASTNode;
typedef struct ASTNodeList ASTNodeList;

struct ASTNodeList {
    ASTNode **nodes;
    size_t count;
    size_t capacity;
};

struct ASTNode {
    ASTNodeType type;
    union {
        struct {
            char *value;
        } integer;
        struct {
            char *value;
        } decimal;
        struct {
            char *value;
        } string;
        struct {
            bool value;
        } boolean;
        struct {
            char *name;
        } identifier;
        struct {
            ASTNode *name;
            ASTNode *value;
        } let;
        struct {
            ASTNode *name;
            ASTNode *value;
        } mutable_let;
        struct {
            ASTNode *left;
            char *operator;
            ASTNode *right;
        } infix;
        struct {
            char *operator;
            ASTNode *operand;
        } unary;
        struct {
            ASTNodeList *parameters;
            ASTNode *body;
        } function;
        struct {
            ASTNodeList *statements;
        } block;
        struct {
            ASTNodeList *elements;
        } list;
        struct {
            ASTNodeList *elements;
        } set;
        struct {
            ASTNodeList *items; // array of dict entries (key-value pairs)
        } dict;
        struct {
            ASTNode *key;
            ASTNode *value;
        } dict_entry;
        struct {
            ASTNode *object;
            ASTNode *index;
        } index;
        struct {
            ASTNode *condition;
            ASTNode *then_branch;
            ASTNode *else_branch;
        } if_expr;
        struct {
            ASTNode *function;
            ASTNodeList *arguments;
        } call;
        struct {
            ASTNode *target;
            ASTNode *value;
        } assignment;
        struct {
            ASTNodeList *functions;
        } function_composition;
        struct {
            ASTNode *initial;
            ASTNodeList *functions;
        } function_thread;
        struct {
            char *value;
        } comment;
        struct {
            ASTNodeList *statements;
        } program;
        struct {
            ASTNode *value;
        } statement_expression;
    } data;
};

typedef struct {
    Lexer *lexer;
    Token current_token;
    bool has_current_token;
} Parser;

Parser *parser_create(Lexer *lexer);
void parser_free(Parser *parser);
ASTNode *parse_program(Parser *parser);
ASTNode *ast_node_create(ASTNodeType type);
void ast_node_free(ASTNode *node);
void print_ast_json(ASTNode *node);

ASTNodeList *ast_node_list_create(void);
void ast_node_list_free(ASTNodeList *list);
void ast_node_list_append(ASTNodeList *list, ASTNode *node);

#endif