#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Precedence levels
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,       // ||
    PREC_AND,      // &&
    PREC_EQUALITY, // == !=
    PREC_COMPARISON, // > < >= <=
    PREC_PIPE,     // |>
    PREC_COMPOSE,  // >>
    PREC_TERM,     // + -
    PREC_FACTOR,   // * /
    PREC_UNARY,    // -
    PREC_CALL,     // () []
    PREC_PRIMARY
} Precedence;

static ASTNode *parse_expression(Parser *parser);
static ASTNode *parse_statement(Parser *parser);
static ASTNode *parse_primary(Parser *parser);
static ASTNode *parse_precedence(Parser *parser, Precedence precedence);

Parser *parser_create(Lexer *lexer) {
    Parser *parser = malloc(sizeof(Parser));
    parser->lexer = lexer;
    parser->has_current_token = false;
    return parser;
}

void parser_free(Parser *parser) {
    free(parser);
}

static Token current_token(Parser *parser) {
    if (!parser->has_current_token) {
        parser->current_token = lexer_next_token(parser->lexer);
        parser->has_current_token = true;
    }
    return parser->current_token;
}

static Token advance(Parser *parser) {
    Token prev = current_token(parser);
    parser->has_current_token = false;
    return prev;
}

static bool check(Parser *parser, TokenType type) {
    return current_token(parser).type == type;
}

static bool match(Parser *parser, TokenType type) {
    if (check(parser, type)) {
        advance(parser);
        return true;
    }
    return false;
}

static Token consume(Parser *parser, TokenType type, const char *message) {
    if (check(parser, type)) {
        return advance(parser);
    }
    
    // Error handling - for now, just exit
    fprintf(stderr, "Parse error: %s\n", message);
    exit(1);
}

ASTNodeList *ast_node_list_create(void) {
    ASTNodeList *list = malloc(sizeof(ASTNodeList));
    list->nodes = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

void ast_node_list_append(ASTNodeList *list, ASTNode *node) {
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->nodes = realloc(list->nodes, new_capacity * sizeof(ASTNode*));
        list->capacity = new_capacity;
    }
    list->nodes[list->count++] = node;
}

void ast_node_list_free(ASTNodeList *list) {
    if (list->nodes) {
        for (size_t i = 0; i < list->count; i++) {
            ast_node_free(list->nodes[i]);
        }
        free(list->nodes);
    }
    free(list);
}

static char *copy_token_value(Token token) {
    char *value = malloc(token.length + 1);
    memcpy(value, token.value, token.length);
    value[token.length] = '\0';
    return value;
}

ASTNode *ast_node_create(ASTNodeType type) {
    ASTNode *node = calloc(1, sizeof(ASTNode));
    node->type = type;
    return node;
}

void ast_node_free(ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_INTEGER:
            free(node->data.integer.value);
            break;
        case AST_DECIMAL:
            free(node->data.decimal.value);
            break;
        case AST_STRING:
            free(node->data.string.value);
            break;
        case AST_IDENTIFIER:
            free(node->data.identifier.name);
            break;
        case AST_LET:
            ast_node_free(node->data.let.name);
            ast_node_free(node->data.let.value);
            break;
        case AST_MUTABLE_LET:
            ast_node_free(node->data.mutable_let.name);
            ast_node_free(node->data.mutable_let.value);
            break;
        case AST_INFIX:
            ast_node_free(node->data.infix.left);
            ast_node_free(node->data.infix.right);
            free(node->data.infix.operator);
            break;
        case AST_UNARY:
            ast_node_free(node->data.unary.operand);
            free(node->data.unary.operator);
            break;
        case AST_FUNCTION:
            ast_node_list_free(node->data.function.parameters);
            ast_node_free(node->data.function.body);
            break;
        case AST_BLOCK:
            ast_node_list_free(node->data.block.statements);
            break;
        case AST_LIST:
            ast_node_list_free(node->data.list.elements);
            break;
        case AST_SET:
            ast_node_list_free(node->data.set.elements);
            break;
        case AST_DICT:
            ast_node_list_free(node->data.dict.items);
            break;
        case AST_DICT_ENTRY:
            ast_node_free(node->data.dict_entry.key);
            ast_node_free(node->data.dict_entry.value);
            break;
        case AST_INDEX:
            ast_node_free(node->data.index.object);
            ast_node_free(node->data.index.index);
            break;
        case AST_IF:
            ast_node_free(node->data.if_expr.condition);
            ast_node_free(node->data.if_expr.then_branch);
            ast_node_free(node->data.if_expr.else_branch);
            break;
        case AST_CALL:
            ast_node_free(node->data.call.function);
            ast_node_list_free(node->data.call.arguments);
            break;
        case AST_ASSIGNMENT:
            ast_node_free(node->data.assignment.target);
            ast_node_free(node->data.assignment.value);
            break;
        case AST_FUNCTION_COMPOSITION:
            ast_node_list_free(node->data.function_composition.functions);
            break;
        case AST_FUNCTION_THREAD:
            ast_node_free(node->data.function_thread.initial);
            ast_node_list_free(node->data.function_thread.functions);
            break;
        case AST_COMMENT:
            free(node->data.comment.value);
            break;
        case AST_PROGRAM:
            ast_node_list_free(node->data.program.statements);
            break;
        case AST_STATEMENT_EXPRESSION:
            ast_node_free(node->data.statement_expression.value);
            break;
        default:
            break;
    }
    free(node);
}

static ASTNode *parse_primary(Parser *parser) {
    Token token = advance(parser);
    
    switch (token.type) {
        case TOK_INT: {
            ASTNode *node = ast_node_create(AST_INTEGER);
            node->data.integer.value = copy_token_value(token);
            return node;
        }
        case TOK_DEC: {
            ASTNode *node = ast_node_create(AST_DECIMAL);
            node->data.decimal.value = copy_token_value(token);
            return node;
        }
        case TOK_STR: {
            ASTNode *node = ast_node_create(AST_STRING);
            // Remove surrounding quotes and process escape sequences
            size_t len = token.length - 2; // excluding quotes
            char *value = malloc(len + 1);
            size_t write_pos = 0;
            
            for (size_t i = 1; i < token.length - 1; i++) { // skip outer quotes
                if (token.value[i] == '\\' && i + 1 < token.length - 1) {
                    char escaped = token.value[i + 1];
                    switch (escaped) {
                        case 'n': value[write_pos++] = '\n'; break;
                        case 't': value[write_pos++] = '\t'; break;
                        case 'r': value[write_pos++] = '\r'; break;
                        case '\\': value[write_pos++] = '\\'; break;
                        case '"': value[write_pos++] = '"'; break;
                        default: 
                            // Unknown escape, keep as-is
                            value[write_pos++] = '\\';
                            value[write_pos++] = escaped;
                            break;
                    }
                    i++; // skip the escaped character
                } else {
                    value[write_pos++] = token.value[i];
                }
            }
            value[write_pos] = '\0';
            node->data.string.value = value;
            return node;
        }
        case TOK_TRUE: {
            ASTNode *node = ast_node_create(AST_BOOLEAN);
            node->data.boolean.value = true;
            return node;
        }
        case TOK_FALSE: {
            ASTNode *node = ast_node_create(AST_BOOLEAN);
            node->data.boolean.value = false;
            return node;
        }
        case TOK_NIL: {
            return ast_node_create(AST_NIL);
        }
        case TOK_ID: {
            ASTNode *node = ast_node_create(AST_IDENTIFIER);
            node->data.identifier.name = copy_token_value(token);
            return node;
        }
        case TOK_MINUS: {
            // Unary minus
            ASTNode *operand = parse_precedence(parser, PREC_UNARY);
            ASTNode *node = ast_node_create(AST_UNARY);
            node->data.unary.operator = copy_token_value(token);
            node->data.unary.operand = operand;
            return node;
        }
        // Operators used as function names
        case TOK_PLUS:
        case TOK_STAR:
        case TOK_SLASH: {
            ASTNode *node = ast_node_create(AST_IDENTIFIER);
            node->data.identifier.name = copy_token_value(token);
            return node;
        }
        case TOK_LPAREN: {
            ASTNode *expr = parse_expression(parser);
            consume(parser, TOK_RPAREN, "Expected ')' after expression");
            return expr;
        }
        case TOK_LBRACKET: {
            // List literal
            ASTNode *node = ast_node_create(AST_LIST);
            node->data.list.elements = ast_node_list_create();
            
            if (!check(parser, TOK_RBRACKET)) {
                do {
                    ASTNode *element = parse_expression(parser);
                    ast_node_list_append(node->data.list.elements, element);
                } while (match(parser, TOK_COMMA));
            }
            
            consume(parser, TOK_RBRACKET, "Expected ']' after list elements");
            return node;
        }
        case TOK_LBRACE: {
            // Set literal
            ASTNode *node = ast_node_create(AST_SET);
            node->data.set.elements = ast_node_list_create();
            
            if (!check(parser, TOK_RBRACE)) {
                do {
                    ASTNode *element = parse_expression(parser);
                    ast_node_list_append(node->data.set.elements, element);
                } while (match(parser, TOK_COMMA));
            }
            
            consume(parser, TOK_RBRACE, "Expected '}' after set elements");
            return node;
        }
        case TOK_HASH_LBRACE: {
            // Dictionary literal
            ASTNode *node = ast_node_create(AST_DICT);
            node->data.dict.items = ast_node_list_create();
            
            if (!check(parser, TOK_RBRACE)) {
                do {
                    ASTNode *key = parse_expression(parser);
                    consume(parser, TOK_COLON, "Expected ':' after dictionary key");
                    ASTNode *value = parse_expression(parser);
                    
                    // Create dict entry node
                    ASTNode *entry = ast_node_create(AST_DICT_ENTRY);
                    entry->data.dict_entry.key = key;
                    entry->data.dict_entry.value = value;
                    
                    ast_node_list_append(node->data.dict.items, entry);
                } while (match(parser, TOK_COMMA));
            }
            
            consume(parser, TOK_RBRACE, "Expected '}' after dictionary entries");
            return node;
        }
        case TOK_OR_OR: {
            // Empty parameter function || body
            ASTNode *node = ast_node_create(AST_FUNCTION);
            node->data.function.parameters = ast_node_list_create(); // Empty parameters
            
            // Parse body - could be expression or block
            if (check(parser, TOK_LBRACE)) {
                // Block body
                advance(parser); // consume {
                ASTNode *block = ast_node_create(AST_BLOCK);
                block->data.block.statements = ast_node_list_create();
                
                while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
                    ASTNode *stmt = parse_statement(parser);
                    ast_node_list_append(block->data.block.statements, stmt);
                }
                
                consume(parser, TOK_RBRACE, "Expected '}' after block");
                node->data.function.body = block;
            } else {
                // Expression body
                ASTNode *expr = parse_expression(parser);
                ASTNode *block = ast_node_create(AST_BLOCK);
                block->data.block.statements = ast_node_list_create();
                
                ASTNode *expr_stmt = ast_node_create(AST_STATEMENT_EXPRESSION);
                expr_stmt->data.statement_expression.value = expr;
                ast_node_list_append(block->data.block.statements, expr_stmt);
                
                node->data.function.body = block;
            }
            
            return node;
        }
        case TOK_PIPE: {
            // Function literal |x, y| body
            ASTNode *node = ast_node_create(AST_FUNCTION);
            node->data.function.parameters = ast_node_list_create();
            
            // Parse parameters
            if (!check(parser, TOK_PIPE)) {
                do {
                    Token param = consume(parser, TOK_ID, "Expected parameter name");
                    ASTNode *param_node = ast_node_create(AST_IDENTIFIER);
                    param_node->data.identifier.name = copy_token_value(param);
                    ast_node_list_append(node->data.function.parameters, param_node);
                } while (match(parser, TOK_COMMA));
            }
            
            consume(parser, TOK_PIPE, "Expected '|' after function parameters");
            
            // Parse body - could be expression or block
            if (check(parser, TOK_LBRACE)) {
                // Block body
                advance(parser); // consume {
                ASTNode *block = ast_node_create(AST_BLOCK);
                block->data.block.statements = ast_node_list_create();
                
                while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
                    ASTNode *stmt = parse_statement(parser);
                    ast_node_list_append(block->data.block.statements, stmt);
                }
                
                consume(parser, TOK_RBRACE, "Expected '}' after block");
                node->data.function.body = block;
            } else {
                // Expression body - wrap in a block
                ASTNode *block = ast_node_create(AST_BLOCK);
                block->data.block.statements = ast_node_list_create();
                
                ASTNode *expr = parse_expression(parser);
                ASTNode *expr_stmt = ast_node_create(AST_STATEMENT_EXPRESSION);
                expr_stmt->data.statement_expression.value = expr;
                ast_node_list_append(block->data.block.statements, expr_stmt);
                
                node->data.function.body = block;
            }
            
            return node;
        }
        case TOK_IF: {
            // Parse if expression: if condition { then_body } else { else_body }
            ASTNode *condition = parse_expression(parser);
            consume(parser, TOK_LBRACE, "Expected '{' after if condition");
            
            // Parse then block
            ASTNode *then_block = parse_expression(parser);
            consume(parser, TOK_RBRACE, "Expected '}' after then block");
            
            ASTNode *else_block = NULL;
            if (match(parser, TOK_ELSE)) {
                consume(parser, TOK_LBRACE, "Expected '{' after else");
                else_block = parse_expression(parser);
                consume(parser, TOK_RBRACE, "Expected '}' after else block");
            }
            
            ASTNode *node = ast_node_create(AST_IF);
            node->data.if_expr.condition = condition;
            node->data.if_expr.then_branch = then_block;
            node->data.if_expr.else_branch = else_block;
            return node;
        }
        default:
            fprintf(stderr, "Unexpected token in primary: %s\n", token_type_to_string(token.type));
            exit(1);
    }
}

static Precedence get_precedence(TokenType type) {
    switch (type) {
        case TOK_EQUAL: return PREC_ASSIGNMENT;
        case TOK_OR_OR: return PREC_OR;
        case TOK_AND_AND: return PREC_AND;
        case TOK_EQ_EQ:
        case TOK_NOT_EQ: return PREC_EQUALITY;
        case TOK_GT:
        case TOK_LT:
        case TOK_GT_EQ:
        case TOK_LT_EQ: return PREC_COMPARISON;
        case TOK_PIPE_GT: return PREC_PIPE;
        case TOK_GT_GT: return PREC_COMPOSE;
        case TOK_PLUS:
        case TOK_MINUS: return PREC_TERM;
        case TOK_STAR:
        case TOK_SLASH: return PREC_FACTOR;
        case TOK_LPAREN:
        case TOK_LBRACKET: return PREC_CALL;
        default: return PREC_NONE;
    }
}

static ASTNode *parse_infix(Parser *parser, ASTNode *left) {
    Token op_token = advance(parser);
    Precedence precedence = get_precedence(op_token.type);
    
    // Handle assignment specially (right-associative)
    if (op_token.type == TOK_EQUAL) {
        ASTNode *right = parse_precedence(parser, precedence); // same precedence for right-associativity
        ASTNode *node = ast_node_create(AST_ASSIGNMENT);
        node->data.assignment.target = left;
        node->data.assignment.value = right;
        return node;
    }
    
    ASTNode *right = parse_precedence(parser, (Precedence)(precedence + 1));
    
    ASTNode *node = ast_node_create(AST_INFIX);
    node->data.infix.left = left;
    node->data.infix.operator = copy_token_value(op_token);
    node->data.infix.right = right;
    
    return node;
}

static ASTNode *parse_call(Parser *parser, ASTNode *left) {
    ASTNode *node = ast_node_create(AST_CALL);
    node->data.call.function = left;
    node->data.call.arguments = ast_node_list_create();
    
    if (!check(parser, TOK_RPAREN)) {
        do {
            ASTNode *arg = parse_expression(parser);
            ast_node_list_append(node->data.call.arguments, arg);
        } while (match(parser, TOK_COMMA));
    }
    
    consume(parser, TOK_RPAREN, "Expected ')' after arguments");
    return node;
}

static ASTNode *parse_index(Parser *parser, ASTNode *left) {
    ASTNode *index_expr = parse_expression(parser);
    consume(parser, TOK_RBRACKET, "Expected ']' after index");
    
    ASTNode *node = ast_node_create(AST_INDEX);
    node->data.index.object = left;
    node->data.index.index = index_expr;
    
    return node;
}

static ASTNode *parse_function_composition(Parser *parser, ASTNode *left) {
    ASTNode *node = ast_node_create(AST_FUNCTION_COMPOSITION);
    node->data.function_composition.functions = ast_node_list_create();
    
    // Add the left side function
    ast_node_list_append(node->data.function_composition.functions, left);
    
    // Consume the first >> token that was detected in parse_precedence
    advance(parser);
    
    // Parse the right side of the first composition
    ASTNode *right = parse_precedence(parser, (Precedence)(PREC_COMPOSE + 1));
    ast_node_list_append(node->data.function_composition.functions, right);
    
    // Parse any additional composed functions using >> operator
    while (match(parser, TOK_GT_GT)) {
        right = parse_precedence(parser, (Precedence)(PREC_COMPOSE + 1));
        ast_node_list_append(node->data.function_composition.functions, right);
    }
    
    return node;
}

static ASTNode *parse_function_thread(Parser *parser, ASTNode *left) {
    ASTNode *node = ast_node_create(AST_FUNCTION_THREAD);
    node->data.function_thread.initial = left;
    node->data.function_thread.functions = ast_node_list_create();
    
    // Consume the first |> token that was detected in parse_precedence
    advance(parser);
    
    // Parse the right side of the first thread
    ASTNode *right = parse_precedence(parser, (Precedence)(PREC_PIPE + 1));
    ast_node_list_append(node->data.function_thread.functions, right);
    
    // Parse any additional threaded functions using |> operator
    while (match(parser, TOK_PIPE_GT)) {
        right = parse_precedence(parser, (Precedence)(PREC_PIPE + 1));
        ast_node_list_append(node->data.function_thread.functions, right);
    }
    
    return node;
}

static ASTNode *parse_precedence(Parser *parser, Precedence precedence) {
    ASTNode *left = parse_primary(parser);
    
    while (precedence <= get_precedence(current_token(parser).type)) {
        TokenType op_type = current_token(parser).type;
        
        if (op_type == TOK_LPAREN) {
            advance(parser);
            left = parse_call(parser, left);
        } else if (op_type == TOK_LBRACKET) {
            advance(parser);
            left = parse_index(parser, left);
        } else if (op_type == TOK_GT_GT) {
            left = parse_function_composition(parser, left);
        } else if (op_type == TOK_PIPE_GT) {
            left = parse_function_thread(parser, left);
        } else {
            left = parse_infix(parser, left);
        }
    }
    
    return left;
}

static ASTNode *parse_expression(Parser *parser) {
    return parse_precedence(parser, PREC_ASSIGNMENT);
}

static ASTNode *parse_let_statement(Parser *parser) {
    advance(parser); // consume 'let'
    
    bool is_mutable = match(parser, TOK_MUT);
    
    Token name_token = consume(parser, TOK_ID, "Expected variable name");
    consume(parser, TOK_EQUAL, "Expected '=' after variable name");
    
    ASTNode *value = parse_expression(parser);
    
    ASTNode *name_node = ast_node_create(AST_IDENTIFIER);
    name_node->data.identifier.name = copy_token_value(name_token);
    
    ASTNode *node;
    if (is_mutable) {
        node = ast_node_create(AST_MUTABLE_LET);
        node->data.mutable_let.name = name_node;
        node->data.mutable_let.value = value;
    } else {
        node = ast_node_create(AST_LET);
        node->data.let.name = name_node;
        node->data.let.value = value;
    }
    
    return node;
}

static ASTNode *parse_if_statement(Parser *parser) {
    advance(parser); // consume 'if'
    
    ASTNode *condition = parse_expression(parser);
    consume(parser, TOK_LBRACE, "Expected '{' after if condition");
    
    // Parse then block
    ASTNode *then_block = ast_node_create(AST_BLOCK);
    then_block->data.block.statements = ast_node_list_create();
    
    while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
        ASTNode *stmt = parse_statement(parser);
        ast_node_list_append(then_block->data.block.statements, stmt);
    }
    
    consume(parser, TOK_RBRACE, "Expected '}' after then block");
    
    ASTNode *else_block = NULL;
    if (match(parser, TOK_ELSE)) {
        consume(parser, TOK_LBRACE, "Expected '{' after else");
        
        else_block = ast_node_create(AST_BLOCK);
        else_block->data.block.statements = ast_node_list_create();
        
        while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
            ASTNode *stmt = parse_statement(parser);
            ast_node_list_append(else_block->data.block.statements, stmt);
        }
        
        consume(parser, TOK_RBRACE, "Expected '}' after else block");
    }
    
    ASTNode *node = ast_node_create(AST_IF);
    node->data.if_expr.condition = condition;
    node->data.if_expr.then_branch = then_block;
    node->data.if_expr.else_branch = else_block;
    
    return node;
}

static ASTNode *parse_statement(Parser *parser) {
    if (check(parser, TOK_COMMENT)) {
        Token comment_token = advance(parser);
        ASTNode *comment = ast_node_create(AST_COMMENT);
        comment->data.comment.value = copy_token_value(comment_token);
        return comment;
    } else if (check(parser, TOK_LET)) {
        ASTNode *let_stmt = parse_let_statement(parser);
        match(parser, TOK_SEMICOLON); // optional semicolon
        
        ASTNode *stmt = ast_node_create(AST_STATEMENT_EXPRESSION);
        stmt->data.statement_expression.value = let_stmt;
        return stmt;
    } else if (check(parser, TOK_IF)) {
        ASTNode *if_stmt = parse_if_statement(parser);
        match(parser, TOK_SEMICOLON); // optional semicolon
        
        ASTNode *stmt = ast_node_create(AST_STATEMENT_EXPRESSION);
        stmt->data.statement_expression.value = if_stmt;
        return stmt;
    } else {
        // Expression statement
        ASTNode *expr = parse_expression(parser);
        match(parser, TOK_SEMICOLON); // optional semicolon
        
        ASTNode *stmt = ast_node_create(AST_STATEMENT_EXPRESSION);
        stmt->data.statement_expression.value = expr;
        return stmt;
    }
}

ASTNode *parse_program(Parser *parser) {
    ASTNode *program = ast_node_create(AST_PROGRAM);
    program->data.program.statements = ast_node_list_create();
    
    while (!check(parser, TOK_EOF)) {
        ASTNode *stmt = parse_statement(parser);
        ast_node_list_append(program->data.program.statements, stmt);
    }
    
    return program;
}

// JSON printing functions
static void print_json_string(const char *str) {
    printf("\"");
    for (const char *c = str; *c; c++) {
        switch (*c) {
            case '"': printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\n': printf("\\n"); break;
            case '\t': printf("\\t"); break;
            case '\r': printf("\\r"); break;
            default: printf("%c", *c); break;
        }
    }
    printf("\"");
}

static void print_ast_json_internal(ASTNode *node, int indent);

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

static void print_node_list_json(ASTNodeList *list, int indent) {
    if (list->count == 0) {
        printf("[]");
        return;
    }
    
    printf("[\n");
    for (size_t i = 0; i < list->count; i++) {
        print_indent(indent + 1);
        print_ast_json_internal(list->nodes[i], indent + 1);
        if (i < list->count - 1) {
            printf(",");
        }
        printf("\n");
    }
    print_indent(indent);
    printf("]");
}

static void print_ast_json_internal(ASTNode *node, int indent) {
    printf("{\n");
    
    switch (node->type) {
        case AST_INTEGER:
            print_indent(indent + 1);
            printf("\"type\": \"Integer\",\n");
            print_indent(indent + 1);
            printf("\"value\": ");
            print_json_string(node->data.integer.value);
            printf("\n");
            break;
            
        case AST_DECIMAL:
            print_indent(indent + 1);
            printf("\"type\": \"Decimal\",\n");
            print_indent(indent + 1);
            printf("\"value\": ");
            print_json_string(node->data.decimal.value);
            printf("\n");
            break;
            
        case AST_STRING:
            print_indent(indent + 1);
            printf("\"type\": \"String\",\n");
            print_indent(indent + 1);
            printf("\"value\": ");
            print_json_string(node->data.string.value);
            printf("\n");
            break;
            
        case AST_BOOLEAN:
            print_indent(indent + 1);
            printf("\"type\": \"Boolean\",\n");
            print_indent(indent + 1);
            printf("\"value\": %s\n", node->data.boolean.value ? "true" : "false");
            break;
            
        case AST_NIL:
            print_indent(indent + 1);
            printf("\"type\": \"Nil\"\n");
            break;
            
        case AST_IDENTIFIER:
            print_indent(indent + 1);
            printf("\"name\": ");
            print_json_string(node->data.identifier.name);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"Identifier\"\n");
            break;
            
        case AST_LET:
            print_indent(indent + 1);
            printf("\"name\": ");
            print_ast_json_internal(node->data.let.name, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"Let\",\n");
            print_indent(indent + 1);
            printf("\"value\": ");
            print_ast_json_internal(node->data.let.value, indent + 1);
            printf("\n");
            break;
            
        case AST_MUTABLE_LET:
            print_indent(indent + 1);
            printf("\"name\": ");
            print_ast_json_internal(node->data.mutable_let.name, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"MutableLet\",\n");
            print_indent(indent + 1);
            printf("\"value\": ");
            print_ast_json_internal(node->data.mutable_let.value, indent + 1);
            printf("\n");
            break;
            
        case AST_INFIX:
            print_indent(indent + 1);
            printf("\"left\": ");
            print_ast_json_internal(node->data.infix.left, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"operator\": ");
            print_json_string(node->data.infix.operator);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"right\": ");
            print_ast_json_internal(node->data.infix.right, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"Infix\"\n");
            break;
            
        case AST_UNARY:
            print_indent(indent + 1);
            printf("\"operand\": ");
            print_ast_json_internal(node->data.unary.operand, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"operator\": ");
            print_json_string(node->data.unary.operator);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"Unary\"\n");
            break;
            
        case AST_FUNCTION:
            print_indent(indent + 1);
            printf("\"body\": ");
            print_ast_json_internal(node->data.function.body, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"parameters\": ");
            print_node_list_json(node->data.function.parameters, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"Function\"\n");
            break;
            
        case AST_BLOCK:
            print_indent(indent + 1);
            printf("\"statements\": ");
            print_node_list_json(node->data.block.statements, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"Block\"\n");
            break;
            
        case AST_PROGRAM:
            print_indent(indent + 1);
            printf("\"statements\": ");
            print_node_list_json(node->data.program.statements, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"Program\"\n");
            break;
            
        case AST_LIST:
            print_indent(indent + 1);
            printf("\"items\": ");
            print_node_list_json(node->data.list.elements, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"List\"\n");
            break;
            
        case AST_SET:
            print_indent(indent + 1);
            printf("\"items\": ");
            print_node_list_json(node->data.set.elements, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"Set\"\n");
            break;
            
        case AST_DICT:
            print_indent(indent + 1);
            printf("\"items\": ");
            print_node_list_json(node->data.dict.items, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"Dictionary\"\n");
            break;
            
        case AST_DICT_ENTRY:
            print_indent(indent + 1);
            printf("\"key\": ");
            print_ast_json_internal(node->data.dict_entry.key, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"value\": ");
            print_ast_json_internal(node->data.dict_entry.value, indent + 1);
            printf("\n");
            break;
            
        case AST_INDEX:
            print_indent(indent + 1);
            printf("\"index\": ");
            print_ast_json_internal(node->data.index.index, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"left\": ");
            print_ast_json_internal(node->data.index.object, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"Index\"\n");
            break;
            
        case AST_IF:
            if (node->data.if_expr.else_branch) {
                print_indent(indent + 1);
                printf("\"alternative\": ");
                print_ast_json_internal(node->data.if_expr.else_branch, indent + 1);
                printf(",\n");
            }
            print_indent(indent + 1);
            printf("\"condition\": ");
            print_ast_json_internal(node->data.if_expr.condition, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"consequence\": ");
            print_ast_json_internal(node->data.if_expr.then_branch, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"If\"\n");
            break;
            
        case AST_CALL:
            print_indent(indent + 1);
            printf("\"arguments\": ");
            print_node_list_json(node->data.call.arguments, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"function\": ");
            print_ast_json_internal(node->data.call.function, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"Call\"\n");
            break;
            
        case AST_ASSIGNMENT:
            print_indent(indent + 1);
            printf("\"name\": ");
            print_ast_json_internal(node->data.assignment.target, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"Assignment\",\n");
            print_indent(indent + 1);
            printf("\"value\": ");
            print_ast_json_internal(node->data.assignment.value, indent + 1);
            printf("\n");
            break;
            
        case AST_STATEMENT_EXPRESSION:
            print_indent(indent + 1);
            printf("\"type\": \"Expression\",\n");
            print_indent(indent + 1);
            printf("\"value\": ");
            print_ast_json_internal(node->data.statement_expression.value, indent + 1);
            printf("\n");
            break;
            
        case AST_FUNCTION_COMPOSITION:
            print_indent(indent + 1);
            printf("\"functions\": ");
            print_node_list_json(node->data.function_composition.functions, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"FunctionComposition\"\n");
            break;
            
        case AST_FUNCTION_THREAD:
            print_indent(indent + 1);
            printf("\"functions\": ");
            print_node_list_json(node->data.function_thread.functions, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"initial\": ");
            print_ast_json_internal(node->data.function_thread.initial, indent + 1);
            printf(",\n");
            print_indent(indent + 1);
            printf("\"type\": \"FunctionThread\"\n");
            break;
            
        case AST_COMMENT:
            print_indent(indent + 1);
            printf("\"type\": \"Comment\",\n");
            print_indent(indent + 1);
            printf("\"value\": ");
            print_json_string(node->data.comment.value);
            printf("\n");
            break;
            
        default:
            print_indent(indent + 1);
            printf("\"type\": \"Unknown\"\n");
            break;
    }
    
    print_indent(indent);
    printf("}");
}

void print_ast_json(ASTNode *node) {
    print_ast_json_internal(node, 0);
    printf("\n");
}