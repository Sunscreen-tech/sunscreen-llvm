//=== Parasol.h - Standard library for Parasol targets --------------------===//
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information
//
//===----------------------------------------------------------------------===//

#ifndef PARASOL_H
#define PARASOL_H

// Type definitions
typedef _Bool bool;
#define true 1
#define false 0
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

// Conditional selector functions
// On Parasol target: use FHE-friendly cmux instruction
// On other targets: use standard ternary operator

#if defined(__parasol__)
// FHE-friendly conditional selector using inline assembly
#define DEFINE_SELECT(BITS)                                                    \
  inline uint##BITS##_t select##BITS(bool cond, uint##BITS##_t a,              \
                                     uint##BITS##_t b) {                       \
    register uint##BITS##_t result;                                            \
    asm volatile("cmux %0, %1, %2, %3"                                         \
                 : "=r"(result)                                                \
                 : "r"(cond), "r"(a), "r"(b)                                   \
                 :);                                                           \
    return result;                                                             \
  }                                                                            \
  inline int##BITS##_t iselect##BITS(bool cond, int##BITS##_t a,               \
                                     int##BITS##_t b) {                        \
    register int##BITS##_t result;                                             \
    asm volatile("cmux %0, %1, %2, %3"                                         \
                 : "=r"(result)                                                \
                 : "r"(cond), "r"(a), "r"(b)                                   \
                 :);                                                           \
    return result;                                                             \
  }

#else
// Standard C implementation using ternary operator
#define DEFINE_SELECT(BITS)                                                    \
  inline uint##BITS##_t select##BITS(bool cond, uint##BITS##_t a,              \
                                     uint##BITS##_t b) {                       \
    return cond ? a : b;                                                       \
  }                                                                            \
  inline int##BITS##_t iselect##BITS(bool cond, int##BITS##_t a,               \
                                     int##BITS##_t b) {                        \
    return cond ? a : b;                                                       \
  }

#endif

// Generate select functions for different bit sizes
DEFINE_SELECT(8)
DEFINE_SELECT(16)
DEFINE_SELECT(32)
DEFINE_SELECT(64)

#endif // PARASOL_H
