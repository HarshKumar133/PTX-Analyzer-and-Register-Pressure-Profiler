# Register pressure

At each instruction, PTXScope counts the union of live-in and live-out values. It selects the instruction boundary with the largest combined count and reports the split at that boundary: GPR-like virtual registers, predicate registers, and recognized special-register-like values. Declarations are counted separately and expanded declaration arrays contribute each virtual name.

The estimate is based on a static liveness model and can be conservative, incomplete, or affected by unsupported instruction effects. It is not the number of registers PTXAS allocates. Actual allocation, occupancy, and spills depend on compiler transformations, target architecture, launch configuration, and hardware resource limits. PTXScope does not calculate GPU runtime or occupancy.
