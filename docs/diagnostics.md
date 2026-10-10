# Diagnostics

Diagnostics contain a stable code, severity, message, file/line/column, and optional note. Current families include `PTX100x` lexer failures, `PTX110x` unsupported/malformed declarations, `PTX112x` instruction parse failures, `PTX210x` control-flow validation, `PTX220x` static findings and incomplete effects, and `PTX230x` solver failure.

Unknown syntax and semantics are not silently described as fully understood. Unreachable blocks and self-moves are reported as observations or candidates, not guaranteed performance issues. Diagnostics sort by source file, line, column, then code.
