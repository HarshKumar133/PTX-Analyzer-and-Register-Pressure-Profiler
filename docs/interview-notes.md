# Interview notes

- **PTX:** a virtual ISA and intermediate representation that NVIDIA toolchains can lower to architecture-specific machine code.
- **Operands:** identify destinations, sources, predicates, immediates, labels, symbols, or address components. Their roles depend on the opcode.
- **Basic block:** a straight-line instruction sequence with one entry and control-transfer boundaries at its ends.
- **CFG:** a graph of blocks and possible direct transfers; it makes branch joins, loops, and reachability explicit.
- **USE/DEF:** registers read before a local definition, and registers written within the block.
- **Liveness:** `OUT[B]` is the union of successor `IN` sets; `IN[B] = USE[B] union (OUT[B] - DEF[B])`. The solver iterates because a loop's later iteration can make a value live earlier.
- **Predicated writes:** a false guard can leave the destination unchanged, so its old value may be needed. This implementation models a guarded destination as a read and write.
- **Virtual versus physical registers:** PTX names are compiler IR values. Optimization, coalescing, allocation, spilling, and target constraints separate them from hardware registers.
- **Soundness assumptions:** only modeled effects and direct control flow can support a reliable result. Unknown effects are surfaced as incomplete rather than treated as no-ops.
- **Not a backend:** PTXScope does not select machine instructions, allocate hardware registers, assemble code, or execute a kernel.
