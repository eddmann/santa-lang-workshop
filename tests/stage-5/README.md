Stage 5 adds higher-order functions, partial application, composition, threading, and closures. It verifies:

- Partial application for user functions and built-ins/operators; extra args ignored
- Higher-order list ops: `map`, `filter`, `fold` with function or operator args; empty collection behavior
- Recursive functions and lexical scoping
- Function composition `>>` (right-associative) and pipeline `|>` threading (value as first arg)
- Operator functions usable in prefix form and as HOF args
- Closures capture variables by reference; mutation across scopes is visible
- Error cases: calling non-function, wrong argument types to map/filter/fold
