#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <stdbool.h>

typedef enum {
    // Literals
    TOK_INT,
    TOK_DEC,
    TOK_STR,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NIL,
    
    // Identifiers and keywords
    TOK_ID,
    TOK_LET,
    TOK_MUT,
    TOK_IF,
    TOK_ELSE,
    
    // Single character tokens
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_EQUAL,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_GT,
    TOK_LT,
    TOK_SEMICOLON,
    TOK_COMMA,
    TOK_PIPE,
    TOK_COLON,
    
    // Multi-character tokens
    TOK_HASH_LBRACE,    // #{
    TOK_EQ_EQ,          // ==
    TOK_NOT_EQ,         // !=
    TOK_GT_EQ,          // >=
    TOK_LT_EQ,          // <=
    TOK_AND_AND,        // &&
    TOK_OR_OR,          // ||
    TOK_PIPE_GT,        // |>
    TOK_GT_GT,          // >>
    
    // Comments
    TOK_COMMENT,
    
    // Special
    TOK_EOF,
    TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char *value;    // exact slice from source
    size_t length;
    int line;
    int column;
} Token;

typedef struct {
    char *source;
    size_t length;
    size_t current;
    int line;
    int column;
} Lexer;

const char *token_type_to_string(TokenType type);
Lexer *lexer_create(char *source, size_t length);
void lexer_free(Lexer *lexer);
Token lexer_next_token(Lexer *lexer);
void token_free(Token *token);

#endif