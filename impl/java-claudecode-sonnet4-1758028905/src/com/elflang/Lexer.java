package com.elflang;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

public class Lexer {
    private final String input;
    private int position;
    private int line;
    private int column;

    private static final Set<String> KEYWORDS = Set.of("let", "mut", "if", "else", "true", "false", "nil");

    public Lexer(String input) {
        this.input = input;
        this.position = 0;
        this.line = 1;
        this.column = 1;
    }

    public List<Token> tokenize() {
        List<Token> tokens = new ArrayList<>();

        while (position < input.length()) {
            char current = input.charAt(position);

            if (Character.isWhitespace(current)) {
                skipWhitespace();
                continue;
            }

            if (current == '/' && position + 1 < input.length() && input.charAt(position + 1) == '/') {
                Token comment = readComment();
                tokens.add(comment);
                continue;
            }

            if (Character.isDigit(current)) {
                tokens.add(readNumber());
                continue;
            }

            if (current == '"') {
                tokens.add(readString());
                continue;
            }

            if (Character.isLetter(current) || current == '_') {
                tokens.add(readIdentifierOrKeyword());
                continue;
            }

            Token operator = readOperator();
            if (operator != null) {
                tokens.add(operator);
                continue;
            }

            throw new RuntimeException("Unexpected character: " + current);
        }

        return tokens;
    }

    private void skipWhitespace() {
        while (position < input.length() && Character.isWhitespace(input.charAt(position))) {
            if (input.charAt(position) == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
            position++;
        }
    }

    private Token readComment() {
        int start = position;
        position += 2; // skip //

        while (position < input.length() && input.charAt(position) != '\n') {
            position++;
            column++;
        }

        String value = input.substring(start, position);
        return new Token("CMT", value);
    }

    private Token readNumber() {
        int start = position;
        boolean hasDot = false;

        while (position < input.length()) {
            char ch = input.charAt(position);

            if (Character.isDigit(ch) || ch == '_') {
                position++;
                column++;
            } else if (ch == '.' && !hasDot && position + 1 < input.length()
                      && Character.isDigit(input.charAt(position + 1))) {
                hasDot = true;
                position++;
                column++;
            } else {
                break;
            }
        }

        String value = input.substring(start, position);
        String type = hasDot ? "DEC" : "INT";

        return new Token(type, value);
    }

    private Token readString() {
        int start = position;
        position++; // skip opening quote
        column++;

        StringBuilder value = new StringBuilder("\"");

        while (position < input.length()) {
            char ch = input.charAt(position);

            if (ch == '"') {
                value.append('"');
                position++;
                column++;
                break;
            } else if (ch == '\\' && position + 1 < input.length()) {
                char next = input.charAt(position + 1);
                if (next == 'n' || next == 't' || next == '"' || next == '\\') {
                    value.append('\\').append(next);
                    position += 2;
                    column += 2;
                } else {
                    value.append(ch);
                    position++;
                    column++;
                }
            } else {
                value.append(ch);
                position++;
                if (ch == '\n') {
                    line++;
                    column = 1;
                } else {
                    column++;
                }
            }
        }

        return new Token("STR", value.toString());
    }

    private Token readIdentifierOrKeyword() {
        int start = position;

        while (position < input.length() &&
               (Character.isLetterOrDigit(input.charAt(position)) || input.charAt(position) == '_')) {
            position++;
            column++;
        }

        String value = input.substring(start, position);

        if (KEYWORDS.contains(value)) {
            return new Token(value.toUpperCase(), value);
        } else {
            return new Token("ID", value);
        }
    }

    private Token readOperator() {
        char current = input.charAt(position);

        // Two-character operators
        if (position + 1 < input.length()) {
            String twoChar = input.substring(position, position + 2);
            switch (twoChar) {
                case "==":
                case "!=":
                case ">=":
                case "<=":
                case "&&":
                case "||":
                case "|>":
                case ">>":
                case "#{":
                    position += 2;
                    column += 2;
                    return new Token(twoChar, twoChar);
            }
        }

        // Single-character operators
        switch (current) {
            case '+':
            case '-':
            case '*':
            case '/':
            case '=':
            case '>':
            case '<':
            case '{':
            case '}':
            case '[':
            case ']':
            case '(':
            case ')':
            case ',':
            case ':':
            case ';':
            case '|':
                position++;
                column++;
                return new Token(String.valueOf(current), String.valueOf(current));
        }

        return null;
    }
}