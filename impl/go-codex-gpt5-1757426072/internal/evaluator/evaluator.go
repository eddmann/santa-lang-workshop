package evaluator

import (
    "errors"
    "fmt"
    "io"
    "sort"
    "strings"

    "elf-lang/impl/internal/parser"
)

// Value system
type Value interface{ repr() string }

type (
    Int    struct{ V int64 }
    Dec    struct{ V float64; Lit string }
    Str    struct{ V string }
    Bool   struct{ V bool }
    Nil    struct{}
    List   struct{ Items []Value }
    Set    struct{ Items []Value }
    Dict   struct{ Items []dictEntry }
)

func (v Int) repr() string  { return fmt.Sprintf("%d", v.V) }
func (v Dec) repr() string  { if v.Lit != "" { return v.Lit }; return formatDecimal(v.V) }
func (v Str) repr() string  { return fmt.Sprintf("\"%s\"", escapeForPrint(v.V)) }
func (v Bool) repr() string { if v.V { return "true" }; return "false" }
func (v Nil) repr() string  { return "nil" }
func (v List) repr() string {
    var b strings.Builder
    b.WriteByte('[')
    for i, it := range v.Items {
        if i > 0 { b.WriteString(", ") }
        b.WriteString(Format(it))
    }
    b.WriteByte(']')
    return b.String()
}
func (v Set) repr() string {
    // Print in ascending order by value
    items := make([]Value, len(v.Items))
    copy(items, v.Items)
    sort.Slice(items, func(i, j int) bool { return compare(items[i], items[j]) < 0 })
    var b strings.Builder
    b.WriteByte('{')
    for i, it := range items {
        if i > 0 { b.WriteString(", ") }
        b.WriteString(Format(it))
    }
    b.WriteByte('}')
    return b.String()
}
func (v Dict) repr() string {
    // Print ascending order by key
    items := make([]dictEntry, len(v.Items))
    copy(items, v.Items)
    sort.Slice(items, func(i, j int) bool { return compare(items[i].Key, items[j].Key) < 0 })
    var b strings.Builder
    b.WriteString("#{")
    for i, it := range items {
        if i > 0 { b.WriteString(", ") }
        b.WriteString(Format(it.Key))
        b.WriteString(": ")
        b.WriteString(Format(it.Val))
    }
    b.WriteByte('}')
    return b.String()
}

// decimal formatting to match tests:
// use fixed 15 decimals then trim trailing zeros/dot
func formatDecimal(f float64) string {
    s := fmt.Sprintf("%.15f", f)
    s = strings.TrimRight(s, "0")
    if strings.HasSuffix(s, ".") { s = s[:len(s)-1] }
    if s == "" { return "0" }
    return s
}

func escapeForPrint(s string) string {
    // Do not escape double-quotes; preserve them as-is to match tests
    // Escape backslash, newline, tab for readability
    s = strings.ReplaceAll(s, "\\", "\\\\")
    s = strings.ReplaceAll(s, "\n", "\\n")
    s = strings.ReplaceAll(s, "\t", "\\t")
    return s
}

func normalizeDecLiteralString(s string) string {
    s = strings.ReplaceAll(s, "_", "")
    if i := strings.IndexByte(s, '.'); i >= 0 {
        // trim trailing zeros
        s = strings.TrimRight(s, "0")
        if strings.HasSuffix(s, ".") { s = s[:len(s)-1] }
    }
    return s
}

// Environment with mutability
type binding struct {
    val Value
    mut bool
}

type Env struct {
    store map[string]binding
    outer *Env
}

func NewEnv(outer *Env) *Env { return &Env{store: map[string]binding{}, outer: outer} }

func (e *Env) Define(name string, v Value, mutable bool) { e.store[name] = binding{val: v, mut: mutable} }

func (e *Env) Get(name string) (Value, error) {
    if b, ok := e.store[name]; ok { return b.val, nil }
    if e.outer != nil { return e.outer.Get(name) }
    return nil, fmt.Errorf("Identifier can not be found: %s", name)
}

func (e *Env) Assign(name string, v Value) error {
    if b, ok := e.store[name]; ok {
        if !b.mut { return fmt.Errorf("Variable '%s' is not mutable", name) }
        e.store[name] = binding{val: v, mut: b.mut}
        return nil
    }
    if e.outer != nil { return e.outer.Assign(name, v) }
    return fmt.Errorf("Identifier can not be found: %s", name)
}

// Evaluator
type Evaluator struct {
    out io.Writer
    env *Env
}

func New(w io.Writer) *Evaluator {
    env := NewEnv(nil)
    ev := &Evaluator{out: w, env: env}
    // Built-ins
    env.Define("puts", newBuiltin("puts", 1, func(ev2 *Evaluator, args []Value) (Value, error) {
        for _, a := range args { fmt.Fprintf(ev.out, "%s ", a.repr()) }
        fmt.Fprint(ev.out, "\n")
        return Nil{}, nil
    }), false)
    // Collections and utilities
    env.Define("first", newBuiltin("first", 1, func(ev2 *Evaluator, args []Value) (Value, error) {
        switch x := args[0].(type) {
        case List:
            if len(x.Items) == 0 { return Nil{}, nil }
            return x.Items[0], nil
        case Str:
            if len(x.V) == 0 { return Nil{}, nil }
            return Str{V: x.V[:1]}, nil
        default:
            return Nil{}, nil
        }
    }), false)
    env.Define("rest", newBuiltin("rest", 1, func(ev2 *Evaluator, args []Value) (Value, error) {
        switch x := args[0].(type) {
        case List:
            if len(x.Items) == 0 { return List{Items: []Value{}}, nil }
            cp := make([]Value, len(x.Items)-1)
            copy(cp, x.Items[1:])
            return List{Items: cp}, nil
        case Str:
            if len(x.V) == 0 { return Str{V: ""}, nil }
            return Str{V: x.V[1:]}, nil
        default:
            return Nil{}, nil
        }
    }), false)
    env.Define("size", newBuiltin("size", 1, func(ev2 *Evaluator, args []Value) (Value, error) {
        switch x := args[0].(type) {
        case List: return Int{V: int64(len(x.Items))}, nil
        case Set: return Int{V: int64(len(x.Items))}, nil
        case Dict: return Int{V: int64(len(x.Items))}, nil
        case Str: return Int{V: int64(len(x.V))}, nil
        default: return Int{V: 0}, nil
        }
    }), false)
    env.Define("push", newBuiltin("push", 2, func(ev2 *Evaluator, args []Value) (Value, error) {
        v := args[0]
        switch coll := args[1].(type) {
        case List:
            cp := make([]Value, 0, len(coll.Items)+1)
            cp = append(cp, coll.Items...)
            cp = append(cp, v)
            return List{Items: cp}, nil
        case Set:
            // add if not present (structural equality)
            for _, it := range coll.Items { if equal(it, v) { return coll, nil } }
            cp := make([]Value, 0, len(coll.Items)+1)
            cp = append(cp, coll.Items...)
            cp = append(cp, v)
            return Set{Items: cp}, nil
        default:
            return Nil{}, fmt.Errorf("Unsupported operation: %s push", typeName(args[1]))
        }
    }), false)
    env.Define("assoc", newBuiltin("assoc", 3, func(ev2 *Evaluator, args []Value) (Value, error) {
        key := args[0]
        val := args[1]
        dict, ok := args[2].(Dict)
        if !ok { return Nil{}, fmt.Errorf("assoc(...): invalid argument type, expected Dictionary, found %s", typeName(args[2])) }
        if _, isDict := key.(Dict); isDict { return Nil{}, fmt.Errorf("Unable to use a Dictionary as a Dictionary key") }
        // copy and replace
        replaced := false
        out := make([]dictEntry, 0, len(dict.Items))
        for _, e := range dict.Items {
            if equal(e.Key, key) {
                if !replaced { out = append(out, dictEntry{Key: key, Val: val}); replaced = true }
            } else {
                out = append(out, e)
            }
        }
        if !replaced { out = append(out, dictEntry{Key: key, Val: val}) }
        return Dict{Items: out}, nil
    }), false)
    // Higher-order list operations
    env.Define("map", newBuiltin("map", 2, func(ev2 *Evaluator, args []Value) (Value, error) {
        fn, ok1 := args[0].(Function)
        list, ok2 := args[1].(List)
        if !ok1 || !ok2 {
            a := typeName(args[0]); b := typeName(args[1])
            return nil, fmt.Errorf("Unexpected argument: map(%s, %s)", a, b)
        }
        out := make([]Value, 0, len(list.Items))
        for _, it := range list.Items {
            v, err := fn.call(ev2, []Value{it}); if err != nil { return nil, err }
            out = append(out, v)
        }
        return List{Items: out}, nil
    }), false)
    env.Define("filter", newBuiltin("filter", 2, func(ev2 *Evaluator, args []Value) (Value, error) {
        fn, ok1 := args[0].(Function)
        list, ok2 := args[1].(List)
        if !ok1 || !ok2 {
            a := typeName(args[0]); b := typeName(args[1])
            return nil, fmt.Errorf("Unexpected argument: filter(%s, %s)", a, b)
        }
        out := make([]Value, 0, len(list.Items))
        for _, it := range list.Items {
            v, err := fn.call(ev2, []Value{it}); if err != nil { return nil, err }
            if isTruthy(v) { out = append(out, it) }
        }
        return List{Items: out}, nil
    }), false)
    env.Define("fold", newBuiltin("fold", 3, func(ev2 *Evaluator, args []Value) (Value, error) {
        acc := args[0]
        fn, ok1 := args[1].(Function)
        list, ok2 := args[2].(List)
        if !ok1 || !ok2 {
            a := typeName(args[0]); b := typeName(args[1]); c := typeName(args[2])
            return nil, fmt.Errorf("Unexpected argument: fold(%s, %s, %s)", a, b, c)
        }
        cur := acc
        for _, it := range list.Items {
            v, err := fn.call(ev2, []Value{cur, it}); if err != nil { return nil, err }
            cur = v
        }
        return cur, nil
    }), false)
    // Operator functions
    env.Define("+", newBuiltin("+", 2, func(ev2 *Evaluator, args []Value) (Value, error) { return ev.add(args[0], args[1]) }), false)
    env.Define("-", newBuiltin("-", 2, func(ev2 *Evaluator, args []Value) (Value, error) { return ev.sub(args[0], args[1]) }), false)
    env.Define("*", newBuiltin("*", 2, func(ev2 *Evaluator, args []Value) (Value, error) { return ev.mul(args[0], args[1]) }), false)
    env.Define("/", newBuiltin("/", 2, func(ev2 *Evaluator, args []Value) (Value, error) { return ev.div(args[0], args[1]) }), false)
    return ev
}

// Function values (only built-ins needed for stage-3)
type Function interface{ Value; call(ev *Evaluator, args []Value) (Value, error) }

type builtinFunc func(args []Value) (Value, error)

func (f builtinFunc) repr() string { return "|...| { [builtin] }" }
func (f builtinFunc) call(ev *Evaluator, args []Value) (Value, error) { return f(args) }

// builtin with arity and partial application support
type builtin struct {
    name  string
    arity int
    impl  func(ev *Evaluator, args []Value) (Value, error)
    pre   []Value
}

func (b *builtin) repr() string { return "|...| { [builtin] }" }
func (b *builtin) call(ev *Evaluator, args []Value) (Value, error) {
    all := append(append([]Value{}, b.pre...), args...)
    if len(all) < b.arity {
        return &builtin{name: b.name, arity: b.arity, impl: b.impl, pre: all}, nil
    }
    return b.impl(ev, all)
}

func newBuiltin(name string, arity int, impl func(ev *Evaluator, args []Value) (Value, error)) Function {
    return &builtin{name: name, arity: arity, impl: impl, pre: nil}
}

// Format produces the canonical printed representation for a value
func Format(v Value) string { return v.repr() }

// Public API
func (ev *Evaluator) Eval(prog parser.Program) (Value, error) {
    var last Value = Nil{}
    // Top-level: evaluate statements; only last non-comment value returned
    for _, st := range prog.Statements {
        switch s := st.(type) {
        case parser.CommentStmt:
            // ignore for last value
            continue
        case parser.ExpressionStmt:
            v, err := ev.evalExpr(s.Value)
            if err != nil { return nil, err }
            last = v
        default:
            // ignore unknown
        }
    }
    return last, nil
}

func (ev *Evaluator) evalStmt(st parser.Statement) (Value, error) {
    switch s := st.(type) {
    case parser.ExpressionStmt:
        return ev.evalExpr(s.Value)
    case parser.CommentStmt:
        return Nil{}, nil
    default:
        return Nil{}, nil
    }
}

func (ev *Evaluator) evalExpr(e parser.Expr) (Value, error) {
    switch ex := e.(type) {
    case parser.IntegerLit:
        // remove underscores when parsing; strconv can handle, but here digits only
        // value range fits into int64 for tests
        var v int64 = 0
        for i := 0; i < len(ex.Value); i++ {
            c := ex.Value[i]
            if c == '_' { continue }
            v = v*10 + int64(c-'0')
        }
        return Int{V: v}, nil
    case parser.DecimalLit:
        // keep literal for printing; also parse to float for arithmetic
        s := normalizeDecLiteralString(ex.Value)
        var f float64
        fmt.Sscanf(strings.ReplaceAll(s, "_", ""), "%f", &f)
        return Dec{V: f, Lit: s}, nil
    case parser.StringLit:
        return Str{V: ex.Value}, nil
    case parser.BooleanLit:
        return Bool{V: ex.Value}, nil
    case parser.NilLit:
        return Nil{}, nil
    case parser.Identifier:
        v, err := ev.env.Get(ex.Name)
        if err != nil { return nil, err }
        return v, nil
    case parser.FunctionLit:
        params := make([]string, len(ex.Parameters))
        for i, p := range ex.Parameters { params[i] = p.Name }
        return &userFunc{params: params, body: ex.Body, env: ev.env}, nil
    case parser.ListLit:
        items := make([]Value, 0, len(ex.Items))
        for _, it := range ex.Items { v, err := ev.evalExpr(it); if err != nil { return nil, err }; items = append(items, v) }
        return List{Items: items}, nil
    case parser.SetLit:
        items := make([]Value, 0, len(ex.Items))
        for _, it := range ex.Items {
            v, err := ev.evalExpr(it); if err != nil { return nil, err }
            if _, isDict := v.(Dict); isDict { return nil, fmt.Errorf("Unable to include a Dictionary within a Set") }
            // dedupe
            present := false
            for _, e2 := range items { if equal(e2, v) { present = true; break } }
            if !present { items = append(items, v) }
        }
        return Set{Items: items}, nil
    case parser.DictLit:
        items := make([]dictEntry, 0, len(ex.Items))
        for _, it := range ex.Items {
            k, err := ev.evalExpr(it.Key); if err != nil { return nil, err }
            if _, isDict := k.(Dict); isDict { return nil, fmt.Errorf("Unable to use a Dictionary as a Dictionary key") }
            v, err := ev.evalExpr(it.Value); if err != nil { return nil, err }
            // override if duplicate key
            replaced := false
            for i := range items {
                if equal(items[i].Key, k) { items[i].Val = v; replaced = true; break }
            }
            if !replaced { items = append(items, dictEntry{Key: k, Val: v}) }
        }
        return Dict{Items: items}, nil
    case parser.LetExpr:
        v, err := ev.evalExpr(ex.Value)
        if err != nil { return nil, err }
        mutable := (ex.Type == "MutableLet")
        ev.env.Define(ex.Name.Name, v, mutable)
        return v, nil
    case parser.AssignExpr:
        v, err := ev.evalExpr(ex.Value)
        if err != nil { return nil, err }
        if err := ev.env.Assign(ex.Name.Name, v); err != nil { return nil, err }
        return v, nil
    case parser.InfixExpr:
        // evaluate logical with truthiness and short-circuit
        if ex.Operator == "&&" {
            l, err := ev.evalExpr(ex.Left); if err != nil { return nil, err }
            if !isTruthy(l) { return Bool{V: false}, nil }
            r, err := ev.evalExpr(ex.Right); if err != nil { return nil, err }
            return Bool{V: isTruthy(r)}, nil
        }
        if ex.Operator == "||" {
            l, err := ev.evalExpr(ex.Left); if err != nil { return nil, err }
            if isTruthy(l) { return Bool{V: true}, nil }
            r, err := ev.evalExpr(ex.Right); if err != nil { return nil, err }
            return Bool{V: isTruthy(r)}, nil
        }
        l, err := ev.evalExpr(ex.Left); if err != nil { return nil, err }
        r, err := ev.evalExpr(ex.Right); if err != nil { return nil, err }
        switch ex.Operator {
        case "+": return ev.add(l, r)
        case "-": return ev.sub(l, r)
        case "*": return ev.mul(l, r)
        case "/": return ev.div(l, r)
        case "==": return Bool{V: equal(l, r)}, nil
        case "!=": return Bool{V: !equal(l, r)}, nil
        case ">": return Bool{V: compare(l, r) > 0}, nil
        case "<": return Bool{V: compare(l, r) < 0}, nil
        case ">=": return Bool{V: compare(l, r) >= 0}, nil
        case "<=": return Bool{V: compare(l, r) <= 0}, nil
        default:
            return nil, errors.New("Unsupported operator")
        }
    case parser.PrefixExpr:
        v, err := ev.evalExpr(ex.Operand)
        if err != nil { return nil, err }
        switch t := v.(type) {
        case Int:
            return Int{V: -t.V}, nil
        case Dec:
            return Dec{V: -t.V}, nil
        default:
            return nil, fmt.Errorf("Unsupported operation: %s %s", ex.Operator, typeName(v))
        }
    case parser.CallExpr:
        fn, err := ev.evalExpr(ex.Function)
        if err != nil { return nil, err }
        f, ok := fn.(Function)
        if !ok { return nil, fmt.Errorf("Expected a Function, found: %s", typeName(fn)) }
        args := make([]Value, 0, len(ex.Arguments))
        for _, a := range ex.Arguments { v, err := ev.evalExpr(a); if err != nil { return nil, err }; args = append(args, v) }
        return f.call(ev, args)
    case parser.IfExpr:
        cond, err := ev.evalExpr(ex.Condition)
        if err != nil { return nil, err }
        if isTruthy(cond) { return ev.evalBlock(ex.Consequence) }
        return ev.evalBlock(ex.Alternative)
    case parser.FunctionComposition:
        funs := make([]Function, 0, len(ex.Functions))
        for _, fe := range ex.Functions {
            v, err := ev.evalExpr(fe); if err != nil { return nil, err }
            f, ok := v.(Function); if !ok { return nil, fmt.Errorf("Expected a Function, found: %s", typeName(v)) }
            funs = append(funs, f)
        }
        return &composedFunc{functions: funs}, nil
    case parser.FunctionThread:
        cur, err := ev.evalExpr(ex.Initial)
        if err != nil { return nil, err }
        for _, step := range ex.Functions {
            if ce, ok := step.(parser.CallExpr); ok {
                fnVal, err := ev.evalExpr(ce.Function)
                if err != nil { return nil, err }
                f, ok := fnVal.(Function); if !ok { return nil, fmt.Errorf("Expected a Function, found: %s", typeName(fnVal)) }
                args := make([]Value, 0, len(ce.Arguments)+1)
                for _, a := range ce.Arguments { v, err := ev.evalExpr(a); if err != nil { return nil, err }; args = append(args, v) }
                args = append(args, cur)
                cur, err = f.call(ev, args)
                if err != nil { return nil, err }
            } else {
                v, err := ev.evalExpr(step); if err != nil { return nil, err }
                f, ok := v.(Function); if !ok { return nil, fmt.Errorf("Expected a Function, found: %s", typeName(v)) }
                cur, err = f.call(ev, []Value{cur})
                if err != nil { return nil, err }
            }
        }
        return cur, nil
    case parser.IndexExpr:
        left, err := ev.evalExpr(ex.Left)
        if err != nil { return nil, err }
        idxVal, err := ev.evalExpr(ex.Index)
        if err != nil { return nil, err }
        switch coll := left.(type) {
        case List:
            idx, ok := idxVal.(Int)
            if !ok { return nil, fmt.Errorf("Unable to perform index operation, found: List[%s]", typeName(idxVal)) }
            i := int(idx.V)
            if i < 0 { i = len(coll.Items) + i }
            if i < 0 || i >= len(coll.Items) { return Nil{}, nil }
            return coll.Items[i], nil
        case Str:
            idx, ok := idxVal.(Int)
            if !ok { return nil, fmt.Errorf("Unable to perform index operation, found: String[%s]", typeName(idxVal)) }
            i := int(idx.V)
            if i < 0 { i = len(coll.V) + i }
            if i < 0 || i >= len(coll.V) { return Nil{}, nil }
            return Str{V: coll.V[i : i+1]}, nil
        case Dict:
            if _, isDict := idxVal.(Dict); isDict { return nil, fmt.Errorf("Unable to use a Dictionary as a Dictionary key") }
            for _, e := range coll.Items { if equal(e.Key, idxVal) { return e.Val, nil } }
            return Nil{}, nil
        default:
            return Nil{}, nil
        }
    default:
        // For stage-3, other expressions are not used
        return Nil{}, nil
    }
}

func (ev *Evaluator) evalBlock(b parser.Block) (Value, error) {
    outer := ev.env
    ev.env = NewEnv(outer)
    defer func() { ev.env = outer }()
    var last Value = Nil{}
    for _, st := range b.Statements {
        v, err := ev.evalStmt(st)
        if err != nil { return nil, err }
        last = v
    }
    return last, nil
}

// user-defined function with closure environment
type userFunc struct {
    params []string
    body   parser.Block
    env    *Env
}

func (f *userFunc) repr() string { return "|...| { [function] }" }
func (f *userFunc) call(ev *Evaluator, args []Value) (Value, error) {
    if len(args) < len(f.params) {
        // partial application: bind provided args into a new env
        newEnv := NewEnv(f.env)
        for i, name := range f.params[:len(args)] { newEnv.Define(name, args[i], false) }
        return &userFunc{params: f.params[len(args):], body: f.body, env: newEnv}, nil
    }
    callEnv := NewEnv(f.env)
    // bind parameters (ignore extras)
    for i, name := range f.params {
        callEnv.Define(name, args[i], false)
    }
    // switch into function env
    saved := ev.env
    ev.env = callEnv
    defer func() { ev.env = saved }()
    return ev.evalBlock(f.body)
}

// composed function applying functions left-to-right, passing result forward
type composedFunc struct {
    functions []Function
}

func (c *composedFunc) repr() string { return "|...| { [composed] }" }
func (c *composedFunc) call(ev *Evaluator, args []Value) (Value, error) {
    if len(c.functions) == 0 { return Nil{}, nil }
    // apply first with provided args
    cur, err := c.functions[0].call(ev, args)
    if err != nil { return nil, err }
    for i := 1; i < len(c.functions); i++ {
        cur, err = c.functions[i].call(ev, []Value{cur})
        if err != nil { return nil, err }
    }
    return cur, nil
}

// Operations
func (ev *Evaluator) add(a, b Value) (Value, error) {
    switch x := a.(type) {
    case Int:
        switch y := b.(type) {
        case Int: return Int{V: x.V + y.V}, nil
        case Dec: return Dec{V: float64(x.V) + y.V}, nil
        case Str: return Str{V: fmt.Sprintf("%s%s", x.repr(), y.V)}, nil
        }
    case Dec:
        switch y := b.(type) {
        case Int: return Dec{V: x.V + float64(y.V)}, nil
        case Dec: return Dec{V: x.V + y.V}, nil
        case Str: return Str{V: fmt.Sprintf("%s%s", formatDecimal(x.V), y.V)}, nil
        }
    case Str:
        if y, ok := b.(Str); ok {
            return Str{V: x.V + y.V}, nil
        }
        return Str{V: x.V + b.repr()}, nil
    case List:
        if y, ok := b.(List); ok {
            out := make([]Value, 0, len(x.Items)+len(y.Items))
            out = append(out, x.Items...)
            out = append(out, y.Items...)
            return List{Items: out}, nil
        }
        return nil, fmt.Errorf("Unsupported operation: List + %s", typeName(b))
    case Set:
        if y, ok := b.(Set); ok {
            out := make([]Value, 0, len(x.Items)+len(y.Items))
            // union with structural equality
            addIfMissing := func(v Value) {
                for _, it := range out { if equal(it, v) { return } }
                out = append(out, v)
            }
            for _, it := range x.Items { addIfMissing(it) }
            for _, it := range y.Items { addIfMissing(it) }
            return Set{Items: out}, nil
        }
        return nil, fmt.Errorf("Unsupported operation: Set + %s", typeName(b))
    case Dict:
        if y, ok := b.(Dict); ok {
            // right-biased merge
            out := make([]dictEntry, 0, len(x.Items)+len(y.Items))
            for _, e := range x.Items { out = append(out, dictEntry{Key: e.Key, Val: e.Val}) }
            for _, e := range y.Items {
                replaced := false
                for i := range out {
                    if equal(out[i].Key, e.Key) { out[i].Val = e.Val; replaced = true; break }
                }
                if !replaced { out = append(out, dictEntry{Key: e.Key, Val: e.Val}) }
            }
            return Dict{Items: out}, nil
        }
        return nil, fmt.Errorf("Unsupported operation: Dictionary + %s", typeName(b))
    }
    return nil, fmt.Errorf("Unsupported operation: %s + %s", typeName(a), typeName(b))
}

func (ev *Evaluator) sub(a, b Value) (Value, error) {
    switch x := a.(type) {
    case Int:
        switch y := b.(type) {
        case Int: return Int{V: x.V - y.V}, nil
        case Dec: return Dec{V: float64(x.V) - y.V}, nil
        }
    case Dec:
        switch y := b.(type) {
        case Int: return Dec{V: x.V - float64(y.V)}, nil
        case Dec: return Dec{V: x.V - y.V}, nil
        }
    }
    return nil, fmt.Errorf("Unsupported operation: %s - %s", typeName(a), typeName(b))
}

func (ev *Evaluator) mul(a, b Value) (Value, error) {
    // String repetition
    if s, ok := a.(Str); ok {
        switch y := b.(type) {
        case Int:
            if y.V < 0 { return nil, fmt.Errorf("Unsupported operation: String * Integer (< 0)") }
            if y.V == 0 { return Str{V: ""}, nil }
            var bld strings.Builder
            for i := int64(0); i < y.V; i++ { bld.WriteString(s.V) }
            return Str{V: bld.String()}, nil
        case Dec:
            return nil, fmt.Errorf("Unsupported operation: String * Decimal")
        }
    }
    if s, ok := b.(Str); ok { // Integer * String
        return ev.mul(s, a)
    }
    switch x := a.(type) {
    case Int:
        switch y := b.(type) {
        case Int: return Int{V: x.V * y.V}, nil
        case Dec: return Dec{V: float64(x.V) * y.V}, nil
        }
    case Dec:
        switch y := b.(type) {
        case Int: return Dec{V: x.V * float64(y.V)}, nil
        case Dec: return Dec{V: x.V * y.V}, nil
        }
    }
    return nil, fmt.Errorf("Unsupported operation: %s * %s", typeName(a), typeName(b))
}

func (ev *Evaluator) div(a, b Value) (Value, error) {
    switch x := a.(type) {
    case Int:
        switch y := b.(type) {
        case Int:
            if y.V == 0 { return nil, fmt.Errorf("Division by zero") }
            // trunc toward zero
            return Int{V: x.V / y.V}, nil
        case Dec:
            if y.V == 0 { return nil, fmt.Errorf("Division by zero") }
            return Dec{V: float64(x.V) / y.V}, nil
        }
    case Dec:
        switch y := b.(type) {
        case Int:
            if y.V == 0 { return nil, fmt.Errorf("Division by zero") }
            return Dec{V: x.V / float64(y.V)}, nil
        case Dec:
            if y.V == 0 { return nil, fmt.Errorf("Division by zero") }
            return Dec{V: x.V / y.V}, nil
        }
    }
    return nil, fmt.Errorf("Unsupported operation: %s / %s", typeName(a), typeName(b))
}

func equal(a, b Value) bool { return compare(a, b) == 0 }

func compare(a, b Value) int {
    switch x := a.(type) {
    case Int:
        switch y := b.(type) {
        case Int: if x.V < y.V { return -1 } ; if x.V > y.V { return 1 }; return 0
        case Dec:
            f := float64(x.V)
            if f < y.V { return -1 } ; if f > y.V { return 1 }; return 0
        }
    case Dec:
        switch y := b.(type) {
        case Int:
            f := float64(y.V)
            if x.V < f { return -1 } ; if x.V > f { return 1 }; return 0
        case Dec:
            if x.V < y.V { return -1 } ; if x.V > y.V { return 1 }; return 0
        }
    case Str:
        if y, ok := b.(Str); ok {
            if x.V < y.V { return -1 } ; if x.V > y.V { return 1 }; return 0
        }
    case Bool:
        if y, ok := b.(Bool); ok {
            if !x.V && y.V { return -1 }
            if x.V && !y.V { return 1 }
            return 0
        }
    case Nil:
        if _, ok := b.(Nil); ok { return 0 }
    case List:
        if y, ok := b.(List); ok {
            // lexicographic compare by elements then length
            n := len(x.Items); m := len(y.Items)
            for i := 0; i < n && i < m; i++ {
                c := compare(x.Items[i], y.Items[i])
                if c != 0 { return c }
            }
            if n < m { return -1 } ; if n > m { return 1 } ; return 0
        }
    case Set:
        if y, ok := b.(Set); ok {
            // compare sorted elements lexicographically then length
            aItems := make([]Value, len(x.Items)); copy(aItems, x.Items); sort.Slice(aItems, func(i, j int) bool { return compare(aItems[i], aItems[j]) < 0 })
            bItems := make([]Value, len(y.Items)); copy(bItems, y.Items); sort.Slice(bItems, func(i, j int) bool { return compare(bItems[i], bItems[j]) < 0 })
            n := len(aItems); m := len(bItems)
            for i := 0; i < n && i < m; i++ {
                c := compare(aItems[i], bItems[i])
                if c != 0 { return c }
            }
            if n < m { return -1 } ; if n > m { return 1 } ; return 0
        }
    case Dict:
        if y, ok := b.(Dict); ok {
            // compare by sorted key-value pairs
            aItems := make([]dictEntry, len(x.Items)); copy(aItems, x.Items); sort.Slice(aItems, func(i, j int) bool { return compare(aItems[i].Key, aItems[j].Key) < 0 })
            bItems := make([]dictEntry, len(y.Items)); copy(bItems, y.Items); sort.Slice(bItems, func(i, j int) bool { return compare(bItems[i].Key, bItems[j].Key) < 0 })
            n := len(aItems); m := len(bItems)
            for i := 0; i < n && i < m; i++ {
                ck := compare(aItems[i].Key, bItems[i].Key)
                if ck != 0 { return ck }
                cv := compare(aItems[i].Val, bItems[i].Val)
                if cv != 0 { return cv }
            }
            if n < m { return -1 } ; if n > m { return 1 } ; return 0
        }
    }
    // Incomparable types: arbitrary but stable order by type name
    ta := typeName(a); tb := typeName(b)
    if ta < tb { return -1 } ; if ta > tb { return 1 } ; return 0
}

func isTruthy(v Value) bool {
    switch x := v.(type) {
    case Int: return x.V != 0
    case Dec: return x.V != 0
    case Str: return x.V != ""
    case Bool: return x.V
    case Nil: return false
    case List: return len(x.Items) > 0
    case Set: return len(x.Items) > 0
    case Dict: return len(x.Items) > 0
    default: return true
    }
}

func typeName(v Value) string {
    switch v.(type) {
    case Int: return "Integer"
    case Dec: return "Decimal"
    case Str: return "String"
    case Bool: return "Boolean"
    case Nil: return "Nil"
    case List: return "List"
    case Set: return "Set"
    case Dict: return "Dictionary"
    case Function: return "Function"
    default: return "Unknown"
    }
}

// internal type for dict items to keep stable order and keys of any Value
type dictEntry struct {
    Key Value
    Val Value
}
