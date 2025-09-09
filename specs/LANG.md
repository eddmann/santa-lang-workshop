# `elf-lang` Language Specification

`elf-lang` is a subset of `santa-lang` used to explore the power of LLM Agents.

## Lexical elements

- **Literals**: integers, decimals, strings, booleans, nil
  - Integers: `42`, `1_000_000`
    - Underscores are allowed only between digits; not at start or end
    - Leading zeros are allowed only for zero itself (`0`), not other integers
  - Decimals: `3.14`, `1_000.50`, `0.001_234`
    - Underscores are allowed only between digits; not adjacent to the decimal point
  - Strings: `"hello"`, supports escapes: `\n`, `\t`, `\"`, `\\`
    - Strings use double quotes; single-quoted strings are not supported
  - Booleans: `true`, `false`
  - Nil: `nil`
- **Identifiers**: `x`, `y_123`
- **Keywords**: `let`, `mut`, `if`, `else`
- **Statement terminator**: `;` (optional)
- **Blocks**: `{ ... }` contain statements; last expression is the block value
  - Disambiguation: `{}` is a Set literal in expression position; braces after `if`/`else` or a function parameter list denote a block. Dictionaries always use `#{ ... }`.
- **Comments**: line comments start with `//` and run to end of line. Block comments are not supported.

## Types

- Primitive types: Integer, Decimal, String, Boolean, Nil
- Compound types: List, Set, Dictionary, Function

**Note:** Collection immutability: Lists, Sets, and Dictionaries are immutable. Operations that appear to modify a collection (such as `push`, `assoc`, and `+` on collections) return new collections; the original value is not changed.

Equality is structural for all values (including collections). Two collections are equal if their elements/entries are equal in structure. Any value can be used as a Set element. Dictionary keys may be any value except a Dictionary. Attempting to use a Dictionary as a Dictionary key is an error: `Unable to use a Dictionary as a Dictionary key`.

## Literals and data structures

- **Immutable List**: `[1, 2, 3]`, `[]`, mixed types allowed
- **Immutable Set**: `{1, 2, 3}`, `{}`. Sets are unordered and contain unique elements. Printed order is deterministically sorted in ascending order by value.
- **Immutable Dictionary**: `#{"a": 1, "b": 2}`, `#{}`, keys and values may be of various types (e.g., integer keys, list keys). Nested dictionaries and collections are supported.
  - Printed order is deterministically sorted in ascending order by key.
- **Function literal**: `|x, y| x + y` or with a block `|x| { ... }`. Zero-arg functions: `|| "hello"`.

## Expressions and operators

- Arithmetic: `+`, `-`, `*`, `/`
  - Examples: `1 + 2 -> 3`, `3 * 4 -> 12`, `10 / 2 -> 5`, `2.5 * 3 -> 7.5`, `10 / 2.5 -> 4`
  - Mixed integer/decimal arithmetic is supported; results may be decimal when decimals are involved.
  - Division `/`: Integer ÷ Integer uses truncating division toward zero (e.g., `3 / 2 -> 1`, `-3 / 2 -> -1`). If either operand is Decimal, the result is Decimal.
  - Division by zero is an error.
- Comparison: `==`, `!=`, `>`, `<`, `>=`, `<=`
- Logical: `&&`, `||` produce Boolean results using truthiness (see Truthiness). Short-circuiting applies.
  - These operators always return Booleans (never the last evaluated operand).
- String operations:
  - Concatenation: `"hello" + " " + "world" -> "hello world"`
  - Repetition: `"a" * 5 -> "aaaaa"` (multiplier must be a non-negative Integer)
    - `"a" * 0 -> ""`
    - Negative or non-integer multipliers are errors
- Collection operations with `+`:
  - List + List: concatenation `[1, 2] + [3, 4] -> [1, 2, 3, 4]`
  - Set + Set: union `{1, 2} + {3, 4} -> {1, 2, 3, 4}` (printed ascending by value)
  - Dictionary + Dictionary: right-biased merge `#{"a": 1} + #{"a": 2, "b": 3} -> #{"a": 2, "b": 3}` (keys in the right operand override the left)
- Operator functions:
  - The arithmetic operators `+`, `-`, `*`, `/` are also first-class functions.
  - They can be called in prefix form: `+(1, 2)`, `-(10, 3)`, `*(2, 4)`, `/(10, 2)`.
  - They can be passed as arguments to higher-order functions (e.g., `fold(0, +, list)`).
- Operator precedence:
  - Higher → lower: parentheses `()`, indexing `[]`, function call `()`, unary `-`; `*` `/`; `+` `-`; `>>`; `|>`; comparisons `==` `!=` `>` `<` `>=` `<=`; `&&`; `||`
  - Function calls and indexing bind tighter than `|>` and `>>`
  - All binary operators are left-associative except `>>`, which is right-associative
  - Example: `10 - 5 - 2 -> 3`

## Indexing

- **List indexing**: `list[index]`
  - Zero-based indexing; negative indices count from the end (`-1` is last)
  - Out-of-bounds returns `nil`
  - Non-integer indices error with message style like `Unable to perform index operation, found: List[String]`
- **String indexing**: `string[index]`
  - Same rules as lists; returns a one-character string
  - Non-integer indices error, e.g., `String[Decimal]`, `String[Boolean]`
  - Indexing uses the same unit as `size(string)` (implementation-defined)
- **Dictionary indexing**: `dict[key]`
  - Returns value if present, otherwise `nil`
  - Any non-Dictionary value may be used as a key; if the key is not present, returns `nil`

## Variables and assignment

- Declarations:
  - Immutable: `let x = 42;`
  - Mutable: `let mut y = 10;`
- Assignment: `y = 20;` (only allowed for variables declared with `mut`)
- Reassigning an immutable variable is an error: `Variable 'x' is not mutable`
- Variables are captured by closures by reference; outer variables can be mutated inside inner functions.
  - Scoping: lexical scoping; variables are block-scoped. Inner scopes may shadow outer variables. Captured variables persist as long as referenced by closures.

## Control flow

- `if` is an expression: `if cond { expr1 } else { expr2 }`
  - Returns the value of the taken branch
  - Used within larger expressions, e.g., `if ... { ... } else { ... } + 2`

## Truthiness

- Truthy: non-zero integers/decimals, non-empty strings, non-empty lists/sets/dictionaries, `true`
- Falsy: `0`, `0.0`, `""`, `[]`, `{}`, `#{}`, `false`, `nil`
- Logical operators return Booleans based on truthiness: e.g., `42 && "hello" -> true`, `0 || "default" -> true`

## Functions

- Definition: `let add = |x, y| x + y;`
- Calling: `add(1, 2)`; functions are first-class and can be nested.
- Partial application and arity handling:
  - Providing fewer arguments returns a function awaiting the remaining parameters (e.g., `add(1) -> |y| { [closure] }`)
  - Providing extra arguments is allowed; extras are ignored (e.g., `add(1, 2, 3) -> 3`)
  - Zero-arg functions ignore any provided arguments: `(|| "hello")(1, 2) -> "hello"`
- Closures capture surrounding variables; mutating captured variables is supported.

## Function composition and threading

- Thread (pipeline) operator: `|>`
  - Passes the left value as the last argument to the right function
  - Examples:
    - `list |> first` -> `first(list)`
    - `list |> push(4)` -> `push(4, list)`
    - `[1,2,3] |> map(|x| x + 1) |> filter(|x| x > 2)`
    - Value-threading with lambdas: `42 |> |x| x * 2 |> |x| x + 1`
- Function composition operator: `>>`
  - `f >> g` composes functions; applying the result to `x` behaves like `g(f(x))`
  - Works with built-ins: `map(add1) >> map(double)`
  - Threading with multi-arg functions: `x |> f(a, b)` is `f(a, b, x)`
  - `>>` composes functions right-associatively; compose unary functions (use partial application for multi-arg functions)
  - Precedence: function calls/indexing > `>>` > `|>`

## Built-in functions

- `puts(value)`: prints a canonical representation and returns `nil`
- List operations:
  - `first(list)`: first element or `nil` for empty
  - `rest(list)`: all but the first; `[]` for empty
  - `push(value, list)`: returns a new list with `value` appended
  - `size(list)`: length
  - `map(fn, list)`: transforms each element
  - `filter(fn, list)`: keeps elements where predicate is truthy
  - `fold(init, fn|op, list)`: reduces list; supports operator functions like `+`, `*`
- Set operations:
  - `push(value, set)`: returns a new set with `value` added (no duplicate)
  - `size(set)`: cardinality
- Dictionary operations:
  - `assoc(key, value, dict)`: returns a new dictionary with `key` associated to `value`
  - `size(dict)`: number of entries
- String operation:
  - `size(string)`: length; Unicode is supported but size reflects implementation-defined units (e.g., `"❤" |> size -> 3`)

## Collections edge cases

- Empty list operations: `size -> 0`, `first -> nil`, `rest -> []`, `push(1) -> [1]`, `map/filter -> []`, `fold(init, +) -> init`
- Empty set operations: `size -> 0`, `push(1) -> {1}`, `map/filter -> {}`
- Empty dict operations: `size -> 0`, `assoc("key","value") -> #{"key": "value"}`, missing key access -> `nil`
- Set elements and dictionaries:
  - A set literal cannot contain a dictionary: `Unable to include a Dictionary within a Set`
  - Pushing a dictionary into a set via `push` is allowed: `{1, "hello", [2, 3]} |> push(#{4: "four"}) -> {1, "hello", [2, 3], #{4: "four"}}`

## Errors

- Unknown identifier: `Identifier can not be found: <name>`
- Immutable reassignment: `Variable '<name>' is not mutable`
- Type mismatch operations: `Unsupported operation: <LeftType> <op> <RightType>`
- Invalid indexing type: `Unable to perform index operation, found: <Type>[<IndexType>]`
- Division by zero: `Division by zero`
- Invalid string repetition count: `Invalid string repetition count: <count>`
- Calling a non-function value: `Value is not callable: <Type>`
- Invalid built-in argument type: `<fn>(...): invalid argument type, expected <Expected>, found <Actual>`

## CLI / execution

- Programs are sequences of statements. `puts(...)` prints a value and returns `nil`.
- Multiple `puts` calls print multiple lines.

## Examples

```santa
let mut y = 10;
y = 20;
puts(y);               // 20

let factorial = |n| if n <= 1 { 1 } else { n * factorial(n - 1) };
puts(factorial(5));    // 120

let numbers = [1, 2, 3, 4, 5];
puts(numbers |> map(|x| x * 2) |> filter(|x| x > 5)); // [6, 8, 10]

let add = |x, y| x + y;
let double = |x| x * 2;
let add_then_double = add(1) >> double;
puts(add_then_double(5)); // 12
```
