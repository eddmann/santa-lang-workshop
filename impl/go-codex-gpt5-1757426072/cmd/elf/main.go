package main

import (
    "bufio"
    "encoding/json"
    "fmt"
    "os"
    "path/filepath"

    "elf-lang/impl/internal/lexer"
    "elf-lang/impl/internal/evaluator"
    "elf-lang/impl/internal/parser"
)

type tokenOut struct {
    Type  string `json:"type"`
    Value string `json:"value"`
}

func printTokens(path string) error {
    data, err := os.ReadFile(path)
    if err != nil {
        return err
    }
    toks := lexer.Lex(string(data))
    enc := json.NewEncoder(os.Stdout)
    enc.SetEscapeHTML(false)
    // json.Encoder by default emits minified JSON
    for _, t := range toks {
        if err := enc.Encode(tokenOut{Type: t.Type, Value: t.Lit}); err != nil {
            return err
        }
    }
    return nil
}

func printAST(path string) error {
    data, err := os.ReadFile(path)
    if err != nil { return err }
    toks := lexer.Lex(string(data))
    p := parser.New(toks)
    prog := p.ParseProgram()
    w := bufio.NewWriter(os.Stdout)
    enc := json.NewEncoder(w)
    enc.SetEscapeHTML(false)
    enc.SetIndent("", "  ")
    if err := enc.Encode(prog); err != nil { return err }
    return w.Flush()
}

func runProgram(path string) error {
    data, err := os.ReadFile(path)
    if err != nil { return err }
    toks := lexer.Lex(string(data))
    p := parser.New(toks)
    prog := p.ParseProgram()
    ev := evaluator.New(os.Stdout)
    val, err := ev.Eval(prog)
    if err != nil { return err }
    // Print only the value of the last top-level statement
    fmt.Fprintln(os.Stdout, evaluator.Format(val))
    return nil
}

func usage(prog string) {
    fmt.Fprintf(os.Stdout, "Usage: %s [tokens|ast] <file>\n", filepath.Base(prog))
}

func main() {
    args := os.Args
    if len(args) < 2 {
        usage(args[0])
        return
    }
    // Subcommands: tokens <file>, ast <file>; default: run <file>
    if args[1] == "tokens" {
        if len(args) < 3 {
            usage(args[0])
            return
        }
        if err := printTokens(args[2]); err != nil { fmt.Fprintln(os.Stdout, "[Error]", err) }
        return
    }
    if args[1] == "ast" {
        if len(args) < 3 {
            usage(args[0])
            return
        }
        if err := printAST(args[2]); err != nil { fmt.Fprintln(os.Stdout, "[Error]", err) }
        return
    }
    // Default: run program
    if err := runProgram(args[1]); err != nil { fmt.Fprintln(os.Stdout, "[Error]", err) }
}
