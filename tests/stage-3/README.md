Stage 3 focuses on evaluation semantics. It verifies:

- `puts` prints canonical values and returns `nil`
- Evaluating literals and showing normalized output (e.g., numeric underscores removed)
- Arithmetic semantics including precedence and operator function forms, truncating integer division, and decimal math
- Variables: immutable `let`, mutable `let mut`, assignment, and error on reassigning immutable
- Mixed-type operations and string concatenation; logical `&&`/`||` use truthiness and return Booleans
- String repetition with non-negative integers; `"a" * 0 -> ""`; errors for negative/decimal counts
- Division by zero error
- Unknown identifier error
