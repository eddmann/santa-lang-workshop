#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "evaluator.h"

static char *read_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    fread(buffer, 1, size, file);
    buffer[size] = '\0';
    fclose(file);
    
    if (out_size) *out_size = size;
    return buffer;
}

static void print_token_json(Token token) {
    printf("{\"type\":\"%s\",\"value\":\"", token_type_to_string(token.type));
    
    // Print the value, escaping necessary characters for JSON
    for (size_t i = 0; i < token.length; i++) {
        char c = token.value[i];
        switch (c) {
            case '"': printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\n': printf("\\n"); break;
            case '\t': printf("\\t"); break;
            case '\r': printf("\\r"); break;
            default: putchar(c); break;
        }
    }
    
    printf("\"}\n");
}

static int run_tokens(const char *filename) {
    size_t size;
    char *source = read_file(filename, &size);
    if (!source) {
        fprintf(stderr, "Error reading file: %s\n", filename);
        return 1;
    }
    
    Lexer *lexer = lexer_create(source, size);
    
    while (true) {
        Token token = lexer_next_token(lexer);
        if (token.type == TOK_EOF) {
            break;
        }
        if (token.type == TOK_ERROR) {
            fprintf(stderr, "Lexer error at line %d, column %d\n", token.line, token.column);
            free(source);
            lexer_free(lexer);
            return 1;
        }
        
        print_token_json(token);
    }
    
    free(source);
    lexer_free(lexer);
    return 0;
}

static int run_ast(const char *filename) {
    size_t size;
    char *source = read_file(filename, &size);
    if (!source) {
        fprintf(stderr, "Error reading file: %s\n", filename);
        return 1;
    }
    
    Lexer *lexer = lexer_create(source, size);
    Parser *parser = parser_create(lexer);
    
    ASTNode *program = parse_program(parser);
    print_ast_json(program);
    
    ast_node_free(program);
    parser_free(parser);
    lexer_free(lexer);
    free(source);
    return 0;
}

static int run_program(const char *filename) {
    size_t size;
    char *source = read_file(filename, &size);
    if (!source) {
        fprintf(stderr, "Error reading file: %s\n", filename);
        return 1;
    }
    
    Lexer *lexer = lexer_create(source, size);
    Parser *parser = parser_create(lexer);
    
    ASTNode *program = parse_program(parser);
    if (!program) {
        parser_free(parser);
        lexer_free(lexer);
        free(source);
        return 1;
    }
    
    Evaluator *evaluator = evaluator_create();
    register_builtin_functions(evaluator);
    
    Value *result = evaluate(evaluator, program);
    
    if (result->type == VAL_ERROR) {
        print_value(result);
        printf("\n");
        evaluator_free(evaluator);
        value_free(result);
        ast_node_free(program);
        parser_free(parser);
        lexer_free(lexer);
        free(source);
        return 1;
    }
    
    // Print the final result of the program
    print_value(result);
    printf(" \n");
    
    value_free(result);
    evaluator_free(evaluator);
    ast_node_free(program);
    parser_free(parser);
    lexer_free(lexer);
    free(source);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [file]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "tokens") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s tokens <file>\n", argv[0]);
            return 1;
        }
        return run_tokens(argv[2]);
    } else if (strcmp(argv[1], "ast") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s ast <file>\n", argv[0]);
            return 1;
        }
        return run_ast(argv[2]);
    } else {
        // Regular run mode - first arg is the file
        return run_program(argv[1]);
    }
}