# Parsing design

The lexer preserves file, line, and column on every token. It distinguishes directives/modifiers, identifiers, `%` registers, numeric spellings, strings, punctuation, and EOF. It skips `//` and `/* ... */` comments, diagnosing unterminated block comments and strings.

The parser locates `.entry`/`.func` bodies and creates function, declaration, label, instruction, guard, modifier, and operand records. Operands retain source spelling and extracted register references. Labels and operands are not reduced to regex matches. Parse errors carry stable codes and source locations; the supported implementation does not attempt aggressive recovery beyond the current statement/function.

Grammar sketch:

```text
program       := (metadata | function | other-directive)*
function      := [.visible] ( .entry | .func ) name signature "{" body "}"
body          := (register-decl | skipped-directive | label | instruction)*
register-decl := .reg type (register ["<" integer ">"])? ("," register)* ";"
label         := identifier ":"
instruction   := ["@" ["!"] predicate] identifier modifier* operand-list ";"
operand-list  := operand ("," operand)*
```

This is a grammar-like description of the accepted core, not a complete PTX grammar. Parameter declarations are skipped as part of the header. Unsupported body directives emit a warning and are skipped to their semicolon. An unknown instruction remains in the IR and later marks semantic analysis incomplete.
