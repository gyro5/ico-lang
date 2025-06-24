# Adding features

## Adding OpCode

When adding a new opcode, the following files need to be updated:

- `ico_chunk.h`: enum.
- `ico_vm_goto.h`: computed goto table.
- `ico_debug.c`: disassembling print.
- `ico_vm.c`: implementation in the VM.

## Adding `Obj` subtype

When adding a new subtype for `Obj`, the following files need to be updated:

- `ico_object.h`: definition and macros.
- `ico_object.c`: print, "constructor" function.
- `ico_memory.c`: free, blacken.