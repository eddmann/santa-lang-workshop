#!/usr/bin/env python3
"""
Elf-lang lexer implementation.
"""

import re
import json
from enum import Enum
from typing import List, NamedTuple, Optional


class TokenType(Enum):
    # Literals
    INT = "INT"
    DEC = "DEC"
    STR = "STR"
    TRUE = "TRUE"
    FALSE = "FALSE"
    NIL = "NIL"
    
    # Keywords
    LET = "LET"
    MUT = "MUT"
    IF = "IF"
    ELSE = "ELSE"
    
    # Identifiers
    ID = "ID"
    
    # Comments
    CMT = "CMT"
    
    # Two-character operators (must come before single-character ones)
    EQ = "=="
    NE = "!="
    GE = ">="
    LE = "<="
    AND = "&&"
    OR = "||"
    PIPE_OP = "|>"
    COMPOSE = ">>"
    DICT_START = "#{"
    
    # Single-character operators and symbols
    PLUS = "+"
    MINUS = "-"
    MULTIPLY = "*"
    DIVIDE = "/"
    ASSIGN = "="
    GT = ">"
    LT = "<"
    LBRACE = "{"
    RBRACE = "}"
    LBRACKET = "["
    RBRACKET = "]"
    SEMICOLON = ";"
    LPAREN = "("
    RPAREN = ")"
    COMMA = ","
    COLON = ":"
    PIPE = "|"


class Token(NamedTuple):
    type: TokenType
    value: str
    position: int


class Lexer:
    def __init__(self, source: str):
        self.source = source
        self.position = 0
        self.current_char = self.source[0] if source else None
    
    def advance(self):
        """Move to the next character."""
        self.position += 1
        if self.position >= len(self.source):
            self.current_char = None
        else:
            self.current_char = self.source[self.position]
    
    def peek(self, offset: int = 1) -> Optional[str]:
        """Peek at the character at current position + offset."""
        peek_pos = self.position + offset
        if peek_pos >= len(self.source):
            return None
        return self.source[peek_pos]
    
    def skip_whitespace(self):
        """Skip whitespace characters except newlines."""
        while self.current_char is not None and self.current_char in ' \t':
            self.advance()
    
    def read_number(self) -> Token:
        """Read a number (integer or decimal)."""
        start_pos = self.position
        has_decimal = False
        
        while (self.current_char is not None and 
               (self.current_char.isdigit() or self.current_char in '._')):
            if self.current_char == '.':
                if has_decimal:
                    break  # Two decimal points, stop here
                has_decimal = True
            self.advance()
        
        value = self.source[start_pos:self.position]
        token_type = TokenType.DEC if has_decimal else TokenType.INT
        
        return Token(token_type, value, start_pos)
    
    def read_string(self) -> Token:
        """Read a string literal including the quotes."""
        start_pos = self.position
        quote_char = self.current_char
        self.advance()  # Skip opening quote
        
        while self.current_char is not None and self.current_char != quote_char:
            if self.current_char == '\\':
                self.advance()  # Skip escape character
                if self.current_char is not None:
                    self.advance()  # Skip escaped character
            else:
                self.advance()
        
        if self.current_char == quote_char:
            self.advance()  # Skip closing quote
        
        value = self.source[start_pos:self.position]
        return Token(TokenType.STR, value, start_pos)
    
    def read_identifier(self) -> Token:
        """Read an identifier or keyword."""
        start_pos = self.position
        
        while (self.current_char is not None and 
               (self.current_char.isalnum() or self.current_char == '_')):
            self.advance()
        
        value = self.source[start_pos:self.position]
        
        # Check if it's a keyword
        keywords = {
            'let': TokenType.LET,
            'mut': TokenType.MUT,
            'if': TokenType.IF,
            'else': TokenType.ELSE,
            'true': TokenType.TRUE,
            'false': TokenType.FALSE,
            'nil': TokenType.NIL,
        }
        
        token_type = keywords.get(value, TokenType.ID)
        return Token(token_type, value, start_pos)
    
    def read_comment(self) -> Token:
        """Read a line comment."""
        start_pos = self.position
        
        # Read until end of line or end of file
        while self.current_char is not None and self.current_char != '\n':
            self.advance()
        
        value = self.source[start_pos:self.position]
        return Token(TokenType.CMT, value, start_pos)
    
    def get_next_token(self) -> Optional[Token]:
        """Get the next token from the source."""
        while self.current_char is not None:
            # Skip whitespace but not newlines
            if self.current_char in ' \t':
                self.skip_whitespace()
                continue
            
            # Skip newlines
            if self.current_char in '\n\r':
                self.advance()
                continue
            
            # Numbers
            if self.current_char.isdigit():
                return self.read_number()
            
            # Strings
            if self.current_char == '"':
                return self.read_string()
            
            # Identifiers and keywords
            if self.current_char.isalpha() or self.current_char == '_':
                return self.read_identifier()
            
            # Comments
            if self.current_char == '/' and self.peek() == '/':
                return self.read_comment()
            
            # Two-character operators (must check before single-character ones)
            if self.current_char == '=' and self.peek() == '=':
                pos = self.position
                self.advance()
                self.advance()
                return Token(TokenType.EQ, "==", pos)
            
            if self.current_char == '!' and self.peek() == '=':
                pos = self.position
                self.advance()
                self.advance()
                return Token(TokenType.NE, "!=", pos)
            
            if self.current_char == '>' and self.peek() == '=':
                pos = self.position
                self.advance()
                self.advance()
                return Token(TokenType.GE, ">=", pos)
            
            if self.current_char == '<' and self.peek() == '=':
                pos = self.position
                self.advance()
                self.advance()
                return Token(TokenType.LE, "<=", pos)
            
            if self.current_char == '&' and self.peek() == '&':
                pos = self.position
                self.advance()
                self.advance()
                return Token(TokenType.AND, "&&", pos)
            
            if self.current_char == '|' and self.peek() == '|':
                pos = self.position
                self.advance()
                self.advance()
                return Token(TokenType.OR, "||", pos)
            
            if self.current_char == '|' and self.peek() == '>':
                pos = self.position
                self.advance()
                self.advance()
                return Token(TokenType.PIPE_OP, "|>", pos)
            
            if self.current_char == '>' and self.peek() == '>':
                pos = self.position
                self.advance()
                self.advance()
                return Token(TokenType.COMPOSE, ">>", pos)
            
            if self.current_char == '#' and self.peek() == '{':
                pos = self.position
                self.advance()
                self.advance()
                return Token(TokenType.DICT_START, "#{", pos)
            
            # Single-character operators and symbols
            single_char_tokens = {
                '+': TokenType.PLUS,
                '-': TokenType.MINUS,
                '*': TokenType.MULTIPLY,
                '/': TokenType.DIVIDE,
                '=': TokenType.ASSIGN,
                '>': TokenType.GT,
                '<': TokenType.LT,
                '{': TokenType.LBRACE,
                '}': TokenType.RBRACE,
                '[': TokenType.LBRACKET,
                ']': TokenType.RBRACKET,
                ';': TokenType.SEMICOLON,
                '(': TokenType.LPAREN,
                ')': TokenType.RPAREN,
                ',': TokenType.COMMA,
                ':': TokenType.COLON,
                '|': TokenType.PIPE,
            }
            
            if self.current_char in single_char_tokens:
                pos = self.position
                char = self.current_char
                self.advance()
                return Token(single_char_tokens[char], char, pos)
            
            # Unknown character, skip it
            self.advance()
        
        return None
    
    def tokenize(self) -> List[Token]:
        """Tokenize the entire source code."""
        tokens = []
        
        while True:
            token = self.get_next_token()
            if token is None:
                break
            tokens.append(token)
        
        return tokens


def tokenize_to_json_lines(source: str) -> str:
    """Tokenize source and return JSON Lines format."""
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    
    lines = []
    for token in tokens:
        token_dict = {
            "type": token.type.value,
            "value": token.value
        }
        lines.append(json.dumps(token_dict, separators=(',', ':'), ensure_ascii=False))
    
    return '\n'.join(lines)