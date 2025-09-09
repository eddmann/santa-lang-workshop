Stage 2 focuses on the parser and AST construction. It verifies:

- Literal expressions parsed into AST nodes (ints, decimals, strings, booleans, nil)
- Variable declarations: immutable `let` and mutable `let mut` with initializers
- Arithmetic precedence and grouping for + - \* /, including parentheses
- Collection literals: Lists `[]`, Sets `{}`, Dictionaries `#{}` (with mixed/nested values)
- Indexing: list[index] and dict[key]
- If-expressions: `if cond { ... } else { ... }` as expressions with `Block` bodies
- Function literals: `|x, y| expr` and block-bodied forms; zero or more params
- Known limitations in these tests intentionally surface parser errors for later stages (e.g., certain calls/composition)

Note: Semicolons are optional; both with and without `;` are accepted.
