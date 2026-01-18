# JS Engine Optimization Plan

## 1. NaN-Boxing Value Representation

### Current Problem
- Value class is ~48 bytes (type enum + union + 2 shared_ptrs)
- Every value copy is expensive
- Poor cache locality

### Solution: NaN-Boxing
```
IEEE 754 Double (64 bits):
[Sign:1][Exponent:11][Mantissa:52]

NaN: Exponent = 0x7FF, Mantissa != 0

Our Encoding:
- Normal doubles: stored as-is
- Tagged values: use quiet NaN space (high bits = 0xFFF8+)

Layout for tagged values:
Bits 63-48: Tag (0xFFFA-0xFFFF)
Bits 47-0:  Payload (48 bits, enough for pointers on x64)

Tags:
0xFFFA = Integer (i32 in lower 32 bits)
0xFFFB = Undefined
0xFFFC = Null
0xFFFD = Boolean (0=false, 1=true in payload)
0xFFFE = Object pointer
0xFFFF = String pointer
```

### Benefits
- Value size: 48 bytes → 8 bytes (6x smaller)
- Type check: enum comparison → bit mask (faster)
- Cache locality: 6x more values per cache line

## 2. Computed Goto (Direct Threading)

### Current Problem
```cpp
while (running) {
    switch (opcode) {  // Branch predictor struggles
        case Add: ... break;
        case Sub: ... break;
    }
}
```
- Switch dispatch has indirect branch overhead
- Branch predictor can't optimize well

### Solution: Computed Goto (GCC Extension)
```cpp
static void* table[] = {&&op_add, &&op_sub, ...};
#define DISPATCH() goto *table[READ_OP()]

op_add:
    // implementation
    DISPATCH();

op_sub:
    // implementation
    DISPATCH();
```

### Benefits
- Eliminates switch overhead
- Each opcode ends with direct goto
- Better branch prediction (each site has own history)
- ~20-30% speedup on interpreter-bound code
