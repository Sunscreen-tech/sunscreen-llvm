// Parasol.h - Standard library for Parasol targets
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information

#ifndef PARASOL_H
#define PARASOL_H

// Type definitions
#ifndef __cplusplus
#ifndef bool
typedef _Bool bool;
#define true 1
#define false 0
#endif
#endif
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

#if defined(__Parasol__)
// FHE-friendly conditional selector using inline assembly
#define DEFINE_SELECT(BITS)                                                    \
  static inline uint##BITS##_t select##BITS(bool cond, uint##BITS##_t a,       \
                                            uint##BITS##_t b) {                \
    register uint##BITS##_t result;                                            \
    asm volatile("cmux %0, %1, %2, %3"                                         \
                 : "=r"(result)                                                \
                 : "r"(cond), "r"(a), "r"(b)                                   \
                 :);                                                           \
    return result;                                                             \
  }                                                                            \
  static inline int##BITS##_t iselect##BITS(bool cond, int##BITS##_t a,        \
                                            int##BITS##_t b) {                 \
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
  static inline uint##BITS##_t select##BITS(bool cond, uint##BITS##_t a,       \
                                            uint##BITS##_t b) {                \
    return cond ? a : b;                                                       \
  }                                                                            \
  static inline int##BITS##_t iselect##BITS(bool cond, int##BITS##_t a,        \
                                            int##BITS##_t b) {                 \
    return cond ? a : b;                                                       \
  }

#endif

// Generate select functions for different bit sizes
DEFINE_SELECT(8)
DEFINE_SELECT(16)
DEFINE_SELECT(32)
DEFINE_SELECT(64)

// Absolute value functions (signed)
// Note: abs64 omitted - requires 64-bit shift operation (>> 63) not supported by parasol target
#define abs8(x) iselect8((x) < 0, -(x), (x))
#define abs16(x) iselect16((x) < 0, -(x), (x))
#define abs32(x) iselect32((x) < 0, -(x), (x))

// Integer square root macro for defining sqrt functions
// Uses digit-by-digit binary algorithm building result from MSB to LSB
#define DEFINE_SQRT(name, input_type, result_type, start_bit, select_func)     \
  static inline result_type name(input_type n) {                               \
    result_type result = 0;                                                    \
    for (result_type bit = start_bit; bit > 0; bit >>= 1) {                    \
      result_type test = result | bit;                                         \
      bool fits = ((input_type)test * (input_type)test) <= n;                  \
      result = select_func(fits, test, result);                                \
    }                                                                          \
    return result;                                                             \
  }

// sqrt8: Integer square root for 8-bit inputs (0-255) -> (0-15)
DEFINE_SQRT(sqrt8, uint8_t, uint8_t, 8, select8)

// sqrt16: Integer square root for 16-bit inputs (0-65535) -> (0-255)
DEFINE_SQRT(sqrt16, uint16_t, uint8_t, 128, select8)

// sqrt32: Integer square root for 32-bit inputs (0-4294967295) -> (0-65535)
DEFINE_SQRT(sqrt32, uint32_t, uint16_t, 32768, select16)

// Minimum functions (unsigned)
#define min8(a, b) select8((a) < (b), (a), (b))
#define min16(a, b) select16((a) < (b), (a), (b))
#define min32(a, b) select32((a) < (b), (a), (b))
#define min64(a, b) select64((a) < (b), (a), (b))

// Maximum functions (unsigned)
#define max8(a, b) select8((a) > (b), (a), (b))
#define max16(a, b) select16((a) > (b), (a), (b))
#define max32(a, b) select32((a) > (b), (a), (b))
#define max64(a, b) select64((a) > (b), (a), (b))

// Minimum functions (signed)
#define imin8(a, b) iselect8((a) < (b), (a), (b))
#define imin16(a, b) iselect16((a) < (b), (a), (b))
#define imin32(a, b) iselect32((a) < (b), (a), (b))
#define imin64(a, b) iselect64((a) < (b), (a), (b))

// Maximum functions (signed)
#define imax8(a, b) iselect8((a) > (b), (a), (b))
#define imax16(a, b) iselect16((a) > (b), (a), (b))
#define imax32(a, b) iselect32((a) > (b), (a), (b))
#define imax64(a, b) iselect64((a) > (b), (a), (b))

// Clamp functions (unsigned) - Restricts value to [min_val, max_val]
#define clamp8(x, min_val, max_val) min8(max8((x), (min_val)), (max_val))
#define clamp16(x, min_val, max_val) min16(max16((x), (min_val)), (max_val))
#define clamp32(x, min_val, max_val) min32(max32((x), (min_val)), (max_val))
#define clamp64(x, min_val, max_val) min64(max64((x), (min_val)), (max_val))

// Clamp functions (signed)
#define iclamp8(x, min_val, max_val) imin8(imax8((x), (min_val)), (max_val))
#define iclamp16(x, min_val, max_val) imin16(imax16((x), (min_val)), (max_val))
#define iclamp32(x, min_val, max_val) imin32(imax32((x), (min_val)), (max_val))
#define iclamp64(x, min_val, max_val) imin64(imax64((x), (min_val)), (max_val))

// Sign functions - Returns -1, 0, or 1 based on sign
// Note: sign64 omitted - generates 64-bit constant (-1) not supported by parasol target
#define sign8(x) iselect8((x) < 0, -1, iselect8((x) > 0, 1, 0))
#define sign16(x) iselect16((x) < 0, -1, iselect16((x) > 0, 1, 0))
#define sign32(x) iselect32((x) < 0, -1, iselect32((x) > 0, 1, 0))

// Conditional swap - Swaps values if condition is true
#define cswap8(cond, a, b)                                                     \
  do {                                                                         \
    uint8_t _cswap_tmp_a = (a);                                                \
    uint8_t _cswap_tmp_b = (b);                                                \
    (a) = select8((cond), _cswap_tmp_b, _cswap_tmp_a);                         \
    (b) = select8((cond), _cswap_tmp_a, _cswap_tmp_b);                         \
  } while (0)

#define cswap16(cond, a, b)                                                    \
  do {                                                                         \
    uint16_t _cswap_tmp_a = (a);                                               \
    uint16_t _cswap_tmp_b = (b);                                               \
    (a) = select16((cond), _cswap_tmp_b, _cswap_tmp_a);                        \
    (b) = select16((cond), _cswap_tmp_a, _cswap_tmp_b);                        \
  } while (0)

#define cswap32(cond, a, b)                                                    \
  do {                                                                         \
    uint32_t _cswap_tmp_a = (a);                                               \
    uint32_t _cswap_tmp_b = (b);                                               \
    (a) = select32((cond), _cswap_tmp_b, _cswap_tmp_a);                        \
    (b) = select32((cond), _cswap_tmp_a, _cswap_tmp_b);                        \
  } while (0)

#define cswap64(cond, a, b)                                                    \
  do {                                                                         \
    uint64_t _cswap_tmp_a = (a);                                               \
    uint64_t _cswap_tmp_b = (b);                                               \
    (a) = select64((cond), _cswap_tmp_b, _cswap_tmp_a);                        \
    (b) = select64((cond), _cswap_tmp_a, _cswap_tmp_b);                        \
  } while (0)

// Conditional swap (signed)
#define icswap8(cond, a, b)                                                    \
  do {                                                                         \
    int8_t _icswap_tmp_a = (a);                                                \
    int8_t _icswap_tmp_b = (b);                                                \
    (a) = iselect8((cond), _icswap_tmp_b, _icswap_tmp_a);                      \
    (b) = iselect8((cond), _icswap_tmp_a, _icswap_tmp_b);                      \
  } while (0)

#define icswap16(cond, a, b)                                                   \
  do {                                                                         \
    int16_t _icswap_tmp_a = (a);                                               \
    int16_t _icswap_tmp_b = (b);                                               \
    (a) = iselect16((cond), _icswap_tmp_b, _icswap_tmp_a);                     \
    (b) = iselect16((cond), _icswap_tmp_a, _icswap_tmp_b);                     \
  } while (0)

#define icswap32(cond, a, b)                                                   \
  do {                                                                         \
    int32_t _icswap_tmp_a = (a);                                               \
    int32_t _icswap_tmp_b = (b);                                               \
    (a) = iselect32((cond), _icswap_tmp_b, _icswap_tmp_a);                     \
    (b) = iselect32((cond), _icswap_tmp_a, _icswap_tmp_b);                     \
  } while (0)

#define icswap64(cond, a, b)                                                   \
  do {                                                                         \
    int64_t _icswap_tmp_a = (a);                                               \
    int64_t _icswap_tmp_b = (b);                                               \
    (a) = iselect64((cond), _icswap_tmp_b, _icswap_tmp_a);                     \
    (b) = iselect64((cond), _icswap_tmp_a, _icswap_tmp_b);                     \
  } while (0)

// Average/midpoint functions (unsigned) - Avoids overflow
// Note: avg64 omitted - requires 64-bit shift operation (>> 1) not supported by parasol target
#define avg8(a, b) ((uint8_t)(((a) & (b)) + (((a) ^ (b)) >> 1)))
#define avg16(a, b) ((uint16_t)(((a) & (b)) + (((a) ^ (b)) >> 1)))
#define avg32(a, b) ((uint32_t)(((a) & (b)) + (((a) ^ (b)) >> 1)))

// Absolute difference (unsigned result, branchless)
#define absdiff8(a, b) select8((a) > (b), (a) - (b), (b) - (a))
#define absdiff16(a, b) select16((a) > (b), (a) - (b), (b) - (a))
#define absdiff32(a, b) select32((a) > (b), (a) - (b), (b) - (a))
#define absdiff64(a, b) select64((a) > (b), (a) - (b), (b) - (a))

// Note: iabsdiff64 omitted - generates 64-bit shift (>> 63) during optimization, not supported by parasol target
#define iabsdiff8(a, b) ((uint8_t)iselect8((a) > (b), (a) - (b), (b) - (a)))
#define iabsdiff16(a, b) ((uint16_t)iselect16((a) > (b), (a) - (b), (b) - (a)))
#define iabsdiff32(a, b) ((uint32_t)iselect32((a) > (b), (a) - (b), (b) - (a)))

// Saturating arithmetic (clamps instead of wrapping on overflow/underflow)
// Note: sadd64, ssub64 omitted - require 64-bit constants not supported by parasol target
#define sadd8(a, b) select8((a) > (uint8_t)(255U - (b)), 255, (a) + (b))
#define sadd16(a, b) select16((a) > (uint16_t)(65535U - (b)), 65535, (a) + (b))
#define sadd32(a, b) select32((a) > (4294967295U - (b)), 4294967295U, (a) + (b))

#define ssub8(a, b) select8((a) < (b), 0, (a) - (b))
#define ssub16(a, b) select16((a) < (b), 0, (a) - (b))
#define ssub32(a, b) select32((a) < (b), 0, (a) - (b))

// Signed saturating arithmetic (clamps to INT_MIN/INT_MAX)
#define DEFINE_ISADD(SUFFIX, TYPE, UTYPE, SELECT_FUNC, MIN_VAL, MAX_VAL)       \
  static inline TYPE isadd##SUFFIX(TYPE a, TYPE b) {                           \
    TYPE sum = a + b;                                                          \
    bool pos_overflow = (a > 0) && (b > 0) && (sum < 0);                       \
    bool neg_overflow = (a < 0) && (b < 0) && (sum > 0);                       \
    return SELECT_FUNC(pos_overflow, MAX_VAL,                                  \
                       SELECT_FUNC(neg_overflow, MIN_VAL, sum));               \
  }

#define DEFINE_ISSUB(SUFFIX, TYPE, UTYPE, SELECT_FUNC, MIN_VAL, MAX_VAL)       \
  static inline TYPE issub##SUFFIX(TYPE a, TYPE b) {                           \
    TYPE diff = a - b;                                                         \
    bool pos_overflow = (a > 0) && (b < 0) && (diff < 0);                      \
    bool neg_overflow = (a < 0) && (b > 0) && (diff > 0);                      \
    return SELECT_FUNC(pos_overflow, MAX_VAL,                                  \
                       SELECT_FUNC(neg_overflow, MIN_VAL, diff));              \
  }

DEFINE_ISADD(8, int8_t, uint8_t, iselect8, -128, 127)
DEFINE_ISADD(16, int16_t, uint16_t, iselect16, -32768, 32767)
DEFINE_ISADD(32, int32_t, uint32_t, iselect32, -2147483648, 2147483647)
DEFINE_ISADD(64, int64_t, uint64_t, iselect64, -9223372036854775807LL - 1,
             9223372036854775807LL)

DEFINE_ISSUB(8, int8_t, uint8_t, iselect8, -128, 127)
DEFINE_ISSUB(16, int16_t, uint16_t, iselect16, -32768, 32767)
DEFINE_ISSUB(32, int32_t, uint32_t, iselect32, -2147483648, 2147483647)
DEFINE_ISSUB(64, int64_t, uint64_t, iselect64, -9223372036854775807LL - 1,
             9223372036854775807LL)

// Power functions using repeated squaring (depth O(log exp))
// Supports exponents 0-15 with constant circuit depth per exponent value
#define DEFINE_POW(SUFFIX, TYPE, SELECT_FUNC)                                  \
  static inline TYPE pow##SUFFIX(TYPE base, uint8_t exp) {                     \
    TYPE result = 1;                                                           \
    TYPE p2 = base;                                                            \
    TYPE p4 = p2 * p2;                                                         \
    TYPE p8 = p4 * p4;                                                         \
    result = SELECT_FUNC((exp & 1) != 0, result * p2, result);                 \
    result = SELECT_FUNC((exp & 2) != 0, result * p4, result);                 \
    result = SELECT_FUNC((exp & 4) != 0, result * p8, result);                 \
    result = SELECT_FUNC((exp & 8) != 0, result * p8 * p8, result);            \
    return result;                                                             \
  }

DEFINE_POW(8, uint8_t, select8)
DEFINE_POW(16, uint16_t, select16)
DEFINE_POW(32, uint32_t, select32)
DEFINE_POW(64, uint64_t, select64)

DEFINE_POW(i8, int8_t, iselect8)
DEFINE_POW(i16, int16_t, iselect16)
DEFINE_POW(i32, int32_t, iselect32)
DEFINE_POW(i64, int64_t, iselect64)

// Parity and utility bit checks
// Note: is_even64, is_odd64 omitted - require 64-bit constant (1) not supported by parasol target
#define is_even8(x) (((x) & 1) == 0)
#define is_even16(x) (((x) & 1) == 0)
#define is_even32(x) (((x) & 1) == 0)

#define is_odd8(x) (((x) & 1) == 1)
#define is_odd16(x) (((x) & 1) == 1)
#define is_odd32(x) (((x) & 1) == 1)

// Bit extraction (returns the nth bit as a boolean)
// Note: get_bit64 omitted - requires 64-bit shift operation not supported by parasol target
static inline bool get_bit8(uint8_t value, unsigned int n) {
  return ((value >> n) & 0x1);
}

static inline bool get_bit16(uint16_t value, unsigned int n) {
  return ((value >> n) & 0x1);
}

static inline bool get_bit32(uint32_t value, unsigned int n) {
  return ((value >> n) & 0x1);
}

// Array operations using tree reduction (depth O(log N)) and bitonic sort
// (depth O(log^2 N)).
//
// Reductions:
// - Require len > 0 (undefined behavior if len == 0)
// - min/max require scratch buffer with len elements of same type as array
// - sum/avg require scratch buffer with len elements of uint64_t/int64_t
// - sum operations may overflow for large arrays (wraps modulo 2^64)
//
// Bitonic sort:
// - Requires n to be power-of-2 (2, 4, 8, 16, ...) - undefined behavior
// otherwise
// - Sorts are not stable (equal elements may be reordered)
// - In-place modification

// Tree reduction for min - finds minimum element in array
#define DEFINE_MIN_ARRAY(SUFFIX, TYPE, SELECT_FUNC)                            \
  static inline TYPE min##SUFFIX##_array(const TYPE *arr, uint16_t len,        \
                                         TYPE *scratch) {                      \
    for (uint32_t i = 0; i < len; i++) {                                       \
      scratch[i] = arr[i];                                                     \
    }                                                                          \
    for (uint32_t current_len = len; current_len > 1;) {                       \
      uint32_t next_len = current_len / 2;                                     \
      for (uint32_t i = 0; i < next_len; i++) {                                \
        TYPE a = scratch[2 * i];                                               \
        TYPE b = scratch[2 * i + 1];                                           \
        scratch[i] = SELECT_FUNC((a) < (b), a, b);                             \
      }                                                                        \
      if (current_len % 2 == 1) {                                              \
        scratch[next_len] = scratch[current_len - 1];                          \
        next_len++;                                                            \
      }                                                                        \
      current_len = next_len;                                                  \
    }                                                                          \
    return scratch[0];                                                         \
  }

// Tree reduction for max - finds maximum element in array
#define DEFINE_MAX_ARRAY(SUFFIX, TYPE, SELECT_FUNC)                            \
  static inline TYPE max##SUFFIX##_array(const TYPE *arr, uint16_t len,        \
                                         TYPE *scratch) {                      \
    for (uint32_t i = 0; i < len; i++) {                                       \
      scratch[i] = arr[i];                                                     \
    }                                                                          \
    for (uint32_t current_len = len; current_len > 1;) {                       \
      uint32_t next_len = current_len / 2;                                     \
      for (uint32_t i = 0; i < next_len; i++) {                                \
        TYPE a = scratch[2 * i];                                               \
        TYPE b = scratch[2 * i + 1];                                           \
        scratch[i] = SELECT_FUNC((a) > (b), a, b);                             \
      }                                                                        \
      if (current_len % 2 == 1) {                                              \
        scratch[next_len] = scratch[current_len - 1];                          \
        next_len++;                                                            \
      }                                                                        \
      current_len = next_len;                                                  \
    }                                                                          \
    return scratch[0];                                                         \
  }

// Tree reduction for sum - uses wider accumulator to reduce overflow risk
#define DEFINE_SUM_ARRAY(NAME, INPUT_TYPE, SUM_TYPE)                           \
  static inline SUM_TYPE NAME##_array(                                         \
      const INPUT_TYPE *arr, uint16_t len, SUM_TYPE *scratch) {                \
    for (uint32_t i = 0; i < len; i++) {                                       \
      scratch[i] = (SUM_TYPE)arr[i];                                           \
    }                                                                          \
    for (uint32_t current_len = len; current_len > 1;) {                       \
      uint32_t next_len = current_len / 2;                                     \
      for (uint32_t i = 0; i < next_len; i++) {                                \
        scratch[i] = scratch[2 * i] + scratch[2 * i + 1];                      \
      }                                                                        \
      if (current_len % 2 == 1) {                                              \
        scratch[next_len] = scratch[current_len - 1];                          \
        next_len++;                                                            \
      }                                                                        \
      current_len = next_len;                                                  \
    }                                                                          \
    return scratch[0];                                                         \
  }

// Bitonic sort helper - compare and swap elements based on direction
#define DEFINE_COMPARE_SWAP(TYPE_NAME, TYPE, SELECT_FUNC)                      \
  static inline void compare_swap_##TYPE_NAME(TYPE *arr, uint16_t idx_a,       \
                                              uint16_t idx_b, bool dir_asc) {  \
    TYPE a = arr[idx_a];                                                       \
    TYPE b = arr[idx_b];                                                       \
    bool a_gt_b = (a > b);                                                     \
    TYPE lesser = SELECT_FUNC(a_gt_b, b, a);                                   \
    TYPE greater = SELECT_FUNC(a_gt_b, a, b);                                  \
    arr[idx_a] = SELECT_FUNC(dir_asc, lesser, greater);                        \
    arr[idx_b] = SELECT_FUNC(dir_asc, greater, lesser);                        \
  }

// Sort - uses bitonic sort algorithm, requires power-of-2 array size
#define DEFINE_BITONIC_SORT(TYPE_NAME, TYPE)                                   \
  static inline void sort_##TYPE_NAME##_asc(TYPE *arr, uint16_t n) {           \
    for (uint16_t k = 2; k <= n; k <<= 1) {                                    \
      for (uint16_t j = k >> 1; j > 0; j >>= 1) {                              \
        for (uint16_t i = 0; i < n; i++) {                                     \
          uint16_t ixj = i ^ j;                                                \
          if (ixj > i) {                                                       \
            bool ascending = ((i & k) == 0);                                   \
            compare_swap_##TYPE_NAME(arr, i, ixj, ascending);                  \
          }                                                                    \
        }                                                                      \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  static inline void sort_##TYPE_NAME##_desc(TYPE *arr, uint16_t n) {          \
    for (uint16_t k = 2; k <= n; k <<= 1) {                                    \
      for (uint16_t j = k >> 1; j > 0; j >>= 1) {                              \
        for (uint16_t i = 0; i < n; i++) {                                     \
          uint16_t ixj = i ^ j;                                                \
          if (ixj > i) {                                                       \
            bool ascending = ((i & k) == 0);                                   \
            compare_swap_##TYPE_NAME(arr, i, ixj, !ascending);                 \
          }                                                                    \
        }                                                                      \
      }                                                                        \
    }                                                                          \
  }

// Generate functions for unsigned types
#define GENERATE_ARRAY_OPS_UNSIGNED(BITS)                                      \
  DEFINE_MIN_ARRAY(BITS, uint##BITS##_t, select##BITS)                         \
  DEFINE_MAX_ARRAY(BITS, uint##BITS##_t, select##BITS)                         \
  DEFINE_COMPARE_SWAP(uint##BITS, uint##BITS##_t, select##BITS)                \
  DEFINE_BITONIC_SORT(uint##BITS, uint##BITS##_t)

// Signed type wrappers that add 'i' prefix correctly
#define DEFINE_MIN_ARRAY_SIGNED(BITS)                                          \
  static inline int##BITS##_t imin##BITS##_array(                              \
      const int##BITS##_t *arr, uint16_t len, int##BITS##_t *scratch) {        \
    for (uint32_t i = 0; i < len; i++) {                                       \
      scratch[i] = arr[i];                                                     \
    }                                                                          \
    for (uint32_t current_len = len; current_len > 1;) {                       \
      uint32_t next_len = current_len / 2;                                     \
      for (uint32_t i = 0; i < next_len; i++) {                                \
        int##BITS##_t a = scratch[2 * i];                                      \
        int##BITS##_t b = scratch[2 * i + 1];                                  \
        scratch[i] = iselect##BITS((a) < (b), a, b);                           \
      }                                                                        \
      if (current_len % 2 == 1) {                                              \
        scratch[next_len] = scratch[current_len - 1];                          \
        next_len++;                                                            \
      }                                                                        \
      current_len = next_len;                                                  \
    }                                                                          \
    return scratch[0];                                                         \
  }

#define DEFINE_MAX_ARRAY_SIGNED(BITS)                                          \
  static inline int##BITS##_t imax##BITS##_array(                              \
      const int##BITS##_t *arr, uint16_t len, int##BITS##_t *scratch) {        \
    for (uint32_t i = 0; i < len; i++) {                                       \
      scratch[i] = arr[i];                                                     \
    }                                                                          \
    for (uint32_t current_len = len; current_len > 1;) {                       \
      uint32_t next_len = current_len / 2;                                     \
      for (uint32_t i = 0; i < next_len; i++) {                                \
        int##BITS##_t a = scratch[2 * i];                                      \
        int##BITS##_t b = scratch[2 * i + 1];                                  \
        scratch[i] = iselect##BITS((a) > (b), a, b);                           \
      }                                                                        \
      if (current_len % 2 == 1) {                                              \
        scratch[next_len] = scratch[current_len - 1];                          \
        next_len++;                                                            \
      }                                                                        \
      current_len = next_len;                                                  \
    }                                                                          \
    return scratch[0];                                                         \
  }

// Generate functions for signed types
#define GENERATE_ARRAY_OPS_SIGNED(BITS)                                        \
  DEFINE_MIN_ARRAY_SIGNED(BITS)                                                \
  DEFINE_MAX_ARRAY_SIGNED(BITS)                                                \
  DEFINE_COMPARE_SWAP(int##BITS, int##BITS##_t, iselect##BITS)                 \
  DEFINE_BITONIC_SORT(int##BITS, int##BITS##_t)

GENERATE_ARRAY_OPS_UNSIGNED(8)
GENERATE_ARRAY_OPS_UNSIGNED(16)
GENERATE_ARRAY_OPS_UNSIGNED(32)

// Sum functions for unsigned types
// Function naming: sum<input_bits>_<output_bits>_array
//
// Choosing accumulator size:
//   - Known small arrays or bounded sums: use smallest safe accumulator
//   - Unknown/dynamic array sizes: use next larger accumulator for safety margin
//   - Critical correctness requirements: use largest accumulator (64-bit)
//   - Performance-critical with validated bounds: use matching-size accumulator
//
// Overflow safety: max_safe_elements = MAX_ACCUMULATOR / MAX_INPUT_VALUE
//   Example: sum8_16 safe for 65535/255 = 257 elements of max value

// 8-bit input sum functions
DEFINE_SUM_ARRAY(sum8_16, uint8_t, uint16_t)   // Up to 257 elements of max value (255)
DEFINE_SUM_ARRAY(sum8_32, uint8_t, uint32_t)   // Up to 16M elements of max value
DEFINE_SUM_ARRAY(sum8_64, uint8_t, uint64_t)   // Maximum safety (72+ quadrillion elements)

// 16-bit input sum functions
DEFINE_SUM_ARRAY(sum16_32, uint16_t, uint32_t) // Up to 65537 elements of max value (65535)
DEFINE_SUM_ARRAY(sum16_64, uint16_t, uint64_t) // Maximum safety (281+ trillion elements)

// 32-bit input sum functions
DEFINE_SUM_ARRAY(sum32_64, uint32_t, uint64_t) // Safe default (4+ billion elements of max value)
DEFINE_SUM_ARRAY(sum32_32, uint32_t, uint32_t) // Performance option (overflow possible, use with care)

// 64-bit input sum function
DEFINE_SUM_ARRAY(sum64_64, uint64_t, uint64_t) // Same size (no 128-bit available, overflow possible)

// Generate 64-bit unsigned functions except sum (sum64 would overflow without 128-bit accumulator)
DEFINE_MIN_ARRAY(64, uint64_t, select64)
DEFINE_MAX_ARRAY(64, uint64_t, select64)
DEFINE_COMPARE_SWAP(uint64, uint64_t, select64)
DEFINE_BITONIC_SORT(uint64, uint64_t)

GENERATE_ARRAY_OPS_SIGNED(8)
GENERATE_ARRAY_OPS_SIGNED(16)
GENERATE_ARRAY_OPS_SIGNED(32)

// Sum functions for signed types
// Function naming: isum<input_bits>_<output_bits>_array
// See unsigned sum functions above for accumulator size selection guidance
//
// Overflow safety for signed: check both positive and negative bounds
//   Positive: max_safe_elements = MAX_ACCUMULATOR / MAX_INPUT_VALUE
//   Negative: max_safe_elements = abs(MIN_ACCUMULATOR) / abs(MIN_INPUT_VALUE)

// 8-bit input sum functions
DEFINE_SUM_ARRAY(isum8_16, int8_t, int16_t)   // Up to 257 elements (32767/127, 32768/128)
DEFINE_SUM_ARRAY(isum8_32, int8_t, int32_t)   // Up to 16M elements of extreme values
DEFINE_SUM_ARRAY(isum8_64, int8_t, int64_t)   // Maximum safety

// 16-bit input sum functions
DEFINE_SUM_ARRAY(isum16_32, int16_t, int32_t) // Up to 65537 elements of extreme values
DEFINE_SUM_ARRAY(isum16_64, int16_t, int64_t) // Maximum safety

// 32-bit input sum functions
DEFINE_SUM_ARRAY(isum32_64, int32_t, int64_t) // Safe default (4+ billion elements)
DEFINE_SUM_ARRAY(isum32_32, int32_t, int32_t) // Performance option (overflow possible, use with care)

// 64-bit input sum function
DEFINE_SUM_ARRAY(isum64_64, int64_t, int64_t) // Same size (no 128-bit available, overflow possible)

// Generate 64-bit signed functions except sum (isum64 would overflow without 128-bit accumulator)
DEFINE_MIN_ARRAY_SIGNED(64)
DEFINE_MAX_ARRAY_SIGNED(64)
DEFINE_COMPARE_SWAP(int64, int64_t, iselect64)
DEFINE_BITONIC_SORT(int64, int64_t)

#endif // PARASOL_H
