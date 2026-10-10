# Limitations

- This is a parser for a tested subset, not a validating PTX assembler.
- Function parameter types and values are not retained as operands.
- Memory spaces, aliases, loads/stores, calls, and side effects are not modeled in depth.
- Instruction effect rules are generic for several families; unknown instructions and `brx` reduce confidence.
- Guarded writes are conservative; no path-sensitive predicate reasoning is attempted.
- Only direct labels in one function are resolved.
- Virtual-register pressure is not physical register allocation, occupancy, spill prediction, or runtime estimation.
- The supported version string is recorded, not used to gate every opcode/modifier by ISA release.

See the subset guide and diagnostics for current explicit behavior. Broader support should be added with official ISA references and paired positive/negative tests.
