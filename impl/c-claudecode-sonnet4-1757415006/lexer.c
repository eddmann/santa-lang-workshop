#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

const char *token_type_to_string(TokenType type) {
    switch (type) {
        case TOK_INT: return "INT";
        case TOK_DEC: return "DEC";
        case TOK_STR: return "STR";
        case TOK_TRUE: return "TRUE";
        case TOK_FALSE: return "FALSE";
        case TOK_NIL: return "NIL";
        case TOK_ID: return "ID";
        case TOK_LET: return "LET";
        case TOK_MUT: return "MUT";
        case TOK_IF: return "IF";
        case TOK_ELSE: return "ELSE";
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_SLASH: return "/";
        case TOK_EQUAL: return "=";
        case TOK_LBRACE: return "{";
        case TOK_RBRACE: return "}";
        case TOK_LBRACKET: return "[";
        case TOK_RBRACKET: return "]";
        case TOK_LPAREN: return "(";
        case TOK_RPAREN: return ")";
        case TOK_GT: return ">";
        case TOK_LT: return "<";
        case TOK_SEMICOLON: return ";";
        case TOK_COMMA: return ",";
        case TOK_PIPE: return "|";
        case TOK_COLON: return ":";
        case TOK_HASH_LBRACE: return "#{";
        case TOK_EQ_EQ: return "==";
        case TOK_NOT_EQ: return "!=";
        case TOK_GT_EQ: return ">=";
        case TOK_LT_EQ: return "<=";
        case TOK_AND_AND: return "&&";
        case TOK_OR_OR: return "||";
        case TOK_PIPE_GT: return "|>";
        case TOK_GT_GT: return ">>";
        case TOK_COMMENT: return "CMT";
        case TOK_EOF: return "EOF";
        case TOK_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

Lexer *lexer_create(char *source, size_t length) {
    Lexer *lexer = malloc(sizeof(Lexer));
    lexer->source = source;
    lexer->length = length;
    lexer->current = 0;
    lexer->line = 1;
    lexer->column = 1;
    return lexer;
}

void lexer_free(Lexer *lexer) {
    free(lexer);
}

static bool is_at_end(Lexer *lexer) {
    return lexer->current >= lexer->length;
}

static char advance(Lexer *lexer) {
    if (is_at_end(lexer)) return '\0';
    char c = lexer->source[lexer->current];
    lexer->current++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static char peek(Lexer *lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->source[lexer->current];
}

static char peek_next(Lexer *lexer) {
    if (lexer->current + 1 >= lexer->length) return '\0';
    return lexer->source[lexer->current + 1];
}

static Token make_token(Lexer *lexer, TokenType type, size_t start) {
    Token token;
    token.type = type;
    token.value = &lexer->source[start];
    token.length = lexer->current - start;
    token.line = lexer->line;
    token.column = lexer->column;
    return token;
}

static void skip_whitespace(Lexer *lexer) {
    while (!is_at_end(lexer)) {
        char c = peek(lexer);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lexer);
        } else {
            break;
        }
    }
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

static Token scan_number(Lexer *lexer, size_t start) {
    bool has_decimal = false;
    
    // Scan integer part (digits and underscores)
    while (!is_at_end(lexer)) {
        char c = peek(lexer);
        if (is_digit(c) || c == '_') {
            advance(lexer);
        } else if (c == '.' && !has_decimal) {
            // Check if there's a digit after the decimal point
            char next = peek_next(lexer);
            if (is_digit(next)) {
                has_decimal = true;
                advance(lexer); // consume '.'
                
                // Scan decimal part
                while (!is_at_end(lexer)) {
                    char dc = peek(lexer);
                    if (is_digit(dc) || dc == '_') {
                        advance(lexer);
                    } else {
                        break;
                    }
                }
            } else {
                break;
            }
        } else {
            break;
        }
    }
    
    return make_token(lexer, has_decimal ? TOK_DEC : TOK_INT, start);
}

static Token scan_string(Lexer *lexer, size_t start) {
    // We've already consumed the opening quote
    while (!is_at_end(lexer) && peek(lexer) != '"') {
        if (peek(lexer) == '\\') {
            advance(lexer); // consume backslash
            if (!is_at_end(lexer)) {
                advance(lexer); // consume escaped character
            }
        } else {
            advance(lexer);
        }
    }
    
    if (is_at_end(lexer)) {
        // Unterminated string - should be an error
        return make_token(lexer, TOK_ERROR, start);
    }
    
    advance(lexer); // consume closing quote
    return make_token(lexer, TOK_STR, start);
}

static Token scan_comment(Lexer *lexer, size_t start) {
    // We've already consumed "//"
    while (!is_at_end(lexer) && peek(lexer) != '\n') {
        advance(lexer);
    }
    return make_token(lexer, TOK_COMMENT, start);
}

static bool is_keyword(const char *text, size_t length, const char *keyword) {
    size_t keyword_len = strlen(keyword);
    return length == keyword_len && memcmp(text, keyword, length) == 0;
}

static Token scan_identifier(Lexer *lexer, size_t start) {
    while (!is_at_end(lexer) && is_alnum(peek(lexer))) {
        advance(lexer);
    }
    
    Token token = make_token(lexer, TOK_ID, start);
    
    // Check if it's a keyword
    if (is_keyword(token.value, token.length, "let")) {
        token.type = TOK_LET;
    } else if (is_keyword(token.value, token.length, "mut")) {
        token.type = TOK_MUT;
    } else if (is_keyword(token.value, token.length, "if")) {
        token.type = TOK_IF;
    } else if (is_keyword(token.value, token.length, "else")) {
        token.type = TOK_ELSE;
    } else if (is_keyword(token.value, token.length, "true")) {
        token.type = TOK_TRUE;
    } else if (is_keyword(token.value, token.length, "false")) {
        token.type = TOK_FALSE;
    } else if (is_keyword(token.value, token.length, "nil")) {
        token.type = TOK_NIL;
    }
    
    return token;
}

Token lexer_next_token(Lexer *lexer) {
    skip_whitespace(lexer);
    
    if (is_at_end(lexer)) {
        return make_token(lexer, TOK_EOF, lexer->current);
    }
    
    size_t start = lexer->current;
    char c = advance(lexer);
    
    // Numbers
    if (is_digit(c)) {
        return scan_number(lexer, start);
    }
    
    // Identifiers and keywords  
    if (is_alpha(c)) {
        return scan_identifier(lexer, start);
    }
    
    // String literals
    if (c == '"') {
        return scan_string(lexer, start);
    }
    
    // Multi-character tokens
    switch (c) {
        case '#':
            if (peek(lexer) == '{') {
                advance(lexer);
                return make_token(lexer, TOK_HASH_LBRACE, start);
            }
            return make_token(lexer, TOK_ERROR, start);
            
        case '=':
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(lexer, TOK_EQ_EQ, start);
            }
            return make_token(lexer, TOK_EQUAL, start);
            
        case '!':
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(lexer, TOK_NOT_EQ, start);
            }
            return make_token(lexer, TOK_ERROR, start);
            
        case '>':
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(lexer, TOK_GT_EQ, start);
            } else if (peek(lexer) == '>') {
                advance(lexer);
                return make_token(lexer, TOK_GT_GT, start);
            }
            return make_token(lexer, TOK_GT, start);
            
        case '<':
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(lexer, TOK_LT_EQ, start);
            }
            return make_token(lexer, TOK_LT, start);
            
        case '&':
            if (peek(lexer) == '&') {
                advance(lexer);
                return make_token(lexer, TOK_AND_AND, start);
            }
            return make_token(lexer, TOK_ERROR, start);
            
        case '|':
            if (peek(lexer) == '|') {
                advance(lexer);
                return make_token(lexer, TOK_OR_OR, start);
            } else if (peek(lexer) == '>') {
                advance(lexer);
                return make_token(lexer, TOK_PIPE_GT, start);
            }
            return make_token(lexer, TOK_PIPE, start);
            
        case '/':
            if (peek(lexer) == '/') {
                advance(lexer);
                return scan_comment(lexer, start);
            }
            return make_token(lexer, TOK_SLASH, start);
            
        // Single character tokens
        case '+': return make_token(lexer, TOK_PLUS, start);
        case '-': return make_token(lexer, TOK_MINUS, start);
        case '*': return make_token(lexer, TOK_STAR, start);
        case '{': return make_token(lexer, TOK_LBRACE, start);
        case '}': return make_token(lexer, TOK_RBRACE, start);
        case '[': return make_token(lexer, TOK_LBRACKET, start);
        case ']': return make_token(lexer, TOK_RBRACKET, start);
        case '(': return make_token(lexer, TOK_LPAREN, start);
        case ')': return make_token(lexer, TOK_RPAREN, start);
        case ';': return make_token(lexer, TOK_SEMICOLON, start);
        case ',': return make_token(lexer, TOK_COMMA, start);
        case ':': return make_token(lexer, TOK_COLON, start);
    }
    
    return make_token(lexer, TOK_ERROR, start);
}

void token_free(Token *token) {
    (void)token;  // Suppress unused parameter warning
    // Since token.value points into the source string, we don't need to free it
    // This function is included for potential future use
}