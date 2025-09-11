package parser

import (
    "fmt"
    "strings"

    "elf-lang/impl/internal/lexer"
)

type Parser struct {
    toks []lexer.Token
    i    int
}

func New(toks []lexer.Token) *Parser { return &Parser{toks: toks} }

func (p *Parser) cur() lexer.Token {
    if p.i >= len(p.toks) {
        return lexer.Token{Type: "EOF"}
    }
    return p.toks[p.i]
}

func (p *Parser) next() lexer.Token {
    t := p.cur()
    if p.i < len(p.toks) { p.i++ }
    return t
}

func (p *Parser) match(typ string) bool {
    if p.cur().Type == typ { p.i++; return true }
    return false
}

func (p *Parser) expect(typ string) lexer.Token {
    t := p.cur()
    if t.Type != typ {
        panic(fmt.Sprintf("expected %s, found %s", typ, t.Type))
    }
    p.i++
    return t
}

// Precedence values (higher binds tighter)
const (
    precLowest = iota
    precOr
    precAnd
    precCompare
    precThread   // |>
    precCompose  // >> (higher than thread, right-assoc)
    precAdd
    precMul
    precCallIndex // calls and indexing
)

func precedence(op string) int {
    switch op {
    case "||": return precOr
    case "&&": return precAnd
    case "==", "!=", ">", "<", ">=", "<=": return precCompare
    case "|>": return precThread
    case ">>": return precCompose
    case "+", "-": return precAdd
    case "*", "/": return precMul
    default:
        return precLowest
    }
}

func (p *Parser) ParseProgram() Program {
    var stmts []Statement
    for p.cur().Type != "EOF" {
        // Comments become Comment statements
        if p.cur().Type == "CMT" {
            c := p.next()
            stmts = append(stmts, CommentStmt{Type: "Comment", Value: c.Lit})
            // Optional semicolon after comment
            if p.match(";") { /* skip */ }
            continue
        }

        expr := p.parseExpression(precLowest)
        stmts = append(stmts, ExpressionStmt{Type: "Expression", Value: expr})
        // Optional semicolon between statements
        if p.match(";") { /* ok */ }
    }
    return Program{Statements: stmts, Type: "Program"}
}

func (p *Parser) parseExpression(minPrec int) Expr {
    left := p.parsePrefix()

    for {
        t := p.cur()
        // Assignment: only when left is Identifier and next token '='
        if t.Type == "=" {
            if id, ok := left.(Identifier); ok {
                p.next()
                right := p.parseExpression(precLowest)
                left = AssignExpr{Name: id, Type: "Assignment", Value: right}
                continue
            }
            break
        }
        // Handle call and indexing as highest precedence postfix
        if t.Type == "(" { // call
            p.next()
            var args []Expr
            if !p.match(")") {
                for {
                    args = append(args, p.parseExpression(precLowest))
                    if p.match(")") { break }
                    p.expect(",")
                }
            }
            left = CallExpr{Arguments: args, Function: left, Type: "Call"}
            continue
        }
        if t.Type == "[" { // indexing
            p.next()
            idx := p.parseExpression(precLowest)
            p.expect("]")
            left = IndexExpr{Index: idx, Left: left, Type: "Index"}
            continue
        }

        // Infix operators
        op := t.Type
        if !(op == "+" || op == "-" || op == "*" || op == "/" ||
            op == ">" || op == "<" || op == ">=" || op == "<=" || op == "==" || op == "!=" ||
            op == "&&" || op == "||" ||
            op == ">>" || op == "|>") {
            break
        }

        // Determine precedence and associativity
        pPrec := precedence(op)
        rightAssoc := (op == ">>")
        if pPrec < minPrec { break }
        // consume operator
        p.next()
        nextMin := pPrec + 1
        if rightAssoc { nextMin = pPrec }
        right := p.parseExpression(nextMin)

        // Special shapes for compose and thread
        if op == ">>" {
            // Flatten into FunctionComposition
            var funcs []Expr
            if fc, ok := left.(FunctionComposition); ok {
                funcs = append(funcs, fc.Functions...)
            } else {
                funcs = append(funcs, left)
            }
            if fc, ok := right.(FunctionComposition); ok {
                funcs = append(funcs, fc.Functions...)
            } else {
                funcs = append(funcs, right)
            }
            left = FunctionComposition{Functions: funcs, Type: "FunctionComposition"}
            continue
        }
        if op == "|>" {
            // Flatten into FunctionThread
            var init Expr
            var funcs []Expr
            if ft, ok := left.(FunctionThread); ok {
                init = ft.Initial
                funcs = append(funcs, ft.Functions...)
            } else {
                init = left
            }
            // Right side may be a call or identifier (function name)
            switch r := right.(type) {
            case CallExpr:
                funcs = append(funcs, r)
            default:
                funcs = append(funcs, r)
            }
            left = FunctionThread{Functions: funcs, Initial: init, Type: "FunctionThread"}
            continue
        }

        left = InfixExpr{Left: left, Operator: op, Right: right, Type: "Infix"}
    }

    return left
}

func (p *Parser) parsePrefix() Expr {
    t := p.next()
    switch t.Type {
    case "-":
        // unary minus
        operand := p.parseExpression(precMul) // higher than add/sub
        return PrefixExpr{Operator: "-", Operand: operand, Type: "Prefix"}
    case "INT":
        return IntegerLit{Type: "Integer", Value: t.Lit}
    case "DEC":
        return DecimalLit{Type: "Decimal", Value: t.Lit}
    case "STR":
        return StringLit{Type: "String", Value: unquote(t.Lit)}
    case "TRUE":
        return BooleanLit{Type: "Boolean", Value: true}
    case "FALSE":
        return BooleanLit{Type: "Boolean", Value: false}
    case "NIL":
        return NilLit{Type: "Nil"}
    case "ID":
        return Identifier{Name: t.Lit, Type: "Identifier"}
    case "[":
        items := make([]Expr, 0)
        if !p.match("]") {
            for {
                items = append(items, p.parseExpression(precLowest))
                if p.match("]") { break }
                p.expect(",")
            }
        }
        return ListLit{Items: items, Type: "List"}
    case "{":
        // Set literal
        items := make([]Expr, 0)
        if !p.match("}") {
            for {
                items = append(items, p.parseExpression(precLowest))
                if p.match("}") { break }
                p.expect(",")
            }
        }
        return SetLit{Items: items, Type: "Set"}
    case "#{":
        items := make([]DictEntry, 0)
        if !p.match("}") { // closing brace is just '}' after '#{'
            for {
                key := p.parseExpression(precLowest)
                p.expect(":")
                val := p.parseExpression(precLowest)
                items = append(items, DictEntry{Key: key, Value: val})
                if p.match("}") { break }
                p.expect(",")
            }
        }
        return DictLit{Items: items, Type: "Dictionary"}
    case "(":
        expr := p.parseExpression(precLowest)
        p.expect(")")
        return expr
    case "|", "||":
        // Function literal: |params| body
        var params []Identifier
        if t.Type == "|" && !p.match("|") { // parameters present; for "||" we already consumed both
            for {
                idTok := p.expect("ID")
                params = append(params, Identifier{Name: idTok.Lit, Type: "Identifier"})
                if p.match("|") { break }
                p.expect(",")
            }
        }
        // Body: expression or block
        var body Block
        if p.cur().Type == "{" { // block body
            body = p.parseBlock()
        } else {
            // single expression wrapped in a Block
            expr := p.parseExpression(precLowest)
            body = Block{Statements: []Statement{ExpressionStmt{Type: "Expression", Value: expr}}, Type: "Block"}
        }
        return FunctionLit{Body: body, Parameters: params, Type: "Function"}
    case "LET":
        // let (mut)? name = expr
        mut := false
        if p.cur().Type == "MUT" { p.next(); mut = true }
        nameTok := p.expect("ID")
        p.expect("=")
        val := p.parseExpression(precLowest)
        typ := "Let"; if mut { typ = "MutableLet" }
        return LetExpr{Name: Identifier{Name: nameTok.Lit, Type: "Identifier"}, Type: typ, Value: val}
    case "IF":
        cond := p.parseExpression(precCompare)
        cons := p.parseBlock()
        p.expect("ELSE")
        alt := p.parseBlock()
        return IfExpr{Alternative: alt, Condition: cond, Consequence: cons, Type: "If"}
    default:
        // Fallback for unexpected token; return identifier of the literal token
        return Identifier{Name: strings.TrimSpace(t.Lit), Type: "Identifier"}
    }
}

func (p *Parser) parseBlock() Block {
    p.expect("{")
    var stmts []Statement
    for p.cur().Type != "}" && p.cur().Type != "EOF" {
        if p.cur().Type == "CMT" {
            c := p.next()
            stmts = append(stmts, CommentStmt{Type: "Comment", Value: c.Lit})
            if p.match(";") { /* optional */ }
            continue
        }
        expr := p.parseExpression(precLowest)
        stmts = append(stmts, ExpressionStmt{Type: "Expression", Value: expr})
        _ = p.match(";")
    }
    p.expect("}")
    return Block{Statements: stmts, Type: "Block"}
}

// unquote removes surrounding quotes from a STR token and unescapes sequences.
func unquote(s string) string {
    if len(s) >= 2 && s[0] == '"' && s[len(s)-1] == '"' {
        s = s[1:len(s)-1]
    }
    // handle simple escapes: \n, \t, \", \\
    var b strings.Builder
    for i := 0; i < len(s); i++ {
        c := s[i]
        if c == '\\' && i+1 < len(s) {
            i++
            switch s[i] {
            case 'n': b.WriteByte('\n')
            case 't': b.WriteByte('\t')
            case '"': b.WriteByte('"')
            case '\\': b.WriteByte('\\')
            default:
                // preserve unknown escapes as-is without backslash
                b.WriteByte(s[i])
            }
            continue
        }
        b.WriteByte(c)
    }
    return b.String()
}
