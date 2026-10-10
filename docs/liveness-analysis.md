# Register liveness

The current model keys registers by their PTX spelling. For each supported instruction, `USE` contains read registers and `DEF` contains written registers. Block sets are computed in source order, then a backward fixed point solves:

```text
OUT[B] = union(IN[S]) for each successor S
IN[B]  = USE[B] union (OUT[B] - DEF[B])
```

Instruction sets are calculated by a reverse walk from each block's `OUT`. A guarded instruction reads its guard predicate. Guarded destinations are treated as both reads and writes, so the old value remains live when the guard is false.

The model is intraprocedural. It does not model memory aliasing, calls, special register side effects, or all opcode-specific effects. Unknown instructions treat extracted registers as reads and are explicitly marked incomplete; this cannot account for unknown writes, so downstream sets must not be treated as exact. The iteration cap is a defensive bound based on blocks and observed registers; failure emits `PTX2301`.
