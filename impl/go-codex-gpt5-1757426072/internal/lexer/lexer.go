package lexer

import (
    "unicode"
)

type Token struct {
    Type string
    Lit  string
}

// Lex converts source into a flat token stream matching Stage 1 expectations.
func Lex(src string) []Token {
    var out []Token
    i := 0
    n := len(src)

    // helper to peek next byte; returns 0 if out of bounds
    peek := func(off int) byte {
        j := i + off
        if j >= n || j < 0 {
            return 0
        }
        return src[j]
    }

    emit := func(typ, lit string) { out = append(out, Token{Type: typ, Lit: lit}) }

    for i < n {
        ch := src[i]

        // Whitespace skip
        if ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' { i++; continue }

        // Line comment: // ... to end of line (without newline)
        if ch == '/' && peek(1) == '/' {
            start := i
            i += 2
            for i < n && src[i] != '\n' { i++ }
            emit("CMT", src[start:i])
            continue
        }

        // Strings: double-quoted, with escapes; capture raw slice including quotes
        if ch == '"' {
            start := i
            i++
            for i < n {
                c := src[i]
                if c == '\\' { // escape, skip next if any
                    i += 2
                    continue
                }
                if c == '"' { i++; break }
                i++
            }
            emit("STR", src[start:i])
            continue
        }

        // Numbers: INT or DEC, numeric underscores preserved
        if isDigit(ch) {
            start := i
            // integer part (digits and underscores)
            for i < n && (isDigit(src[i]) || src[i] == '_') { i++ }
            typ := "INT"
            // fractional part
            if i < n && src[i] == '.' && i+1 < n && isDigit(src[i+1]) {
                i++ // consume '.'
                for i < n && (isDigit(src[i]) || src[i] == '_') { i++ }
                typ = "DEC"
            }
            emit(typ, src[start:i])
            continue
        }

        // Identifiers / keywords / literals true/false/nil
        if isIdentStart(ch) {
            start := i
            i++
            for i < n && isIdentPart(src[i]) { i++ }
            word := src[start:i]
            switch word {
            case "let": emit("LET", word)
            case "mut": emit("MUT", word)
            case "if": emit("IF", word)
            case "else": emit("ELSE", word)
            case "true": emit("TRUE", word)
            case "false": emit("FALSE", word)
            case "nil": emit("NIL", word)
            default:
                emit("ID", word)
            }
            continue
        }

        // Multi-char operators/symbols (longest-match first per starter)
        // #{
        if ch == '#' && peek(1) == '{' {
            emit("#{", "#{")
            i += 2
            continue
        }
        // Two-char ops
        two := func(a, b byte, typ string) bool {
            if ch == a && peek(1) == b { emit(typ, src[i:i+2]); i += 2; return true }
            return false
        }
        if two('=', '=', "==") || two('!', '=', "!=") || two('>', '=', ">=") || two('<', '=', "<=") ||
            two('&', '&', "&&") || two('|', '|', "||") || two('|', '>', "|>") || two('>', '>', ">>") {
            continue
        }

        // Single-char tokens
        switch ch {
        case '+', '-', '*', '/', '=', '{', '}', '[', ']', '>', '<', ';', '(', ')', ',', ':', '|':
            emit(string(ch), string(ch))
            i++
            continue
        }

        // Unknown char: skip to avoid infinite loop (should not occur in tests)
        i++
    }

    return out
}

func isDigit(b byte) bool { return b >= '0' && b <= '9' }

func isIdentStart(b byte) bool {
    // ASCII letter or underscore; keep simple for Stage 1
    return b == '_' || (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') || b >= 128 && unicode.IsLetter(rune(b))
}

func isIdentPart(b byte) bool {
    return isIdentStart(b) || isDigit(b)
}
