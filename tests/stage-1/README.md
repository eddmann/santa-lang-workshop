Stage 1 focuses on the lexer. It verifies tokenization of:

- Literals: integers, decimals (with underscores), strings (UTF-8, escapes), booleans, nil
- Operators and symbols: +-\*/ = {} [] #{ == != > < >= <= && || |> >> ;
- Keywords and identifiers: let, mut, if, else, and identifier forms like x, y_123
- Line comments: // ... both standalone and trailing; comments are surfaced as CMT tokens

Outcome: Input source is split into correct token stream matching `LANG.md` lexical rules.
