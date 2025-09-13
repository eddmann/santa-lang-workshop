module ElfLang.Parser

open ElfLang.Lexer
open System.Text.Json
open System.Collections.Generic

// AST Types based on test expectations
type Expression =
    | Integer of value: string
    | Decimal of value: string
    | String of value: string
    | Boolean of value: bool
    | Nil
    | Identifier of name: string * ``type``: string
    | Let of name: Expression * value: Expression
    | MutableLet of name: Expression * value: Expression
    | Assignment of name: Expression * value: Expression
    | Infix of left: Expression * operator: string * right: Expression
    | Unary of operator: string * operand: Expression
    | Index of target: Expression * index: Expression
    | List of elements: Expression list
    | Set of elements: Expression list
    | Dictionary of entries: (Expression * Expression) list
    | If of condition: Expression * thenBranch: Block * elseBranch: Block option
    | Function of parameters: Expression list * body: Block
    | Call of func: Expression * arguments: Expression list
    | FunctionThread of initial: Expression * functions: Expression list
    | FunctionComposition of functions: Expression list

and Statement =
    | Expression of value: Expression
    | Comment of value: string

and Block = {
    statements: Statement list
    blockType: string  // Always "Block"
}

and Program = {
    statements: Statement list
    ``type``: string  // Always "Program"
}

// Token parser state
type Parser = {
    tokens: Token array
    current: int
}

// Parser helper functions
let peek (parser: Parser) =
    if parser.current < parser.tokens.Length then
        Some parser.tokens[parser.current]
    else
        None

let advance (parser: Parser) =
    { parser with current = parser.current + 1 }

let consume (parser: Parser) (expectedType: TokenType) =
    match peek parser with
    | Some token when token.Type = expectedType ->
        (token, advance parser)
    | Some token ->
        failwith $"Expected {expectedType}, found {token.Type}"
    | None ->
        failwith $"Expected {expectedType}, found EOF"

let check (parser: Parser) (tokenType: TokenType) =
    match peek parser with
    | Some token -> token.Type = tokenType
    | None -> false

let isAtEnd (parser: Parser) =
    parser.current >= parser.tokens.Length

// Precedence levels for operators (higher number = higher precedence)
let getOperatorPrecedence (op: string) =
    match op with
    | "|>" -> 0    // Threading has lowest precedence
    | "||" -> 1
    | "&&" -> 2
    | ">>" -> 2    // Function composition
    | "==" | "!=" | ">" | "<" | ">=" | "<=" -> 3
    | "+" | "-" -> 4
    | "*" | "/" -> 5
    | _ -> 0

// Parse primary expressions (literals, identifiers, parentheses)
let rec parsePrimary (parser: Parser) : Expression * Parser =
    match peek parser with
    | Some { Type = INT; Value = value } ->
        (Integer value, advance parser)
    | Some { Type = DEC; Value = value } ->
        (Decimal value, advance parser)
    | Some { Type = STR; Value = value } ->
        // Remove quotes and unescape string value for AST
        let quotesRemoved = value.Substring(1, value.Length - 2)
        let unescapedValue = quotesRemoved.Replace("\\\"", "\"").Replace("\\\\", "\\").Replace("\\n", "\n").Replace("\\t", "\t")
        (String unescapedValue, advance parser)
    | Some { Type = TRUE; Value = _ } ->
        (Boolean true, advance parser)
    | Some { Type = FALSE; Value = _ } ->
        (Boolean false, advance parser)
    | Some { Type = NIL; Value = _ } ->
        (Nil, advance parser)
    | Some { Type = ID; Value = name } ->
        (Identifier (name, "Identifier"), advance parser)
    | Some token when (token.Type = PLUS || token.Type = MINUS) ->
        // Check if this is a unary operator or an operator used as identifier
        let parser = advance parser
        // If followed by a primary expression (but NOT LPAREN for function calls), treat as unary operator
        if not (isAtEnd parser) then
            let nextToken = peek parser
            match nextToken with
            | Some { Type = LPAREN } ->
                // Function call like +(1, 2), treat as identifier
                (Identifier (token.Value, "Identifier"), parser)
            | Some { Type = INT | DEC | STR | TRUE | FALSE | NIL | ID | LBRACKET | LBRACE | LHASHBRACE | PIPE | IF | LET } ->
                // This is a unary operator
                let (operand, parser) = parsePrimary parser
                (Unary (token.Value, operand), parser)
            | _ ->
                // Not followed by a primary expression, treat as identifier
                (Identifier (token.Value, "Identifier"), parser)
        else
            // End of input, treat as identifier
            (Identifier (token.Value, "Identifier"), parser)
    | Some token when (token.Type = MULT || token.Type = DIV) ->
        // Multiplication and division are always identifiers (can't be unary)
        (Identifier (token.Value, "Identifier"), advance parser)
    | Some { Type = LPAREN; Value = _ } ->
        let parser = advance parser
        let (expr, parser) = parseExpression parser
        let (_, parser) = consume parser RPAREN
        (expr, parser)
    | Some { Type = LBRACKET; Value = _ } ->
        parseList parser
    | Some { Type = LBRACE; Value = _ } ->
        parseSet parser
    | Some { Type = LHASHBRACE; Value = _ } ->
        parseDictionary parser
    | Some { Type = PIPE; Value = _ } ->
        parseFunction parser
    | Some { Type = OR_OR; Value = _ } ->
        parseEmptyParameterFunction parser
    | Some { Type = IF; Value = _ } ->
        parseIf parser
    | Some { Type = LET; Value = _ } ->
        parseLet parser
    | Some token ->
        failwith $"Unexpected token in primary expression: {token.Type} '{token.Value}'"
    | None ->
        failwith "Expected expression, found EOF"

// Parse list literals [1, 2, 3]
and parseList (parser: Parser) : Expression * Parser =
    let (_, parser) = consume parser LBRACKET
    let rec parseElements parser acc =
        if check parser RBRACKET then
            (List.rev acc, parser)
        else
            let (expr, parser) = parseExpression parser
            let acc = expr :: acc
            if check parser COMMA then
                let parser = advance parser
                parseElements parser acc
            else
                (List.rev acc, parser)

    let (elements, parser) = parseElements parser []
    let (_, parser) = consume parser RBRACKET
    (List elements, parser)

// Parse set literals {1, 2, 3}
and parseSet (parser: Parser) : Expression * Parser =
    let (_, parser) = consume parser LBRACE
    let rec parseElements parser acc =
        if check parser RBRACE then
            (List.rev acc, parser)
        else
            let (expr, parser) = parseExpression parser
            let acc = expr :: acc
            if check parser COMMA then
                let parser = advance parser
                parseElements parser acc
            else
                (List.rev acc, parser)

    let (elements, parser) = parseElements parser []
    let (_, parser) = consume parser RBRACE
    (Set elements, parser)

// Parse dictionary literals #{"a": 1, "b": 2}
and parseDictionary (parser: Parser) : Expression * Parser =
    let (_, parser) = consume parser LHASHBRACE
    let rec parseEntries parser acc =
        if check parser RBRACE then
            (List.rev acc, parser)
        else
            let (key, parser) = parseExpression parser
            let (_, parser) = consume parser COLON // Need to add COLON token
            let (value, parser) = parseExpression parser
            let acc = (key, value) :: acc
            if check parser COMMA then
                let parser = advance parser
                parseEntries parser acc
            else
                (List.rev acc, parser)

    let (entries, parser) = parseEntries parser []
    let (_, parser) = consume parser RBRACE
    (Dictionary entries, parser)

// Parse function literals |x, y| expr or |x| { ... }
and parseFunction (parser: Parser) : Expression * Parser =
    let (_, parser) = consume parser PIPE

    // Parse parameters
    let rec parseParameters parser acc =
        if check parser PIPE then
            (List.rev acc, parser)
        else
            match peek parser with
            | Some { Type = ID; Value = name } ->
                let parser = advance parser
                let param = Identifier (name, "Identifier")
                let acc = param :: acc
                if check parser COMMA then
                    let parser = advance parser
                    parseParameters parser acc
                else
                    (List.rev acc, parser)
            | _ ->
                failwith "Expected parameter name"

    let (parameters, parser) = parseParameters parser []
    let (_, parser) = consume parser PIPE

    // Parse body - could be expression or block
    let (body, parser) =
        if check parser LBRACE then
            parseBlock parser
        else
            // Single expression - wrap in block
            let (expr, parser) = parseExpression parser
            let stmt = Expression expr
            let block = { statements = [stmt]; blockType = "Block" }
            (block, parser)

    (Function (parameters, body), parser)

// Parse empty parameter function || { ... }
and parseEmptyParameterFunction (parser: Parser) : Expression * Parser =
    let (_, parser) = consume parser OR_OR

    // Parse body - should be a block
    let (body, parser) =
        if check parser LBRACE then
            parseBlock parser
        else
            // Single expression - wrap in block
            let (expr, parser) = parseExpression parser
            let stmt = Expression expr
            let block = { statements = [stmt]; blockType = "Block" }
            (block, parser)

    (Function ([], body), parser)

// Parse if expressions
and parseIf (parser: Parser) : Expression * Parser =
    let (_, parser) = consume parser IF
    let (condition, parser) = parseExpression parser
    let (thenBlock, parser) = parseBlock parser
    let (elseBlock, parser) =
        if check parser ELSE then
            let parser = advance parser
            let (block, parser) = parseBlock parser
            (Some block, parser)
        else
            (None, parser)

    (If (condition, thenBlock, elseBlock), parser)

// Parse let declarations
and parseLet (parser: Parser) : Expression * Parser =
    let (_, parser) = consume parser LET
    let isMutable = check parser MUT
    let parser = if isMutable then advance parser else parser

    match peek parser with
    | Some { Type = ID; Value = name } ->
        let parser = advance parser
        let (_, parser) = consume parser ASSIGN
        let (value, parser) = parseExpression parser
        let identifier = Identifier (name, "Identifier")
        let expr = if isMutable then MutableLet (identifier, value) else Let (identifier, value)
        (expr, parser)
    | _ ->
        failwith "Expected identifier after 'let'"

// Parse block { ... }
and parseBlock (parser: Parser) : Block * Parser =
    let (_, parser) = consume parser LBRACE
    let rec parseStatements parser acc =
        if check parser RBRACE then
            (List.rev acc, parser)
        else
            let (stmt, parser) = parseStatement parser
            let acc = stmt :: acc
            // Optional semicolon
            let parser = if check parser SEMICOLON then advance parser else parser
            parseStatements parser acc

    let (statements, parser) = parseStatements parser []
    let (_, parser) = consume parser RBRACE
    ({ statements = statements; blockType = "Block" }, parser)

// Parse binary expressions with precedence
and parseBinaryExpression (parser: Parser) (left: Expression) (minPrecedence: int) : Expression * Parser =
    let rec loop parser left =
        match peek parser with
        | Some token when (token.Type = PLUS || token.Type = MINUS || token.Type = MULT || token.Type = DIV ||
                          token.Type = EQ_EQ || token.Type = NOT_EQ || token.Type = GT || token.Type = LT ||
                          token.Type = GTE || token.Type = LTE || token.Type = AND_AND || token.Type = OR_OR ||
                          token.Type = PIPE_GT || token.Type = GT_GT) ->
            let operator = token.Value
            let precedence = getOperatorPrecedence operator
            if precedence >= minPrecedence then
                let parser = advance parser
                let (right, parser) = parsePrimary parser
                let (right, parser) = parsePostfix parser right
                // Use precedence + 1 for left-associative operators
                let (right, parser) = parseBinaryExpression parser right (precedence + 1)

                // Create appropriate AST node based on operator
                let expr =
                    match operator with
                    | "|>" ->
                        // Handle threading - collect all functions in the chain
                        match left with
                        | FunctionThread (initial, funcs) -> FunctionThread (initial, funcs @ [right])
                        | _ -> FunctionThread (left, [right])
                    | ">>" ->
                        // Handle function composition - collect all functions in the chain
                        match left with
                        | FunctionComposition funcs -> FunctionComposition (funcs @ [right])
                        | _ -> FunctionComposition [left; right]
                    | _ -> Infix (left, operator, right)

                loop parser expr
            else
                (left, parser)
        | _ ->
            (left, parser)

    loop parser left

// Parse postfix expressions (like indexing and function calls)
and parsePostfix (parser: Parser) (expr: Expression) : Expression * Parser =
    match peek parser with
    | Some { Type = LBRACKET; Value = _ } ->
        let parser = advance parser
        let (index, parser) = parseExpression parser
        let (_, parser) = consume parser RBRACKET
        let indexExpr = Index (expr, index)
        parsePostfix parser indexExpr
    | Some { Type = LPAREN; Value = _ } ->
        // Function call: expr(arg1, arg2, ...)
        let parser = advance parser
        let rec parseArguments parser acc =
            if check parser RPAREN then
                (List.rev acc, parser)
            else
                let (arg, parser) = parseExpression parser
                let acc = arg :: acc
                if check parser COMMA then
                    let parser = advance parser
                    parseArguments parser acc
                else
                    (List.rev acc, parser)

        let (arguments, parser) = parseArguments parser []
        let (_, parser) = consume parser RPAREN
        let callExpr = Call (expr, arguments)
        parsePostfix parser callExpr
    | _ ->
        (expr, parser)

// Parse expressions
and parseExpression (parser: Parser) : Expression * Parser =
    let (left, parser) = parsePrimary parser
    let (left, parser) = parsePostfix parser left
    parseBinaryExpression parser left 0

// Parse statements
and parseStatement (parser: Parser) : Statement * Parser =
    // Check for assignment: identifier = expression
    if check parser ID then
        let identifierToken = parser.tokens[parser.current]
        let parser = advance parser
        if check parser ASSIGN then
            let parser = advance parser
            let (value, parser) = parseExpression parser
            let identifier = Identifier (identifierToken.Value, "Identifier")
            let assignmentExpr = Assignment (identifier, value)
            (Expression assignmentExpr, parser)
        else
            // Not an assignment, backtrack and parse as normal expression
            let parser = { parser with current = parser.current - 1 }
            let (expr, parser) = parseExpression parser
            (Expression expr, parser)
    else
        let (expr, parser) = parseExpression parser
        (Expression expr, parser)

// Main parse function
let parse (tokens: Token list) : Program =
    let parser = { tokens = List.toArray tokens; current = 0 }

    let rec parseStatements parser acc =
        if isAtEnd parser then
            List.rev acc
        else
            match peek parser with
            | Some { Type = CMT; Value = value } ->
                // Parse comment as a statement
                let parser = advance parser
                let commentStmt = Comment value
                let acc = commentStmt :: acc
                parseStatements parser acc
            | _ ->
                let (stmt, parser) = parseStatement parser
                let acc = stmt :: acc
                // Optional semicolon
                let parser = if check parser SEMICOLON then advance parser else parser
                parseStatements parser acc

    let statements = parseStatements parser []
    { statements = statements; ``type`` = "Program" }

// JSON Serialization functions for AST (manual approach for proper formatting)
let rec expressionToJson (expr: Expression) : string =
    match expr with
    | Integer value ->
        $"{{\"type\":\"Integer\",\"value\":\"{value}\"}}"
    | Decimal value ->
        $"{{\"type\":\"Decimal\",\"value\":\"{value}\"}}"
    | String value ->
        // The value is already unescaped from the lexer, just need to escape for JSON
        let escapedValue = value.Replace("\\", "\\\\").Replace("\"", "\\\"")
        $"{{\"type\":\"String\",\"value\":\"{escapedValue}\"}}"
    | Boolean value ->
        sprintf "{\"type\":\"Boolean\",\"value\":%s}" (if value then "true" else "false")
    | Nil ->
        "{\"type\":\"Nil\"}"
    | Identifier (name, _) ->
        $"{{\"name\":\"{name}\",\"type\":\"Identifier\"}}"
    | Let (name, value) ->
        let nameJson = expressionToJson name
        let valueJson = expressionToJson value
        $"{{\"name\":{nameJson},\"type\":\"Let\",\"value\":{valueJson}}}"
    | MutableLet (name, value) ->
        let nameJson = expressionToJson name
        let valueJson = expressionToJson value
        $"{{\"name\":{nameJson},\"type\":\"MutableLet\",\"value\":{valueJson}}}"
    | Assignment (name, value) ->
        let nameJson = expressionToJson name
        let valueJson = expressionToJson value
        $"{{\"name\":{nameJson},\"type\":\"Assignment\",\"value\":{valueJson}}}"
    | Infix (left, op, right) ->
        let leftJson = expressionToJson left
        let rightJson = expressionToJson right
        $"{{\"left\":{leftJson},\"operator\":\"{op}\",\"right\":{rightJson},\"type\":\"Infix\"}}"
    | Unary (op, operand) ->
        let operandJson = expressionToJson operand
        $"{{\"operand\":{operandJson},\"operator\":\"{op}\",\"type\":\"Unary\"}}"
    | List elements ->
        let elementsJson = elements |> List.map expressionToJson |> String.concat ","
        $"{{\"items\":[{elementsJson}],\"type\":\"List\"}}"
    | Set elements ->
        let elementsJson = elements |> List.map expressionToJson |> String.concat ","
        $"{{\"items\":[{elementsJson}],\"type\":\"Set\"}}"
    | Dictionary entries ->
        let entriesJson = entries |> List.map (fun (k, v) -> $"{{\"key\":{expressionToJson k},\"value\":{expressionToJson v}}}") |> String.concat ","
        $"{{\"items\":[{entriesJson}],\"type\":\"Dictionary\"}}"
    | Function (parameters, body) ->
        let parametersJson = parameters |> List.map expressionToJson |> String.concat ","
        let bodyJson = blockToJson body
        $"{{\"body\":{bodyJson},\"parameters\":[{parametersJson}],\"type\":\"Function\"}}"
    | If (condition, thenBranch, elseBranch) ->
        let conditionJson = expressionToJson condition
        let thenJson = blockToJson thenBranch
        match elseBranch with
        | Some elseBlock ->
            let elseJson = blockToJson elseBlock
            $"{{\"alternative\":{elseJson},\"condition\":{conditionJson},\"consequence\":{thenJson},\"type\":\"If\"}}"
        | None ->
            $"{{\"condition\":{conditionJson},\"consequence\":{thenJson},\"type\":\"If\"}}"
    | Call (func, arguments) ->
        let funcJson = expressionToJson func
        let argsJson = arguments |> List.map expressionToJson |> String.concat ","
        $"{{\"arguments\":[{argsJson}],\"function\":{funcJson},\"type\":\"Call\"}}"
    | Index (target, index) ->
        let targetJson = expressionToJson target
        let indexJson = expressionToJson index
        $"{{\"index\":{indexJson},\"left\":{targetJson},\"type\":\"Index\"}}"
    | FunctionThread (initial, functions) ->
        let initialJson = expressionToJson initial
        let functionsJson = functions |> List.map expressionToJson |> String.concat ","
        $"{{\"functions\":[{functionsJson}],\"initial\":{initialJson},\"type\":\"FunctionThread\"}}"
    | FunctionComposition (functions) ->
        let functionsJson = functions |> List.map expressionToJson |> String.concat ","
        $"{{\"functions\":[{functionsJson}],\"type\":\"FunctionComposition\"}}"

and blockToJson (block: Block) : string =
    let statementsJson = block.statements |> List.map statementToJson |> String.concat ","
    $"{{\"statements\":[{statementsJson}],\"type\":\"Block\"}}"

and statementToJson (stmt: Statement) : string =
    match stmt with
    | Expression expr ->
        let exprJson = expressionToJson expr
        $"{{\"type\":\"Expression\",\"value\":{exprJson}}}"
    | Comment value ->
        $"{{\"type\":\"Comment\",\"value\":\"{value}\"}}"

let programToJson (program: Program) : string =
    let statementsJson = program.statements |> List.map statementToJson |> String.concat ","
    $"{{\"statements\":[{statementsJson}],\"type\":\"Program\"}}"

// Pretty print JSON with 2-space indentation
let prettyPrintJson (json: string) : string =
    let doc = JsonDocument.Parse(json)
    use stream = new System.IO.MemoryStream()
    let options = JsonWriterOptions(Indented = true, Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping)
    use writer = new Utf8JsonWriter(stream, options)
    doc.WriteTo(writer)
    writer.Flush()
    let result = System.Text.Encoding.UTF8.GetString(stream.ToArray())

    // Convert from 4-space to 2-space indentation - handle all levels properly
    let lines = result.Split('\n')
    let convertedLines = lines |> Array.map (fun line ->
        let leadingSpaces = line.Length - line.TrimStart(' ').Length
        let newLeadingSpaces = leadingSpaces / 2
        String.replicate newLeadingSpaces "  " + line.TrimStart(' ')
    )
    let converted = String.concat "\n" convertedLines

    // Fix Unicode surrogate pairs back to proper Unicode characters
    let unicodeFixed = System.Text.RegularExpressions.Regex.Replace(
        converted,
        @"\\u([0-9A-Fa-f]{4})\\u([0-9A-Fa-f]{4})",
        fun m ->
            let high = System.Convert.ToInt32(m.Groups.[1].Value, 16)
            let low = System.Convert.ToInt32(m.Groups.[2].Value, 16)
            if high >= 0xD800 && high <= 0xDBFF && low >= 0xDC00 && low <= 0xDFFF then
                // This is a surrogate pair, convert to actual Unicode character
                let codepoint = 0x10000 + ((high &&& 0x3FF) <<< 10) + (low &&& 0x3FF)
                System.Char.ConvertFromUtf32(codepoint)
            else
                m.Value
    )
    unicodeFixed