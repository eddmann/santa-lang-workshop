#!/usr/bin/env python3
"""
Elf-lang parser implementation.
"""

import json
from typing import List, Dict, Any, Optional, Union
from lexer import Lexer, Token, TokenType


class Parser:
    def __init__(self, source: str):
        self.lexer = Lexer(source)
        self.tokens = self.lexer.tokenize()
        self.position = 0
        self.current_token = self.tokens[0] if self.tokens else None
    
    def advance(self):
        """Move to the next token."""
        self.position += 1
        if self.position >= len(self.tokens):
            self.current_token = None
        else:
            self.current_token = self.tokens[self.position]
    
    def peek(self, offset: int = 1) -> Optional[Token]:
        """Peek at the token at current position + offset."""
        peek_pos = self.position + offset
        if peek_pos >= len(self.tokens):
            return None
        return self.tokens[peek_pos]
    
    def expect(self, expected_type: TokenType) -> Token:
        """Expect a specific token type and consume it."""
        if self.current_token is None or self.current_token.type != expected_type:
            raise SyntaxError(f"Expected {expected_type}, got {self.current_token}")
        token = self.current_token
        self.advance()
        return token
    
    def skip_comments(self):
        """Skip any comment tokens."""
        while self.current_token and self.current_token.type == TokenType.CMT:
            self.advance()
    
    def parse(self) -> Dict[str, Any]:
        """Parse the program."""
        statements = []
        
        while self.current_token is not None:
            stmt = self.parse_statement()
            if stmt:
                statements.append(stmt)
        
        return {
            "statements": statements,
            "type": "Program"
        }
    
    def parse_statement(self) -> Optional[Dict[str, Any]]:
        """Parse a statement."""
        if self.current_token is None:
            return None
        
        # Handle comments
        if self.current_token.type == TokenType.CMT:
            comment_value = self.current_token.value
            self.advance()
            return {
                "type": "Comment",
                "value": comment_value
            }
        
        # Check for assignment: ID = expr
        if (self.current_token.type == TokenType.ID and 
            self.peek() and self.peek().type == TokenType.ASSIGN):
            name = {
                "name": self.current_token.value,
                "type": "Identifier"
            }
            self.advance()  # consume ID
            self.expect(TokenType.ASSIGN)  # consume =
            value = self.parse_expression()
            
            # Optional semicolon
            if self.current_token and self.current_token.type == TokenType.SEMICOLON:
                self.advance()
            
            return {
                "type": "Expression",
                "value": {
                    "type": "Assignment",
                    "name": name,
                    "value": value
                }
            }
        
        # Regular expression statement
        expr = self.parse_expression()
        
        # Optional semicolon
        if self.current_token and self.current_token.type == TokenType.SEMICOLON:
            self.advance()
        
        return {
            "type": "Expression",
            "value": expr
        }
    
    def parse_expression(self) -> Dict[str, Any]:
        """Parse an expression with precedence."""
        return self.parse_threading()
    
    def parse_threading(self) -> Dict[str, Any]:
        """Parse threading (|>) expressions (left-associative)."""
        left = self.parse_composition()
        
        if self.current_token and self.current_token.type == TokenType.PIPE_OP:
            # Collect all threading functions
            functions = []
            initial = left
            
            while self.current_token and self.current_token.type == TokenType.PIPE_OP:
                self.advance()  # consume |>
                right = self.parse_composition()
                functions.append(right)
            
            return {
                "functions": functions,
                "initial": initial,
                "type": "FunctionThread"
            }
        
        return left
    
    def parse_composition(self) -> Dict[str, Any]:
        """Parse composition (>>) expressions (right-associative)."""
        left = self.parse_logical_or()
        
        if self.current_token and self.current_token.type == TokenType.COMPOSE:
            # Collect all composition functions
            functions = [left]
            
            while self.current_token and self.current_token.type == TokenType.COMPOSE:
                self.advance()  # consume >>
                right = self.parse_logical_or()
                functions.append(right)
            
            return {
                "functions": functions,
                "type": "FunctionComposition"
            }
        
        return left
    
    def parse_logical_or(self) -> Dict[str, Any]:
        """Parse logical OR (||) expressions."""
        left = self.parse_logical_and()
        
        while self.current_token and self.current_token.type == TokenType.OR:
            operator = self.current_token.value
            self.advance()
            right = self.parse_logical_and()
            left = {
                "left": left,
                "operator": operator,
                "right": right,
                "type": "Infix"
            }
        
        return left
    
    def parse_logical_and(self) -> Dict[str, Any]:
        """Parse logical AND (&&) expressions."""
        left = self.parse_equality()
        
        while self.current_token and self.current_token.type == TokenType.AND:
            operator = self.current_token.value
            self.advance()
            right = self.parse_equality()
            left = {
                "left": left,
                "operator": operator,
                "right": right,
                "type": "Infix"
            }
        
        return left
    
    def parse_equality(self) -> Dict[str, Any]:
        """Parse equality expressions (==, !=)."""
        left = self.parse_comparison()
        
        while self.current_token and self.current_token.type in [TokenType.EQ, TokenType.NE]:
            operator = self.current_token.value
            self.advance()
            right = self.parse_comparison()
            left = {
                "left": left,
                "operator": operator,
                "right": right,
                "type": "Infix"
            }
        
        return left
    
    def parse_comparison(self) -> Dict[str, Any]:
        """Parse comparison expressions (>, <, >=, <=)."""
        left = self.parse_addition()
        
        while self.current_token and self.current_token.type in [TokenType.GT, TokenType.LT, TokenType.GE, TokenType.LE]:
            operator = self.current_token.value
            self.advance()
            right = self.parse_addition()
            left = {
                "left": left,
                "operator": operator,
                "right": right,
                "type": "Infix"
            }
        
        return left
    
    def parse_addition(self) -> Dict[str, Any]:
        """Parse addition and subtraction expressions."""
        left = self.parse_multiplication()
        
        while self.current_token and self.current_token.type in [TokenType.PLUS, TokenType.MINUS]:
            operator = self.current_token.value
            self.advance()
            right = self.parse_multiplication()
            left = {
                "left": left,
                "operator": operator,
                "right": right,
                "type": "Infix"
            }
        
        return left
    
    def parse_multiplication(self) -> Dict[str, Any]:
        """Parse multiplication and division expressions."""
        left = self.parse_unary()
        
        while self.current_token and self.current_token.type in [TokenType.MULTIPLY, TokenType.DIVIDE]:
            operator = self.current_token.value
            self.advance()
            right = self.parse_unary()
            left = {
                "left": left,
                "operator": operator,
                "right": right,
                "type": "Infix"
            }
        
        return left
    
    def parse_unary(self) -> Dict[str, Any]:
        """Parse unary expressions."""
        if self.current_token and self.current_token.type == TokenType.MINUS:
            operator = self.current_token.value
            self.advance()
            operand = self.parse_unary()
            return {
                "type": "Prefix",
                "operator": operator,
                "operand": operand
            }
        
        return self.parse_postfix()
    
    def parse_postfix(self) -> Dict[str, Any]:
        """Parse postfix expressions (indexing, function calls)."""
        left = self.parse_primary()
        
        while True:
            if self.current_token and self.current_token.type == TokenType.LBRACKET:
                # Indexing
                self.advance()  # consume '['
                index = self.parse_expression()
                self.expect(TokenType.RBRACKET)
                left = {
                    "index": index,
                    "left": left,
                    "type": "Index"
                }
            elif self.current_token and self.current_token.type == TokenType.LPAREN:
                # Function call
                self.advance()  # consume '('
                arguments = []
                
                while self.current_token and self.current_token.type != TokenType.RPAREN:
                    arguments.append(self.parse_expression())
                    
                    if self.current_token and self.current_token.type == TokenType.COMMA:
                        self.advance()
                    elif self.current_token and self.current_token.type != TokenType.RPAREN:
                        break
                
                self.expect(TokenType.RPAREN)
                left = {
                    "arguments": arguments,
                    "function": left,
                    "type": "Call"
                }
            else:
                break
        
        return left
    
    def parse_primary(self) -> Dict[str, Any]:
        """Parse primary expressions."""
        if not self.current_token:
            raise SyntaxError("Unexpected end of input")
        
        # Literals
        if self.current_token.type == TokenType.INT:
            value = self.current_token.value
            self.advance()
            return {"type": "Integer", "value": value}
        
        elif self.current_token.type == TokenType.DEC:
            value = self.current_token.value
            self.advance()
            return {"type": "Decimal", "value": value}
        
        elif self.current_token.type == TokenType.STR:
            # Remove quotes and handle escape sequences for the value
            raw_value = self.current_token.value
            if raw_value.startswith('"') and raw_value.endswith('"'):
                # Extract content without quotes
                content = raw_value[1:-1]
                # Handle escape sequences
                unescaped = content.replace('\\\"', '"').replace('\\\\', '\\').replace('\\n', '\n').replace('\\t', '\t')
                self.advance()
                return {"type": "String", "value": unescaped}
            else:
                self.advance()
                return {"type": "String", "value": raw_value}
        
        elif self.current_token.type == TokenType.TRUE:
            self.advance()
            return {"type": "Boolean", "value": True}
        
        elif self.current_token.type == TokenType.FALSE:
            self.advance()
            return {"type": "Boolean", "value": False}
        
        elif self.current_token.type == TokenType.NIL:
            self.advance()
            return {"type": "Nil"}
        
        # Identifiers
        elif self.current_token.type == TokenType.ID:
            name = self.current_token.value
            self.advance()
            return {"name": name, "type": "Identifier"}
        
        # Parentheses
        elif self.current_token.type == TokenType.LPAREN:
            self.advance()
            expr = self.parse_expression()
            self.expect(TokenType.RPAREN)
            return expr
        
        # Let declarations
        elif self.current_token.type == TokenType.LET:
            return self.parse_let()
        
        # If expressions
        elif self.current_token.type == TokenType.IF:
            return self.parse_if()
        
        # Lists
        elif self.current_token.type == TokenType.LBRACKET:
            return self.parse_list()
        
        # Sets and Dictionaries
        elif self.current_token.type == TokenType.LBRACE:
            return self.parse_set()
        
        elif self.current_token.type == TokenType.DICT_START:  # #{
            return self.parse_dictionary()
        
        # Function literals
        elif self.current_token.type == TokenType.PIPE:
            return self.parse_function()
        
        # Zero-parameter function literals (||)
        elif self.current_token.type == TokenType.OR:
            return self.parse_zero_param_function()
        
        # Operator identifiers (for higher-order functions)
        elif self.current_token.type in [TokenType.PLUS, TokenType.MINUS, TokenType.MULTIPLY, TokenType.DIVIDE]:
            name = self.current_token.value
            self.advance()
            return {"name": name, "type": "Identifier"}
        
        else:
            raise SyntaxError(f"Unexpected token: {self.current_token}")
    
    def parse_let(self) -> Dict[str, Any]:
        """Parse let declarations."""
        self.expect(TokenType.LET)
        
        is_mutable = False
        if self.current_token and self.current_token.type == TokenType.MUT:
            is_mutable = True
            self.advance()
        
        if not self.current_token or self.current_token.type != TokenType.ID:
            raise SyntaxError(f"Expected identifier after let, got {self.current_token}")
        
        name = {
            "name": self.current_token.value,
            "type": "Identifier"
        }
        self.advance()
        
        self.expect(TokenType.ASSIGN)
        value = self.parse_expression()
        
        return {
            "name": name,
            "type": "MutableLet" if is_mutable else "Let",
            "value": value
        }
    
    def parse_if(self) -> Dict[str, Any]:
        """Parse if expressions."""
        self.expect(TokenType.IF)
        condition = self.parse_expression()
        consequence = self.parse_block()
        
        alternative = None
        if self.current_token and self.current_token.type == TokenType.ELSE:
            self.advance()
            alternative = self.parse_block()
        
        return {
            "alternative": alternative,
            "condition": condition,
            "consequence": consequence,
            "type": "If"
        }
    
    def parse_block(self) -> Dict[str, Any]:
        """Parse a block expression."""
        self.expect(TokenType.LBRACE)
        statements = []
        
        while self.current_token and self.current_token.type != TokenType.RBRACE:
            self.skip_comments()
            if self.current_token and self.current_token.type != TokenType.RBRACE:
                stmt = self.parse_statement()
                if stmt:
                    statements.append(stmt)
        
        self.expect(TokenType.RBRACE)
        
        return {
            "statements": statements,
            "type": "Block"
        }
    
    def parse_list(self) -> Dict[str, Any]:
        """Parse list literals."""
        self.expect(TokenType.LBRACKET)
        items = []
        
        while self.current_token and self.current_token.type != TokenType.RBRACKET:
            items.append(self.parse_expression())
            
            if self.current_token and self.current_token.type == TokenType.COMMA:
                self.advance()
            elif self.current_token and self.current_token.type != TokenType.RBRACKET:
                break
        
        self.expect(TokenType.RBRACKET)
        
        return {
            "items": items,
            "type": "List"
        }
    
    def parse_set(self) -> Dict[str, Any]:
        """Parse set literals."""
        self.expect(TokenType.LBRACE)
        items = []
        
        while self.current_token and self.current_token.type != TokenType.RBRACE:
            items.append(self.parse_expression())
            
            if self.current_token and self.current_token.type == TokenType.COMMA:
                self.advance()
            elif self.current_token and self.current_token.type != TokenType.RBRACE:
                break
        
        self.expect(TokenType.RBRACE)
        
        return {
            "items": items,
            "type": "Set"
        }
    
    def parse_dictionary(self) -> Dict[str, Any]:
        """Parse dictionary literals."""
        self.expect(TokenType.DICT_START)  # #{
        items = []
        
        while self.current_token and self.current_token.type != TokenType.RBRACE:
            key = self.parse_expression()
            self.expect(TokenType.COLON)
            value = self.parse_expression()
            
            items.append({
                "key": key,
                "value": value
            })
            
            if self.current_token and self.current_token.type == TokenType.COMMA:
                self.advance()
            elif self.current_token and self.current_token.type != TokenType.RBRACE:
                break
        
        self.expect(TokenType.RBRACE)
        
        return {
            "items": items,
            "type": "Dictionary"
        }
    
    def parse_function(self) -> Dict[str, Any]:
        """Parse function literals."""
        self.expect(TokenType.PIPE)
        parameters = []
        
        # Parse parameters
        while self.current_token and self.current_token.type != TokenType.PIPE:
            if self.current_token.type == TokenType.ID:
                parameters.append({
                    "name": self.current_token.value,
                    "type": "Identifier"
                })
                self.advance()
                
                if self.current_token and self.current_token.type == TokenType.COMMA:
                    self.advance()
            else:
                break
        
        self.expect(TokenType.PIPE)
        
        # Parse body - could be expression or block
        if self.current_token and self.current_token.type == TokenType.LBRACE:
            body = self.parse_block()
        else:
            # Single expression body - wrap in a block
            expr = self.parse_expression()
            body = {
                "statements": [{"type": "Expression", "value": expr}],
                "type": "Block"
            }
        
        return {
            "body": body,
            "parameters": parameters,
            "type": "Function"
        }
    
    def parse_zero_param_function(self) -> Dict[str, Any]:
        """Parse zero-parameter function literals (||)."""
        self.expect(TokenType.OR)  # consume ||
        
        # Parse body - could be expression or block
        if self.current_token and self.current_token.type == TokenType.LBRACE:
            body = self.parse_block()
        else:
            # Single expression body - wrap in a block
            expr = self.parse_expression()
            body = {
                "statements": [{"type": "Expression", "value": expr}],
                "type": "Block"
            }
        
        return {
            "body": body,
            "parameters": [],  # Zero parameters
            "type": "Function"
        }


def parse_to_json(source: str) -> str:
    """Parse source and return pretty-printed JSON."""
    parser = Parser(source)
    ast = parser.parse()
    
    return json.dumps(ast, indent=2, separators=(',', ': '), ensure_ascii=False)