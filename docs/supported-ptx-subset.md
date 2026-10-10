# Supported PTX subset

The parser accepts syntax by form; this is not a complete PTX ISA implementation. PTXScope records `.version` and `.target` but does not enforce a maximum PTX release. The syntax and semantics of PTX are based on NVIDIA's [PTX ISA](https://docs.nvidia.com/cuda/parallel-thread-execution/).

| Area | Supported forms | Analysis status |
|---|---|---|
| Metadata | `.version`, `.target`, `.address_size` | Values retained as strings |
| Functions | `.entry`, `.func`, simple named parameters, braced bodies | Parameters are skipped, not modeled |
| Declarations | `.reg` scalar names, comma lists, `%r<4>` arrays; body directives skipped with warning | Register declarations counted; other storage is not modeled |
| Labels | Identifier followed by `:` | Function scoped; duplicate and unresolved targets diagnosed |
| Guards | `@%pN` and `@!%pN` | Guard predicate is a read; guarded writes retain old-value liveness |
| Operands | Registers, integer/floating spellings, symbols, bracketed addresses, grouped text | Bracket registers are retained; memory aliasing/effects are not modeled |
| Direct control flow | `bra label`, `ret`, `exit`, `trap` | Direct branches, fall-through, reachability; indexed `brx`, `brk`, and `cont` are incomplete |
| Arithmetic/data | `mov`, `add`, `sub`, `mul`, `mad`, `fma`, `div`, `rem`, `min`, `max`, `and`, `or`, `xor`, `not`, `shl`, `shr`, `neg`, `abs`, `cvt`, `cvta`, `selp` | First operand is destination; remaining register operands are reads |
| Predicate | `setp` | First operand register(s) are definitions; remaining registers are reads |
| Memory | `ld`, `st` | Load destination and address/register reads; store operands read; memory behavior is not modeled |
| Other listed | `nop`, `bar`, `vote`, `slct`, `popc`, `clz`, `bfind`, `brev`, `bfe`, `bfi`, `prmt`, `testp`, `rcp`, `sqrt`, `rsqrt`, `sin`, `cos`, `ex2`, `lg2` | Generic destination/source pattern only where identified; some operand-specific effects may need refinement |

PTX statements such as complex tuples, vector operands, multi-result families other than `setp`, inline assembly payloads, interprocedural control flow, and special instruction-specific effects are not guaranteed. Unknown mnemonics are retained and produce `PTX2201`; their registers are treated as conservative reads, and effects are marked incomplete. Do not interpret `parse_valid` as ISA conformance.
