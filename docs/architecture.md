# Architecture

The implementation uses small stages and stable vectors for source order. The analyzer does not execute PTX.

```mermaid
flowchart TD
    S[Source text] --> L[Lexer]
    L --> P[Parser and function IR]
    P --> B[Basic-block partition]
    B --> C[Direct-branch CFG]
    C --> E[Per-instruction register effects]
    E --> D[Block USE/DEF]
    D --> F[Backward fixed point]
    F --> I[Instruction liveness]
    I --> R[Pressure and pattern diagnostics]
    R --> T[Text or versioned JSON]
```

`model.hpp` contains source locations, diagnostics, tokens, structured operands, instructions, declarations, functions, blocks, and result types. `lexer.cpp` only handles token boundaries. `parser.cpp` builds the accepted subset IR. `analysis.cpp` builds CFG and effect/liveness/pressure data. `report.cpp` serializes stable text and JSON. The CLI owns file I/O and exit-code policy.

All control-flow and liveness are intraprocedural. Function IDs, instruction positions, and block IDs follow source order. Ordered sets keep register and diagnostic output deterministic.
