package parser

// Ordered JSON fields are ensured by struct field order.

// Program is the root AST node.
type Program struct {
    Statements []Statement `json:"statements"`
    Type       string      `json:"type"`
}

// Statement is a marker interface.
type Statement interface{ isStatement() }

type ExpressionStmt struct {
    Type  string `json:"type"`
    Value Expr   `json:"value"`
}
func (ExpressionStmt) isStatement() {}

type CommentStmt struct {
    Type  string `json:"type"`
    Value string `json:"value"`
}
func (CommentStmt) isStatement() {}

// Expr is a marker interface for expressions.
type Expr interface{ isExpr() }

// Identifiers and literals
type Identifier struct {
    Name string `json:"name"`
    Type string `json:"type"`
}
func (Identifier) isExpr() {}

type IntegerLit struct {
    Type  string `json:"type"`
    Value string `json:"value"`
}
func (IntegerLit) isExpr() {}

type DecimalLit struct {
    Type  string `json:"type"`
    Value string `json:"value"`
}
func (DecimalLit) isExpr() {}

type StringLit struct {
    Type  string `json:"type"`
    Value string `json:"value"`
}
func (StringLit) isExpr() {}

type BooleanLit struct {
    Type  string `json:"type"`
    Value bool   `json:"value"`
}
func (BooleanLit) isExpr() {}

type NilLit struct {
    Type string `json:"type"`
}
func (NilLit) isExpr() {}

// Let / MutableLet
type LetExpr struct {
    Name  Identifier `json:"name"`
    Type  string     `json:"type"`
    Value Expr       `json:"value"`
}
func (LetExpr) isExpr() {}

// Infix expression
type InfixExpr struct {
    Left     Expr   `json:"left"`
    Operator string `json:"operator"`
    Right    Expr   `json:"right"`
    Type     string `json:"type"`
}
func (InfixExpr) isExpr() {}

// Assignment expression
type AssignExpr struct {
    Name  Identifier `json:"name"`
    Type  string     `json:"type"`
    Value Expr       `json:"value"`
}
func (AssignExpr) isExpr() {}

// Prefix expression (currently only unary minus)
type PrefixExpr struct {
    Operator string `json:"operator"`
    Operand  Expr   `json:"operand"`
    Type     string `json:"type"`
}
func (PrefixExpr) isExpr() {}

// Collections
type ListLit struct {
    Items []Expr `json:"items"`
    Type  string `json:"type"`
}
func (ListLit) isExpr() {}

type SetLit struct {
    Items []Expr `json:"items"`
    Type  string `json:"type"`
}
func (SetLit) isExpr() {}

type DictEntry struct {
    Key   Expr `json:"key"`
    Value Expr `json:"value"`
}

type DictLit struct {
    Items []DictEntry `json:"items"`
    Type  string      `json:"type"`
}
func (DictLit) isExpr() {}

// Indexing
type IndexExpr struct {
    Index Expr  `json:"index"`
    Left  Expr  `json:"left"`
    Type  string `json:"type"`
}
func (IndexExpr) isExpr() {}

// If expression
type IfExpr struct {
    Alternative Block `json:"alternative"`
    Condition   Expr  `json:"condition"`
    Consequence Block `json:"consequence"`
    Type        string `json:"type"`
}
func (IfExpr) isExpr() {}

// Block
type Block struct {
    Statements []Statement `json:"statements"`
    Type       string      `json:"type"`
}

// Function literal and call
type FunctionLit struct {
    Body       Block        `json:"body"`
    Parameters []Identifier `json:"parameters"`
    Type       string       `json:"type"`
}
func (FunctionLit) isExpr() {}

type CallExpr struct {
    Arguments []Expr `json:"arguments"`
    Function  Expr   `json:"function"`
    Type      string `json:"type"`
}
func (CallExpr) isExpr() {}

// Composition and Threading
type FunctionComposition struct {
    Functions []Expr `json:"functions"`
    Type      string `json:"type"`
}
func (FunctionComposition) isExpr() {}

type FunctionThread struct {
    Functions []Expr `json:"functions"`
    Initial   Expr   `json:"initial"`
    Type      string `json:"type"`
}
func (FunctionThread) isExpr() {}
