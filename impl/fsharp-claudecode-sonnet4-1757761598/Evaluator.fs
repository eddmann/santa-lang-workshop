module ElfLang.Evaluator

open ElfLang.Parser
open System.Collections.Generic
open System.Text

// Runtime Value types
type Value =
    | IntValue of int64
    | DecimalValue of float
    | StringValue of string
    | BoolValue of bool
    | NilValue
    | ListValue of Value list
    | SetValue of Value list
    | DictValue of (Value * Value) list
    | FunctionValue of parameters: Expression list * body: Block * closure: Environment
    | CompositionValue of functions: Expression list * closure: Environment

// Variable environment for scoping
and Environment = Dictionary<string, VariableInfo>
and VariableInfo = { valueRef: Value ref; isMutable: bool }

// Exception for runtime errors
exception RuntimeError of string

// Output buffer for program execution
let mutable outputBuffer = StringBuilder()

// Comparison for Value types (needed for Sets/Dicts sorting)
let rec compareValues (a: Value) (b: Value) : int =
    match a, b with
    | IntValue x, IntValue y -> compare x y
    | DecimalValue x, DecimalValue y -> compare x y
    | StringValue x, StringValue y -> compare x y
    | BoolValue x, BoolValue y -> compare x y
    | NilValue, NilValue -> 0
    | IntValue _, _ -> -1
    | _, IntValue _ -> 1
    | DecimalValue _, _ -> -1
    | _, DecimalValue _ -> 1
    | StringValue _, _ -> -1
    | _, StringValue _ -> 1
    | BoolValue _, _ -> -1
    | _, BoolValue _ -> 1
    | NilValue, _ -> -1
    | _, NilValue -> 1
    | ListValue xs, ListValue ys ->
        let rec compareList l1 l2 =
            match l1, l2 with
            | [], [] -> 0
            | [], _ -> -1
            | _, [] -> 1
            | x::xs, y::ys ->
                let cmp = compareValues x y
                if cmp = 0 then compareList xs ys else cmp
        compareList xs ys
    | ListValue _, _ -> -1
    | _, ListValue _ -> 1
    | SetValue xs, SetValue ys ->
        let rec compareList l1 l2 =
            match l1, l2 with
            | [], [] -> 0
            | [], _ -> -1
            | _, [] -> 1
            | x::xs, y::ys ->
                let cmp = compareValues x y
                if cmp = 0 then compareList xs ys else cmp
        compareList xs ys
    | SetValue _, _ -> -1
    | _, SetValue _ -> 1
    | DictValue xs, DictValue ys ->
        let rec compareList l1 l2 =
            match l1, l2 with
            | [], [] -> 0
            | [], _ -> -1
            | _, [] -> 1
            | (k1, v1)::xs, (k2, v2)::ys ->
                let kcmp = compareValues k1 k2
                if kcmp = 0 then
                    let vcmp = compareValues v1 v2
                    if vcmp = 0 then compareList xs ys else vcmp
                else kcmp
        compareList xs ys
    | DictValue _, _ -> -1
    | _, DictValue _ -> 1
    | FunctionValue _, FunctionValue _ -> 0
    | FunctionValue _, _ -> -1
    | _, FunctionValue _ -> 1
    | CompositionValue _, CompositionValue _ -> 0

// Convert Value to string representation for output
let rec valueToString (value: Value) : string =
    match value with
    | IntValue i -> string i
    | DecimalValue f -> string f
    | StringValue s -> sprintf "\"%s\"" s
    | BoolValue b -> if b then "true" else "false"
    | NilValue -> "nil"
    | ListValue values ->
        let items = values |> List.map valueToString |> String.concat ", "
        sprintf "[%s]" items
    | SetValue values ->
        let sortedValues = values |> List.sortWith compareValues
        let items = sortedValues |> List.map valueToString |> String.concat ", "
        sprintf "{%s}" items
    | DictValue entries ->
        let sortedEntries = entries |> List.sortWith (fun (k1, _) (k2, _) -> compareValues k1 k2)
        let items = sortedEntries |> List.map (fun (k, v) -> sprintf "%s: %s" (valueToString k) (valueToString v)) |> String.concat ", "
        sprintf "#{%s}" items
    | FunctionValue _ -> "|closure|"
    | CompositionValue _ -> "|closure|"

// Evaluate a literal expression to its runtime value
let evaluateLiteral (expr: Expression) : Value =
    match expr with
    | Integer valueStr ->
        // Remove underscores from numeric literals
        let cleanValue = valueStr.Replace("_", "")
        IntValue (int64 cleanValue)
    | Decimal valueStr ->
        // Remove underscores from numeric literals
        let cleanValue = valueStr.Replace("_", "")
        DecimalValue (float cleanValue)
    | String s -> StringValue s
    | Boolean b -> BoolValue b
    | Nil -> NilValue
    | _ -> failwith "Not a literal expression"

// Check if value is truthy
let isTruthy (value: Value) : bool =
    match value with
    | BoolValue false | NilValue | IntValue 0L | DecimalValue 0.0 | StringValue "" -> false
    | ListValue [] | SetValue [] | DictValue [] -> false
    | _ -> true

// Main evaluation function
let rec evaluateExpression (expr: Expression) (env: Environment) : Value =
    match expr with
    | Integer _ | Decimal _ | String _ | Boolean _ | Nil ->
        evaluateLiteral expr
    | Identifier (name, _) ->
        // Check for built-in operators/functions first
        match name with
        | "+" | "-" | "*" | "/" | "first" | "rest" | "size" | "push" | "assoc" | "map" | "filter" | "fold" ->
            // Create a built-in function value - we'll handle the actual logic in function calls
            let dummyBody = { statements = [Expression Nil] ; blockType = "Block" }
            FunctionValue ([], dummyBody, Dictionary<string, VariableInfo>())
        | _ ->
            match env.TryGetValue(name) with
            | true, varInfo -> !varInfo.valueRef
            | false, _ -> raise (RuntimeError $"Identifier can not be found: {name}")
    | Let (Identifier (name, _), valueExpr) ->
        let value = evaluateExpression valueExpr env
        env.[name] <- { valueRef = ref value; isMutable = false }
        NilValue
    | MutableLet (Identifier (name, _), valueExpr) ->
        let value = evaluateExpression valueExpr env
        env.[name] <- { valueRef = ref value; isMutable = true }
        NilValue
    | Assignment (Identifier (name, _), valueExpr) ->
        match env.TryGetValue(name) with
        | true, varInfo when varInfo.isMutable ->
            let value = evaluateExpression valueExpr env
            varInfo.valueRef := value
            value
        | true, _ -> raise (RuntimeError $"Variable '{name}' is not mutable")
        | false, _ -> raise (RuntimeError $"Identifier can not be found: {name}")
    | Infix (left, op, right) ->
        evaluateInfixOperation left op right env
    | Unary (op, operand) ->
        evaluateUnaryOperation op operand env
    | List elements ->
        let values = elements |> List.map (fun elem -> evaluateExpression elem env)
        ListValue values
    | Set elements ->
        let values = elements |> List.map (fun elem -> evaluateExpression elem env)
        // Check for dictionaries in set (forbidden)
        for value in values do
            match value with
            | DictValue _ -> raise (RuntimeError "Unable to include a Dictionary within a Set")
            | _ -> ()
        // Remove duplicates and sort for deterministic output
        let uniqueValues = values |> List.distinct |> List.sortWith compareValues
        SetValue uniqueValues
    | Dictionary entries ->
        let evaluatedEntries = entries |> List.map (fun (k, v) ->
            let keyVal = evaluateExpression k env
            let valueVal = evaluateExpression v env
            // Check if key is a dictionary (forbidden)
            match keyVal with
            | DictValue _ -> raise (RuntimeError "Unable to use a Dictionary as a Dictionary key")
            | _ -> ()
            (keyVal, valueVal))
        DictValue evaluatedEntries
    | Index (target, index) ->
        evaluateIndexOperation target index env
    | Call (Identifier ("puts", _), args) ->
        evaluatePutsFunction args env
    | Call (funcExpr, args) ->
        evaluateFunctionCall funcExpr args env
    | Function (parameters, body) ->
        FunctionValue (parameters, body, env)
    | If (condition, thenBranch, elseBranch) ->
        let conditionValue = evaluateExpression condition env
        if isTruthy conditionValue then
            evaluateBlock thenBranch env
        else
            match elseBranch with
            | Some elseBlock -> evaluateBlock elseBlock env
            | None -> NilValue
    | FunctionThread (initial, functions) ->
        // Function threading: initial |> func1 |> func2 |> ...
        let initialValue = evaluateExpression initial env
        let mutable currentValue = initialValue
        for func in functions do
            match func with
            | Call (funcExpr, funcArgs) ->
                // This is a partial application like map(inc)
                let newArgs = funcArgs @ [valueToExpression currentValue]
                currentValue <- evaluateExpression (Call (funcExpr, newArgs)) env
            | Identifier (name, _) ->
                // This is a function name
                currentValue <- evaluateExpression (Call (Identifier (name, ""), [valueToExpression currentValue])) env
            | Function (funcParams, body) ->
                // This is an inline function literal
                currentValue <- callUserFunction funcParams body env [valueToExpression currentValue] env
            | _ ->
                // Try to evaluate it as a function and call it
                let funcValue = evaluateExpression func env
                match funcValue with
                | FunctionValue (funcParams, body, funcClosure) ->
                    currentValue <- callUserFunction funcParams body funcClosure [valueToExpression currentValue] env
                | CompositionValue (subFunctions, subClosure) ->
                    currentValue <- callComposition subFunctions subClosure [valueToExpression currentValue] env
                | _ -> raise (RuntimeError "Invalid function in threading")
        currentValue
    | FunctionComposition functions ->
        // Create a composition value that stores the functions and closure
        CompositionValue (functions, env)
    | _ -> raise (RuntimeError "Unsupported expression type")

// Evaluate a block of statements
and evaluateBlock (block: Block) (env: Environment) : Value =
    let mutable result = NilValue
    for stmt in block.statements do
        match stmt with
        | Expression expr -> result <- evaluateExpression expr env
        | Comment _ -> result <- NilValue
    result

and evaluateInfixOperation (left: Expression) (op: string) (right: Expression) (env: Environment) : Value =
    let leftVal = evaluateExpression left env
    let rightVal = evaluateExpression right env

    match op, leftVal, rightVal with
    // Arithmetic operations
    | "+", IntValue a, IntValue b -> IntValue (a + b)
    | "+", DecimalValue a, DecimalValue b -> DecimalValue (a + b)
    | "+", IntValue a, DecimalValue b -> DecimalValue (float a + b)
    | "+", DecimalValue a, IntValue b -> DecimalValue (a + float b)
    | "+", StringValue a, StringValue b -> StringValue (a + b)
    | "+", StringValue a, IntValue b -> StringValue (a + string b)
    | "+", StringValue a, DecimalValue b -> StringValue (a + string b)
    | "+", IntValue a, StringValue b -> StringValue (string a + b)
    | "+", DecimalValue a, StringValue b -> StringValue (string a + b)

    | "-", IntValue a, IntValue b -> IntValue (a - b)
    | "-", DecimalValue a, DecimalValue b -> DecimalValue (a - b)
    | "-", IntValue a, DecimalValue b -> DecimalValue (float a - b)
    | "-", DecimalValue a, IntValue b -> DecimalValue (a - float b)

    | "*", IntValue a, IntValue b -> IntValue (a * b)
    | "*", DecimalValue a, DecimalValue b -> DecimalValue (a * b)
    | "*", IntValue a, DecimalValue b -> DecimalValue (float a * b)
    | "*", DecimalValue a, IntValue b -> DecimalValue (a * float b)
    | "*", StringValue s, IntValue n when n >= 0L -> StringValue (String.replicate (int n) s)
    | "*", StringValue s, IntValue n -> raise (RuntimeError $"Unsupported operation: String * Integer (< 0)")
    | "*", StringValue _, DecimalValue _ -> raise (RuntimeError "Unsupported operation: String * Decimal")

    | "/", IntValue a, IntValue b when b = 0L -> raise (RuntimeError "Division by zero")
    | "/", IntValue a, IntValue b -> IntValue (a / b)  // Integer division truncates
    | "/", DecimalValue a, DecimalValue b when b = 0.0 -> raise (RuntimeError "Division by zero")
    | "/", DecimalValue a, DecimalValue b -> DecimalValue (a / b)
    | "/", IntValue a, DecimalValue b when b = 0.0 -> raise (RuntimeError "Division by zero")
    | "/", IntValue a, DecimalValue b -> DecimalValue (float a / b)
    | "/", DecimalValue a, IntValue b when b = 0L -> raise (RuntimeError "Division by zero")
    | "/", DecimalValue a, IntValue b -> DecimalValue (a / float b)

    // Logical operations
    | "&&", _, _ -> BoolValue (isTruthy leftVal && isTruthy rightVal)
    | "||", _, _ -> BoolValue (isTruthy leftVal || isTruthy rightVal)

    // Comparison operations
    | "==", _, _ -> BoolValue (structurallyEqual leftVal rightVal)
    | "!=", _, _ -> BoolValue (not (structurallyEqual leftVal rightVal))
    | ">", IntValue a, IntValue b -> BoolValue (a > b)
    | ">", DecimalValue a, DecimalValue b -> BoolValue (a > b)
    | ">", IntValue a, DecimalValue b -> BoolValue (float a > b)
    | ">", DecimalValue a, IntValue b -> BoolValue (a > float b)
    | "<", IntValue a, IntValue b -> BoolValue (a < b)
    | "<", DecimalValue a, DecimalValue b -> BoolValue (a < b)
    | "<", IntValue a, DecimalValue b -> BoolValue (float a < b)
    | "<", DecimalValue a, IntValue b -> BoolValue (a < float b)
    | ">=", IntValue a, IntValue b -> BoolValue (a >= b)
    | ">=", DecimalValue a, DecimalValue b -> BoolValue (a >= b)
    | ">=", IntValue a, DecimalValue b -> BoolValue (float a >= b)
    | ">=", DecimalValue a, IntValue b -> BoolValue (a >= float b)
    | "<=", IntValue a, IntValue b -> BoolValue (a <= b)
    | "<=", DecimalValue a, DecimalValue b -> BoolValue (a <= b)
    | "<=", IntValue a, DecimalValue b -> BoolValue (float a <= b)
    | "<=", DecimalValue a, IntValue b -> BoolValue (a <= float b)

    // Collection operations
    | "+", ListValue a, ListValue b -> ListValue (a @ b)
    | "+", SetValue a, SetValue b ->
        let combined = a @ b |> List.distinct |> List.sortWith compareValues
        SetValue combined
    | "+", DictValue a, DictValue b ->
        let rec merge entries1 entries2 =
            match entries2 with
            | [] -> entries1
            | (k2, v2) :: rest ->
                let updatedEntries = updateOrAdd entries1 k2 v2
                merge updatedEntries rest
        and updateOrAdd entries key value =
            match entries with
            | [] -> [(key, value)]
            | (k, v) :: rest ->
                if structurallyEqual k key then
                    (key, value) :: rest  // Right-biased: replace with new value
                else
                    (k, v) :: updateOrAdd rest key value
        DictValue (merge a b)

    // Forbidden operations
    | "+", ListValue _, IntValue _ -> raise (RuntimeError "Unsupported operation: List + Integer")
    | "+", SetValue _, IntValue _ -> raise (RuntimeError "Unsupported operation: Set + Integer")

    | _, _, _ ->
        let leftType = match leftVal with | IntValue _ -> "Integer" | DecimalValue _ -> "Decimal" | StringValue _ -> "String" | BoolValue _ -> "Boolean" | NilValue -> "Nil" | ListValue _ -> "List" | SetValue _ -> "Set" | DictValue _ -> "Dictionary" | _ -> "Unknown"
        let rightType = match rightVal with | IntValue _ -> "Integer" | DecimalValue _ -> "Decimal" | StringValue _ -> "String" | BoolValue _ -> "Boolean" | NilValue -> "Nil" | ListValue _ -> "List" | SetValue _ -> "Set" | DictValue _ -> "Dictionary" | _ -> "Unknown"
        raise (RuntimeError $"Unsupported operation: {leftType} {op} {rightType}")

and evaluateUnaryOperation (op: string) (operand: Expression) (env: Environment) : Value =
    let operandVal = evaluateExpression operand env

    match op, operandVal with
    | "-", IntValue i -> IntValue (-i)
    | "-", DecimalValue f -> DecimalValue (-f)
    | "+", IntValue i -> IntValue i
    | "+", DecimalValue f -> DecimalValue f
    | _, _ ->
        let operandType = match operandVal with | IntValue _ -> "Integer" | DecimalValue _ -> "Decimal" | StringValue _ -> "String" | BoolValue _ -> "Boolean" | NilValue -> "Nil" | _ -> "Unknown"
        raise (RuntimeError $"Unsupported unary operation: {op} {operandType}")

and evaluateIndexOperation (target: Expression) (index: Expression) (env: Environment) : Value =
    let targetVal = evaluateExpression target env
    let indexVal = evaluateExpression index env

    match targetVal, indexVal with
    | StringValue s, IntValue i ->
        let idx = int i
        let len = s.Length
        if idx >= 0 && idx < len then
            StringValue (string s.[idx])
        elif idx < 0 && (-idx) <= len then
            StringValue (string s.[len + idx])
        else
            NilValue
    | ListValue items, IntValue i ->
        let idx = int i
        let len = items.Length
        if idx >= 0 && idx < len then
            items.[idx]
        elif idx < 0 && (-idx) <= len then
            items.[len + idx]
        else
            NilValue
    | DictValue entries, key ->
        let rec findValue entries =
            match entries with
            | [] -> NilValue
            | (k, v) :: rest ->
                if structurallyEqual k key then v
                else findValue rest
        findValue entries
    | StringValue _, indexType ->
        let typeName = match indexType with | IntValue _ -> "Integer" | DecimalValue _ -> "Decimal" | StringValue _ -> "String" | BoolValue _ -> "Boolean" | NilValue -> "Nil" | _ -> "Unknown"
        raise (RuntimeError $"Unable to perform index operation, found: String[{typeName}]")
    | ListValue _, indexType ->
        let typeName = match indexType with | IntValue _ -> "Integer" | DecimalValue _ -> "Decimal" | StringValue _ -> "String" | BoolValue _ -> "Boolean" | NilValue -> "Nil" | _ -> "Unknown"
        raise (RuntimeError $"Unable to perform index operation, found: List[{typeName}]")
    | _, _ ->
        raise (RuntimeError "Index operation not supported on this type")

// Helper function for structural equality comparison
and structurallyEqual (a: Value) (b: Value) : bool =
    match a, b with
    | IntValue x, IntValue y -> x = y
    | DecimalValue x, DecimalValue y -> x = y
    | StringValue x, StringValue y -> x = y
    | BoolValue x, BoolValue y -> x = y
    | NilValue, NilValue -> true
    | ListValue xs, ListValue ys ->
        xs.Length = ys.Length && List.forall2 structurallyEqual xs ys
    | SetValue xs, SetValue ys ->
        let sortedXs = xs |> List.sortWith compareValues
        let sortedYs = ys |> List.sortWith compareValues
        sortedXs.Length = sortedYs.Length && List.forall2 structurallyEqual sortedXs sortedYs
    | DictValue xs, DictValue ys ->
        let sortedXs = xs |> List.sortWith (fun (k1, _) (k2, _) -> compareValues k1 k2)
        let sortedYs = ys |> List.sortWith (fun (k1, _) (k2, _) -> compareValues k1 k2)
        sortedXs.Length = sortedYs.Length &&
        List.forall2 (fun (k1, v1) (k2, v2) -> structurallyEqual k1 k2 && structurallyEqual v1 v2) sortedXs sortedYs
    | _ -> false

and evaluatePutsFunction (args: Expression list) (env: Environment) : Value =
    let values = args |> List.map (fun arg -> evaluateExpression arg env)
    let output = values |> List.map valueToString |> String.concat " "
    outputBuffer.AppendLine(output + " ") |> ignore
    NilValue

and evaluateFunctionCall (funcExpr: Expression) (args: Expression list) (env: Environment) : Value =
    match funcExpr with
    | Identifier ("+", _) ->
        match args with
        | [a; b] -> evaluateInfixOperation a "+" b env
        | [a] ->
            // Partial application - create a function that captures 'a'
            let aVal = evaluateExpression a env
            let partialEnv = Dictionary<string, VariableInfo>(env)
            partialEnv.["captured_a"] <- { valueRef = ref aVal; isMutable = false }
            let partialBody = { statements = [Expression (Call (Identifier ("+", ""), [Identifier ("captured_a", ""); Identifier ("x", "")]))] ; blockType = "Block" }
            FunctionValue ([Identifier ("x", "")], partialBody, partialEnv)
        | _ -> raise (RuntimeError "Function '+' expects 1 or 2 arguments")
    | Identifier ("-", _) ->
        match args with
        | [a; b] -> evaluateInfixOperation a "-" b env
        | _ -> raise (RuntimeError "Function '-' expects exactly 2 arguments")
    | Identifier ("*", _) ->
        match args with
        | [a; b] -> evaluateInfixOperation a "*" b env
        | [a] ->
            // Partial application - create a function that captures 'a'
            let aVal = evaluateExpression a env
            let partialEnv = Dictionary<string, VariableInfo>(env)
            partialEnv.["captured_a"] <- { valueRef = ref aVal; isMutable = false }
            let partialBody = { statements = [Expression (Call (Identifier ("*", ""), [Identifier ("captured_a", ""); Identifier ("x", "")]))] ; blockType = "Block" }
            FunctionValue ([Identifier ("x", "")], partialBody, partialEnv)
        | _ -> raise (RuntimeError "Function '*' expects 1 or 2 arguments")
    | Identifier ("/", _) ->
        match args with
        | [a; b] -> evaluateInfixOperation a "/" b env
        | _ -> raise (RuntimeError "Function '/' expects exactly 2 arguments")
    | Identifier ("first", _) ->
        match args with
        | [expr] ->
            let value = evaluateExpression expr env
            match value with
            | ListValue [] | SetValue [] | StringValue "" -> NilValue
            | ListValue (head :: _) -> head
            | SetValue (head :: _) -> head  // Sets are already sorted
            | StringValue s -> StringValue (string s.[0])
            | _ -> NilValue
        | _ -> raise (RuntimeError "Function 'first' expects exactly 1 argument")
    | Identifier ("rest", _) ->
        match args with
        | [expr] ->
            let value = evaluateExpression expr env
            match value with
            | ListValue [] -> ListValue []  // Empty list returns empty list, not nil
            | SetValue [] -> SetValue []   // Empty set returns empty set, not nil
            | StringValue "" -> StringValue ""  // Empty string returns empty string, not nil
            | ListValue (_ :: tail) -> ListValue tail
            | SetValue (_ :: tail) -> SetValue tail
            | StringValue s when s.Length > 1 -> StringValue (s.Substring(1))
            | StringValue _ -> StringValue ""
            | _ -> NilValue
        | _ -> raise (RuntimeError "Function 'rest' expects exactly 1 argument")
    | Identifier ("size", _) ->
        match args with
        | [expr] ->
            let value = evaluateExpression expr env
            match value with
            | ListValue items -> IntValue (int64 items.Length)
            | SetValue items -> IntValue (int64 items.Length)
            | DictValue entries -> IntValue (int64 entries.Length)
            | StringValue s ->
                // Count UTF-8 bytes
                let utf8Bytes = System.Text.Encoding.UTF8.GetByteCount(s)
                IntValue (int64 utf8Bytes)
            | _ -> NilValue
        | _ -> raise (RuntimeError "Function 'size' expects exactly 1 argument")
    | Identifier ("push", _) ->
        match args with
        | [item; expr] ->  // Arguments swapped: push(item, collection)
            let itemVal = evaluateExpression item env
            let collectionVal = evaluateExpression expr env
            match collectionVal with
            | ListValue items -> ListValue (items @ [itemVal])
            | SetValue items ->
                if List.exists (structurallyEqual itemVal) items then
                    SetValue items  // No duplicates
                else
                    let newItems = (items @ [itemVal]) |> List.sortWith compareValues
                    SetValue newItems
            | _ -> raise (RuntimeError "Function 'push' expects a list or set as second argument")
        | [item] ->
            // Partial application - create a function that captures the item
            let itemVal = evaluateExpression item env
            let partialEnv = Dictionary<string, VariableInfo>(env)
            partialEnv.["captured_item"] <- { valueRef = ref itemVal; isMutable = false }
            let partialBody = { statements = [Expression (Call (Identifier ("push", ""), [Identifier ("captured_item", ""); Identifier ("collection", "")]))] ; blockType = "Block" }
            FunctionValue ([Identifier ("collection", "")], partialBody, partialEnv)
        | _ -> raise (RuntimeError "Function 'push' expects 1 or 2 arguments")
    | Identifier ("assoc", _) ->
        match args with
        | [key; value; expr] ->  // Arguments reordered: assoc(key, value, dict)
            let keyVal = evaluateExpression key env
            let valueVal = evaluateExpression value env
            let dictVal = evaluateExpression expr env
            match dictVal with
            | DictValue entries ->
                // Check if key is a dictionary (forbidden)
                match keyVal with
                | DictValue _ -> raise (RuntimeError "Unable to use a Dictionary as a Dictionary key")
                | _ -> ()
                let rec updateOrAdd entries =
                    match entries with
                    | [] -> [(keyVal, valueVal)]
                    | (k, v) :: rest ->
                        if structurallyEqual k keyVal then
                            (keyVal, valueVal) :: rest  // Replace existing
                        else
                            (k, v) :: updateOrAdd rest
                DictValue (updateOrAdd entries)
            | _ -> raise (RuntimeError "Function 'assoc' expects a dictionary as third argument")
        | _ -> raise (RuntimeError "Function 'assoc' expects exactly 3 arguments")
    | Identifier ("map", _) ->
        evaluateMapFunction args env
    | Identifier ("filter", _) ->
        evaluateFilterFunction args env
    | Identifier ("fold", _) ->
        evaluateFoldFunction args env
    | Identifier (name, _) ->
        // Check if it's a variable first
        match env.TryGetValue(name) with
        | true, varInfo ->
            match !varInfo.valueRef with
            | FunctionValue (parameters, body, closure) ->
                callUserFunction parameters body closure args env
            | CompositionValue (functions, closure) ->
                callComposition functions closure args env
            | _ -> raise (RuntimeError $"Value is not callable: {name}")
        | false, _ -> raise (RuntimeError $"Identifier can not be found: {name}")
    | _ ->
        // Try to evaluate the function expression and check if it's callable
        let funcValue = evaluateExpression funcExpr env
        match funcValue with
        | FunctionValue (parameters, body, closure) ->
            callUserFunction parameters body closure args env
        | CompositionValue (functions, closure) ->
            callComposition functions closure args env
        | IntValue _ -> raise (RuntimeError "Expected a Function, found: Integer")
        | DecimalValue _ -> raise (RuntimeError "Expected a Function, found: Decimal")
        | StringValue _ -> raise (RuntimeError "Expected a Function, found: String")
        | BoolValue _ -> raise (RuntimeError "Expected a Function, found: Boolean")
        | NilValue -> raise (RuntimeError "Expected a Function, found: Nil")
        | ListValue _ -> raise (RuntimeError "Expected a Function, found: List")
        | SetValue _ -> raise (RuntimeError "Expected a Function, found: Set")
        | DictValue _ -> raise (RuntimeError "Expected a Function, found: Dictionary")

// Map function implementation
and evaluateMapFunction (args: Expression list) (env: Environment) : Value =
    match args with
    | [funcExpr; listExpr] ->
        let funcValue = evaluateExpression funcExpr env
        let listValue = evaluateExpression listExpr env

        match funcValue, listValue with
        | FunctionValue (parameters, body, closure), ListValue items ->
            let mappedItems = items |> List.map (fun item ->
                callUserFunction parameters body closure [valueToExpression item] env)
            ListValue mappedItems
        | CompositionValue (functions, closure), ListValue items ->
            let mappedItems = items |> List.map (fun item ->
                callComposition functions closure [valueToExpression item] env)
            ListValue mappedItems
        | _, ListValue _ -> raise (RuntimeError $"Unexpected argument: map({getTypeName funcValue}, List)")
        | FunctionValue _, _ -> raise (RuntimeError $"Unexpected argument: map(Function, {getTypeName listValue})")
        | CompositionValue _, _ -> raise (RuntimeError $"Unexpected argument: map(Function, {getTypeName listValue})")
        | _, _ -> raise (RuntimeError $"Unexpected argument: map({getTypeName funcValue}, {getTypeName listValue})")
    | [funcExpr] ->
        // Partial application
        let funcValue = evaluateExpression funcExpr env
        match funcValue with
        | FunctionValue _ ->
            let partialEnv = Dictionary<string, VariableInfo>(env)
            partialEnv.["captured_func"] <- { valueRef = ref funcValue; isMutable = false }
            let partialBody = { statements = [Expression (Call (Identifier ("map", ""), [Identifier ("captured_func", ""); Identifier ("list", "")]))] ; blockType = "Block" }
            FunctionValue ([Identifier ("list", "")], partialBody, partialEnv)
        | _ -> raise (RuntimeError $"Unexpected argument: map({getTypeName funcValue}, List)")
    | _ -> raise (RuntimeError "Function 'map' expects 1 or 2 arguments")

// Filter function implementation
and evaluateFilterFunction (args: Expression list) (env: Environment) : Value =
    match args with
    | [funcExpr; listExpr] ->
        let funcValue = evaluateExpression funcExpr env
        let listValue = evaluateExpression listExpr env

        match funcValue, listValue with
        | FunctionValue (parameters, body, closure), ListValue items ->
            let filteredItems = items |> List.filter (fun item ->
                let result = callUserFunction parameters body closure [valueToExpression item] env
                isTruthy result)
            ListValue filteredItems
        | CompositionValue (functions, closure), ListValue items ->
            let filteredItems = items |> List.filter (fun item ->
                let result = callComposition functions closure [valueToExpression item] env
                isTruthy result)
            ListValue filteredItems
        | _, ListValue _ -> raise (RuntimeError $"Unexpected argument: filter({getTypeName funcValue}, List)")
        | FunctionValue _, _ -> raise (RuntimeError $"Unexpected argument: filter(Function, {getTypeName listValue})")
        | CompositionValue _, _ -> raise (RuntimeError $"Unexpected argument: filter(Function, {getTypeName listValue})")
        | _, _ -> raise (RuntimeError $"Unexpected argument: filter({getTypeName funcValue}, {getTypeName listValue})")
    | [funcExpr] ->
        // Partial application
        let funcValue = evaluateExpression funcExpr env
        match funcValue with
        | FunctionValue _ ->
            let partialEnv = Dictionary<string, VariableInfo>(env)
            partialEnv.["captured_func"] <- { valueRef = ref funcValue; isMutable = false }
            let partialBody = { statements = [Expression (Call (Identifier ("filter", ""), [Identifier ("captured_func", ""); Identifier ("list", "")]))] ; blockType = "Block" }
            FunctionValue ([Identifier ("list", "")], partialBody, partialEnv)
        | _ -> raise (RuntimeError $"Unexpected argument: filter({getTypeName funcValue}, List)")
    | _ -> raise (RuntimeError "Function 'filter' expects 1 or 2 arguments")

// Fold function implementation
and evaluateFoldFunction (args: Expression list) (env: Environment) : Value =
    match args with
    | [initExpr; funcExpr; listExpr] ->
        let initValue = evaluateExpression initExpr env
        let funcValue = evaluateExpression funcExpr env
        let listValue = evaluateExpression listExpr env

        match funcValue, listValue with
        | FunctionValue (parameters, body, closure), ListValue items ->
            // Check if this is a built-in operator function
            match funcExpr with
            | Identifier ("+", _) ->
                List.fold (fun acc item ->
                    evaluateInfixOperation (valueToExpression acc) "+" (valueToExpression item) env
                ) initValue items
            | Identifier ("-", _) ->
                List.fold (fun acc item ->
                    evaluateInfixOperation (valueToExpression acc) "-" (valueToExpression item) env
                ) initValue items
            | Identifier ("*", _) ->
                List.fold (fun acc item ->
                    evaluateInfixOperation (valueToExpression acc) "*" (valueToExpression item) env
                ) initValue items
            | Identifier ("/", _) ->
                List.fold (fun acc item ->
                    evaluateInfixOperation (valueToExpression acc) "/" (valueToExpression item) env
                ) initValue items
            | _ ->
                // Regular user-defined function
                List.fold (fun acc item ->
                    callUserFunction parameters body closure [valueToExpression acc; valueToExpression item] env
                ) initValue items
        | _, ListValue _ -> raise (RuntimeError $"Unexpected argument: fold({getTypeName initValue}, {getTypeName funcValue}, List)")
        | FunctionValue _, _ -> raise (RuntimeError $"Unexpected argument: fold({getTypeName initValue}, Function, {getTypeName listValue})")
        | _, _ -> raise (RuntimeError $"Unexpected argument: fold({getTypeName initValue}, {getTypeName funcValue}, {getTypeName listValue})")
    | [initExpr; funcExpr] ->
        // Partial application
        let initValue = evaluateExpression initExpr env
        let funcValue = evaluateExpression funcExpr env
        match funcValue with
        | FunctionValue _ ->
            let partialEnv = Dictionary<string, VariableInfo>(env)
            partialEnv.["captured_init"] <- { valueRef = ref initValue; isMutable = false }
            partialEnv.["captured_func"] <- { valueRef = ref funcValue; isMutable = false }
            // Store the original function expression to preserve operator identity
            match funcExpr with
            | Identifier (op, _) when op = "+" || op = "-" || op = "*" || op = "/" ->
                partialEnv.["captured_op"] <- { valueRef = ref (StringValue op); isMutable = false }
            | _ -> ()
            let partialBody = { statements = [Expression (Call (Identifier ("fold", ""), [Identifier ("captured_init", ""); Identifier ("captured_func", ""); Identifier ("list", "")]))] ; blockType = "Block" }
            FunctionValue ([Identifier ("list", "")], partialBody, partialEnv)
        | _ -> raise (RuntimeError $"Unexpected argument: fold({getTypeName initValue}, {getTypeName funcValue}, List)")
    | [initExpr] ->
        // Partial application with just initial value
        let initValue = evaluateExpression initExpr env
        let partialEnv = Dictionary<string, VariableInfo>(env)
        partialEnv.["captured_init"] <- { valueRef = ref initValue; isMutable = false }
        let partialBody = { statements = [Expression (Call (Identifier ("fold", ""), [Identifier ("captured_init", ""); Identifier ("func", ""); Identifier ("list", "")]))] ; blockType = "Block" }
        FunctionValue ([Identifier ("func", ""); Identifier ("list", "")], partialBody, partialEnv)
    | _ -> raise (RuntimeError "Function 'fold' expects 1, 2, or 3 arguments")

// Helper function to get type name for error messages
and getTypeName (value: Value) : string =
    match value with
    | IntValue _ -> "Integer"
    | DecimalValue _ -> "Decimal"
    | StringValue _ -> "String"
    | BoolValue _ -> "Boolean"
    | NilValue -> "Nil"
    | ListValue _ -> "List"
    | SetValue _ -> "Set"
    | DictValue _ -> "Dictionary"
    | FunctionValue _ -> "Function"
    | CompositionValue _ -> "Function"

// Helper function to convert Value back to Expression
and valueToExpression (value: Value) : Expression =
    match value with
    | IntValue i -> Integer (string i)
    | DecimalValue f -> Decimal (string f)
    | StringValue s -> String s
    | BoolValue true -> Boolean true
    | BoolValue false -> Boolean false
    | NilValue -> Nil
    | ListValue items ->
        let itemExprs = items |> List.map valueToExpression
        Parser.List itemExprs
    | SetValue items ->
        let itemExprs = items |> List.map valueToExpression
        Parser.Set itemExprs
    | DictValue entries ->
        let entryExprs = entries |> List.map (fun (k, v) -> (valueToExpression k, valueToExpression v))
        Parser.Dictionary entryExprs
    | FunctionValue _ -> raise (RuntimeError "Cannot convert function to expression")
    | CompositionValue _ -> raise (RuntimeError "Cannot convert composition to expression")

// Call a function composition
and callComposition (functions: Expression list) (closure: Environment) (args: Expression list) (callEnv: Environment) : Value =
    if args.Length <> 1 then
        raise (RuntimeError "Function composition expects exactly 1 argument")

    let initialValue = evaluateExpression args.Head callEnv

    // Apply each function in the composition sequentially
    let mutable currentValue = initialValue
    for func in functions do
        match func with
        | Call (funcExpr, funcArgs) ->
            // This is a partial application like +(1) or *(2)
            let newArgs = funcArgs @ [valueToExpression currentValue]
            currentValue <- evaluateExpression (Call (funcExpr, newArgs)) closure
        | Identifier (name, _) ->
            // This is a function name like "inc"
            match closure.TryGetValue(name) with
            | true, varInfo ->
                match !varInfo.valueRef with
                | FunctionValue (funcParams, body, funcClosure) ->
                    currentValue <- callUserFunction funcParams body funcClosure [valueToExpression currentValue] callEnv
                | CompositionValue (subFunctions, subClosure) ->
                    currentValue <- callComposition subFunctions subClosure [valueToExpression currentValue] callEnv
                | _ -> raise (RuntimeError $"Value is not callable: {name}")
            | false, _ ->
                // Try as built-in function
                currentValue <- evaluateExpression (Call (Identifier (name, ""), [valueToExpression currentValue])) closure
        | Function (funcParams, body) ->
            // This is an inline function literal like |a| a + 1
            currentValue <- callUserFunction funcParams body closure [valueToExpression currentValue] callEnv
        | _ ->
            // Try to evaluate it as a function and call it
            let funcValue = evaluateExpression func closure
            match funcValue with
            | FunctionValue (funcParams, body, funcClosure) ->
                currentValue <- callUserFunction funcParams body funcClosure [valueToExpression currentValue] callEnv
            | CompositionValue (subFunctions, subClosure) ->
                currentValue <- callComposition subFunctions subClosure [valueToExpression currentValue] callEnv
            | _ -> raise (RuntimeError "Invalid function in composition")

    currentValue

// Call a user-defined function
and callUserFunction (parameters: Expression list) (body: Block) (closure: Environment) (args: Expression list) (callEnv: Environment) : Value =
    let evaluatedArgs = args |> List.map (fun arg -> evaluateExpression arg callEnv)
    let paramNames = parameters |> List.map (fun param ->
        match param with
        | Identifier (name, _) -> name
        | _ -> raise (RuntimeError "Invalid parameter in function definition"))

    // Handle partial application and over-application
    if evaluatedArgs.Length < paramNames.Length then
        // Partial application - return new function with some parameters bound
        let remainingParams = paramNames |> List.skip evaluatedArgs.Length
        let boundEnv = Dictionary<string, VariableInfo>(closure)
        List.zip (paramNames |> List.take evaluatedArgs.Length) evaluatedArgs
        |> List.iter (fun (name, value) ->
            boundEnv.[name] <- { valueRef = ref value; isMutable = false })

        let remainingParamExprs = remainingParams |> List.map (fun name -> Identifier (name, ""))
        FunctionValue (remainingParamExprs, body, boundEnv)
    else
        // Exact match or over-application - call function
        let functionEnv = Dictionary<string, VariableInfo>(closure)
        let argsToUse = if evaluatedArgs.Length > paramNames.Length then
                            evaluatedArgs |> List.take paramNames.Length
                        else evaluatedArgs
        List.zip paramNames argsToUse
        |> List.iter (fun (name, value) ->
            functionEnv.[name] <- { valueRef = ref value; isMutable = false })

        // Special handling for fold partial application with operators
        if body.statements.Length = 1 then
            match body.statements.[0] with
            | Expression (Call (Identifier ("fold", _), [Identifier ("captured_init", _); Identifier ("captured_func", _); Identifier ("list", _)]))
                when functionEnv.ContainsKey("captured_op") ->
                let initVal = !functionEnv.["captured_init"].valueRef
                let listVal = !functionEnv.["list"].valueRef
                let opVal = !functionEnv.["captured_op"].valueRef
                match opVal, listVal with
                | StringValue op, ListValue items ->
                    List.fold (fun acc item ->
                        evaluateInfixOperation (valueToExpression acc) op (valueToExpression item) functionEnv
                    ) initVal items
                | _ ->
                    // Fallback to regular evaluation
                    let mutable result = NilValue
                    for stmt in body.statements do
                        match stmt with
                        | Expression expr -> result <- evaluateExpression expr functionEnv
                        | Comment _ -> result <- NilValue
                    result
            | _ ->
                // Regular function body evaluation
                let mutable result = NilValue
                for stmt in body.statements do
                    match stmt with
                    | Expression expr -> result <- evaluateExpression expr functionEnv
                    | Comment _ -> result <- NilValue
                result
        else
            // Regular function body evaluation
            let mutable result = NilValue
            for stmt in body.statements do
                match stmt with
                | Expression expr -> result <- evaluateExpression expr functionEnv
                | Comment _ -> result <- NilValue
            result

// Evaluate a statement
let evaluateStatement (stmt: Statement) (env: Environment) : Value =
    match stmt with
    | Expression expr -> evaluateExpression expr env
    | Comment _ -> NilValue

// Main program evaluation
let evaluate (program: Program) : string =
    outputBuffer.Clear() |> ignore
    let globalEnv = Dictionary<string, VariableInfo>()

    try
        let mutable lastExpressionValue = NilValue
        let mutable lastExpression = None

        for stmt in program.statements do
            let result = evaluateStatement stmt globalEnv
            match stmt with
            | Expression _ ->
                lastExpressionValue <- result
                lastExpression <- Some stmt
            | Comment _ -> ()

        match lastExpression with
        | Some (Expression (Call (Identifier ("puts", _), _))) ->
            // End with nil after puts
            outputBuffer.AppendLine("nil") |> ignore
        | Some (Expression _) when lastExpressionValue <> NilValue ->
            // Print the last non-nil expression value
            outputBuffer.AppendLine((valueToString lastExpressionValue) + " ") |> ignore
        | _ ->
            // End with nil for other cases
            outputBuffer.AppendLine("nil") |> ignore

        outputBuffer.ToString()
    with
    | RuntimeError msg -> $"{outputBuffer.ToString()}[Error] {msg}\n"