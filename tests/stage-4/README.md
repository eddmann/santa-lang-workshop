Stage 4 adds immutable collections and indexing semantics. It verifies:

- Immutability: `push` returns new lists/sets; original unchanged. No List + Integer or Set + Integer.
- Set `push` avoids duplicates; size reflects cardinality.
- Dictionary `assoc` returns new dict with key set/overridden; right-biased `+` merge
- Collection `+`: List concatenation, Set union
- Printed order for collections: Dictionaries print ascending by key; Sets print ascending by value. Ordering is deterministic and stable across runs.
- Built-ins: `first`, `rest`, `size` for lists/strings/sets/dicts; Unicode size example
- Indexing rules: strings and lists support zero-based and negative indices; OOB -> nil
- Dictionary indexing: returns value or nil; supports complex keys with structural equality; dict as dict key is an error
- Type errors on non-integer indices for lists/strings; set literal cannot contain a dictionary
- Structural equality for all values (collections compare by structure, order-insensitive where specified)
