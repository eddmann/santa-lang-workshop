module ElfLang.Lexer

open System
open System.Text
open System.Text.Json

type TokenType =
    | INT | DEC | STR | TRUE | FALSE | NIL
    | ID | LET | MUT | IF | ELSE
    | PLUS | MINUS | MULT | DIV | EQ | ASSIGN
    | LBRACE | RBRACE | LBRACKET | RBRACKET | LHASHBRACE
    | EQ_EQ | NOT_EQ | GT | LT | GTE | LTE
    | AND_AND | OR_OR | PIPE_GT | GT_GT | SEMICOLON
    | LPAREN | RPAREN | COMMA | PIPE | COLON | CMT

type Token = {
    Type: TokenType
    Value: string
}

let isLetter (c: char) = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c = '_'
let isDigit (c: char) = c >= '0' && c <= '9'
let isWhitespace (c: char) = c = ' ' || c = '\t' || c = '\n' || c = '\r'

let tokenTypeToString (tokenType: TokenType) =
    match tokenType with
    | INT -> "INT" | DEC -> "DEC" | STR -> "STR" | TRUE -> "TRUE" | FALSE -> "FALSE" | NIL -> "NIL"
    | ID -> "ID" | LET -> "LET" | MUT -> "MUT" | IF -> "IF" | ELSE -> "ELSE"
    | PLUS -> "+" | MINUS -> "-" | MULT -> "*" | DIV -> "/" | EQ -> "EQ" | ASSIGN -> "="
    | LBRACE -> "{" | RBRACE -> "}" | LBRACKET -> "[" | RBRACKET -> "]" | LHASHBRACE -> "#{"
    | EQ_EQ -> "==" | NOT_EQ -> "!=" | GT -> ">" | LT -> "<" | GTE -> ">=" | LTE -> "<="
    | AND_AND -> "&&" | OR_OR -> "||" | PIPE_GT -> "|>" | GT_GT -> ">>" | SEMICOLON -> ";"
    | LPAREN -> "(" | RPAREN -> ")" | COMMA -> "," | PIPE -> "|" | COLON -> ":" | CMT -> "CMT"

let rec tokenize (input: string) =
    let chars = input.ToCharArray()
    let length = chars.Length

    let rec tokenizeInner (pos: int) (acc: Token list) =
        if pos >= length then List.rev acc
        else
            let c = chars.[pos]

            if isWhitespace c then
                tokenizeInner (pos + 1) acc
            elif c = '/' && pos + 1 < length && chars.[pos + 1] = '/' then
                // Line comment
                let commentStart = pos
                let rec findLineEnd p =
                    if p >= length || chars.[p] = '\n' then p
                    else findLineEnd (p + 1)
                let commentEnd = findLineEnd pos
                let commentValue = String(chars, commentStart, commentEnd - commentStart)
                let token = { Type = CMT; Value = commentValue }
                tokenizeInner commentEnd (token :: acc)
            elif isDigit c then
                // Number (INT or DEC)
                tokenizeNumber pos acc
            elif c = '"' then
                // String
                tokenizeString pos acc
            elif isLetter c then
                // Identifier or keyword
                tokenizeIdentifier pos acc
            else
                // Operators and symbols
                tokenizeSymbol pos acc

    and tokenizeNumber (pos: int) (acc: Token list) =
        let startPos = pos
        let rec consumeDigitsWithUnderscores p hasDecimal =
            if p >= length then p
            elif isDigit chars.[p] then consumeDigitsWithUnderscores (p + 1) hasDecimal
            elif chars.[p] = '_' && p + 1 < length && isDigit chars.[p + 1] then
                consumeDigitsWithUnderscores (p + 2) hasDecimal  // Skip underscore and continue with next digit
            elif chars.[p] = '.' && not hasDecimal && p + 1 < length && isDigit chars.[p + 1] then
                consumeDigitsWithUnderscores (p + 2) true  // Include decimal point and next digit
            else p

        let endPos = consumeDigitsWithUnderscores pos false
        let value = String(chars, startPos, endPos - startPos)
        let tokenType = if value.Contains('.') then DEC else INT
        let token = { Type = tokenType; Value = value }
        tokenizeInner endPos (token :: acc)

    and tokenizeString (pos: int) (acc: Token list) =
        let startPos = pos  // Include opening quote
        let rec consumeString p (sb: StringBuilder) =
            if p >= length then
                failwith "Unterminated string"
            elif chars.[p] = '"' then
                sb.Append('"') |> ignore  // Include closing quote
                (p + 1, sb.ToString())
            elif chars.[p] = '\\' && p + 1 < length then
                match chars.[p + 1] with
                | 'n' -> sb.Append("\\n") |> ignore; consumeString (p + 2) sb
                | 't' -> sb.Append("\\t") |> ignore; consumeString (p + 2) sb
                | '"' -> sb.Append("\\\"") |> ignore; consumeString (p + 2) sb
                | '\\' -> sb.Append("\\\\") |> ignore; consumeString (p + 2) sb
                | _ -> sb.Append(chars.[p]) |> ignore; consumeString (p + 1) sb
            else
                sb.Append(chars.[p]) |> ignore; consumeString (p + 1) sb

        let sb = StringBuilder()
        sb.Append('"') |> ignore  // Include opening quote
        let (endPos, value) = consumeString (pos + 1) sb
        let token = { Type = STR; Value = value }
        tokenizeInner endPos (token :: acc)

    and tokenizeIdentifier (pos: int) (acc: Token list) =
        let startPos = pos
        let rec consumeIdentifier p =
            if p >= length then p
            elif isLetter chars.[p] || isDigit chars.[p] then consumeIdentifier (p + 1)
            else p

        let endPos = consumeIdentifier pos
        let value = String(chars, startPos, endPos - startPos)
        let tokenType =
            match value with
            | "let" -> LET | "mut" -> MUT | "if" -> IF | "else" -> ELSE
            | "true" -> TRUE | "false" -> FALSE | "nil" -> NIL
            | _ -> ID
        let token = { Type = tokenType; Value = value }
        tokenizeInner endPos (token :: acc)

    and tokenizeSymbol (pos: int) (acc: Token list) =
        let c = chars.[pos]
        match c with
        | '+' ->
            let token = { Type = PLUS; Value = "+" }
            tokenizeInner (pos + 1) (token :: acc)
        | '-' ->
            let token = { Type = MINUS; Value = "-" }
            tokenizeInner (pos + 1) (token :: acc)
        | '*' ->
            let token = { Type = MULT; Value = "*" }
            tokenizeInner (pos + 1) (token :: acc)
        | '/' ->
            let token = { Type = DIV; Value = "/" }
            tokenizeInner (pos + 1) (token :: acc)
        | '=' when pos + 1 < length && chars.[pos + 1] = '=' ->
            let token = { Type = EQ_EQ; Value = "==" }
            tokenizeInner (pos + 2) (token :: acc)
        | '=' ->
            let token = { Type = ASSIGN; Value = "=" }
            tokenizeInner (pos + 1) (token :: acc)
        | '!' when pos + 1 < length && chars.[pos + 1] = '=' ->
            let token = { Type = NOT_EQ; Value = "!=" }
            tokenizeInner (pos + 2) (token :: acc)
        | '>' when pos + 1 < length && chars.[pos + 1] = '=' ->
            let token = { Type = GTE; Value = ">=" }
            tokenizeInner (pos + 2) (token :: acc)
        | '>' when pos + 1 < length && chars.[pos + 1] = '>' ->
            let token = { Type = GT_GT; Value = ">>" }
            tokenizeInner (pos + 2) (token :: acc)
        | '>' ->
            let token = { Type = GT; Value = ">" }
            tokenizeInner (pos + 1) (token :: acc)
        | '<' when pos + 1 < length && chars.[pos + 1] = '=' ->
            let token = { Type = LTE; Value = "<=" }
            tokenizeInner (pos + 2) (token :: acc)
        | '<' ->
            let token = { Type = LT; Value = "<" }
            tokenizeInner (pos + 1) (token :: acc)
        | '&' when pos + 1 < length && chars.[pos + 1] = '&' ->
            let token = { Type = AND_AND; Value = "&&" }
            tokenizeInner (pos + 2) (token :: acc)
        | '|' when pos + 1 < length && chars.[pos + 1] = '>' ->
            let token = { Type = PIPE_GT; Value = "|>" }
            tokenizeInner (pos + 2) (token :: acc)
        | '|' when pos + 1 < length && chars.[pos + 1] = '|' ->
            let token = { Type = OR_OR; Value = "||" }
            tokenizeInner (pos + 2) (token :: acc)
        | '|' ->
            let token = { Type = PIPE; Value = "|" }
            tokenizeInner (pos + 1) (token :: acc)
        | '#' when pos + 1 < length && chars.[pos + 1] = '{' ->
            let token = { Type = LHASHBRACE; Value = "#{" }
            tokenizeInner (pos + 2) (token :: acc)
        | '{' ->
            let token = { Type = LBRACE; Value = "{" }
            tokenizeInner (pos + 1) (token :: acc)
        | '}' ->
            let token = { Type = RBRACE; Value = "}" }
            tokenizeInner (pos + 1) (token :: acc)
        | '[' ->
            let token = { Type = LBRACKET; Value = "[" }
            tokenizeInner (pos + 1) (token :: acc)
        | ']' ->
            let token = { Type = RBRACKET; Value = "]" }
            tokenizeInner (pos + 1) (token :: acc)
        | '(' ->
            let token = { Type = LPAREN; Value = "(" }
            tokenizeInner (pos + 1) (token :: acc)
        | ')' ->
            let token = { Type = RPAREN; Value = ")" }
            tokenizeInner (pos + 1) (token :: acc)
        | ',' ->
            let token = { Type = COMMA; Value = "," }
            tokenizeInner (pos + 1) (token :: acc)
        | ';' ->
            let token = { Type = SEMICOLON; Value = ";" }
            tokenizeInner (pos + 1) (token :: acc)
        | ':' ->
            let token = { Type = COLON; Value = ":" }
            tokenizeInner (pos + 1) (token :: acc)
        | _ ->
            failwith $"Unexpected character: {c}"

    tokenizeInner 0 []

let formatTokenAsJsonLine (token: Token) =
    // Manual JSON construction to avoid Unicode escaping issues
    let typeString = tokenTypeToString token.Type
    let escapedValue =
        token.Value
            .Replace("\\", "\\\\")  // Escape backslashes first
            .Replace("\"", "\\\"")  // Escape quotes
    $"{{\"type\":\"{typeString}\",\"value\":\"{escapedValue}\"}}"