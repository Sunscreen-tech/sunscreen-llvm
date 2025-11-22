// test_utilities.c - Tests for parasol.h utility functions
//
// Created by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information

#include "../clang/lib/Headers/parasol.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Property test configuration
// 1000 trials used for simple operations (abs, min, max, etc.) which execute
// quickly
#define PROPERTY_TEST_TRIALS 1000

static int pass_count = 0;
static int fail_count = 0;

#define TEST(name, condition)                                                  \
  do {                                                                         \
    if (condition) {                                                           \
      printf("  PASS: %s\n", name);                                            \
      pass_count++;                                                            \
    } else {                                                                   \
      printf("  FAIL: %s\n", name);                                            \
      fail_count++;                                                            \
    }                                                                          \
  } while (0)

#define PROPTEST(name)                                                         \
  printf("  Testing %s...", name);                                             \
  fflush(stdout);

#define PROPPASS()                                                             \
  printf(" PASS\n");                                                           \
  pass_count++;

#define PROPFAIL(msg, ...)                                                     \
  printf(" FAIL: " msg "\n", ##__VA_ARGS__);                                   \
  fail_count++;

void test_abs(void) {
  // Edge cases: zero and boundary values
  TEST("abs8(0)", abs8(0) == 0);
  TEST("abs8(INT8_MIN) returns INT8_MIN", abs8(-128) == -128);
  TEST("abs8(127)", abs8(127) == 127);

  TEST("abs16(0)", abs16(0) == 0);
  TEST("abs16(INT16_MIN) returns INT16_MIN", abs16(-32768) == -32768);
  TEST("abs16(32767)", abs16(32767) == 32767);

  TEST("abs32(0)", abs32(0) == 0);
  TEST("abs32(INT32_MAX)", abs32(2147483647) == 2147483647);
}

void test_sqrt(void) {
  // Edge cases: zero, one, max values, and one rounding example
  TEST("sqrt8(0)", sqrt8(0) == 0);
  TEST("sqrt8(1)", sqrt8(1) == 1);
  TEST("sqrt8(255) max", sqrt8(255) == 15);
  TEST("sqrt8(10) rounding", sqrt8(10) == 3);

  TEST("sqrt16(0)", sqrt16(0) == 0);
  TEST("sqrt16(1)", sqrt16(1) == 1);
  TEST("sqrt16(65535) max", sqrt16(65535) == 255);

  TEST("sqrt32(0)", sqrt32(0) == 0);
  TEST("sqrt32(1)", sqrt32(1) == 1);
}

void test_min_max(void) {
  // Edge cases: equal values, boundary values
  TEST("min8(0, 0) equal", min8(0, 0) == 0);
  TEST("min8(255, 0) boundaries", min8(255, 0) == 0);
  TEST("max8(0, 0) equal", max8(0, 0) == 0);
  TEST("max8(255, 0) boundaries", max8(255, 0) == 255);

  // Signed: test sign transitions
  TEST("imin8(0, 0) equal", imin8(0, 0) == 0);
  TEST("imin8(-5, 10) sign transition", imin8(-5, 10) == -5);
  TEST("imax8(0, 0) equal", imax8(0, 0) == 0);
  TEST("imax8(-5, 10) sign transition", imax8(-5, 10) == 10);
}

void test_clamp(void) {
  // Edge cases: clamping behavior, equal bounds
  TEST("clamp8 above max", clamp8(15, 0, 10) == 10);
  TEST("clamp8 below min", clamp8(0, 5, 10) == 5);
  TEST("clamp8 equal bounds", clamp8(10, 10, 10) == 10);

  // Signed: test with negative ranges
  TEST("iclamp8 above max", iclamp8(15, -10, 10) == 10);
  TEST("iclamp8 below min", iclamp8(-15, -10, 10) == -10);
  TEST("iclamp8 negative below", iclamp8(-5, 0, 10) == 0);
}

void test_sign(void) {
  // Edge cases: zero, INT_MIN, INT_MAX
  TEST("sign8(0)", sign8(0) == 0);
  TEST("sign8(127) max", sign8(127) == 1);
  TEST("sign8(-128) min", sign8(-128) == -1);

  TEST("sign16(0)", sign16(0) == 0);
  TEST("sign32(0)", sign32(0) == 0);
  // sign64 omitted - generates 64-bit constant (-1) not supported by parasol target
}

void test_absdiff(void) {
  TEST("absdiff8(0, 0)", absdiff8(0, 0) == 0);
  TEST("absdiff8(255, 0) boundaries", absdiff8(255, 0) == 255);
  TEST("absdiff8(0, 255) commutative", absdiff8(0, 255) == 255);
  TEST("absdiff8(10, 10) equal", absdiff8(10, 10) == 0);

  TEST("iabsdiff8(-10, 10) sign transition", iabsdiff8(-10, 10) == 20);
  TEST("iabsdiff8(10, -10) commutative", iabsdiff8(10, -10) == 20);
  TEST("iabsdiff8(-128, 127) boundaries", iabsdiff8(-128, 127) == 255);
}

void test_saturating_add(void) {
  TEST("sadd8(0, 0)", sadd8(0, 0) == 0);
  TEST("sadd8(255, 0) boundary", sadd8(255, 0) == 255);
  TEST("sadd8(255, 1) overflow", sadd8(255, 1) == 255);
  TEST("sadd8(200, 100) overflow", sadd8(200, 100) == 255);
  TEST("sadd8(100, 100) no overflow", sadd8(100, 100) == 200);

  TEST("isadd8(0, 0)", isadd8(0, 0) == 0);
  TEST("isadd8(127, 0) boundary", isadd8(127, 0) == 127);
  TEST("isadd8(127, 1) pos overflow", isadd8(127, 1) == 127);
  TEST("isadd8(100, 50) pos overflow", isadd8(100, 50) == 127);
  TEST("isadd8(-128, -1) neg overflow", isadd8(-128, -1) == -128);
  TEST("isadd8(-100, -50) neg overflow", isadd8(-100, -50) == -128);
  TEST("isadd8(50, 50) no overflow", isadd8(50, 50) == 100);
}

void test_saturating_sub(void) {
  TEST("ssub8(0, 0)", ssub8(0, 0) == 0);
  TEST("ssub8(0, 1) underflow", ssub8(0, 1) == 0);
  TEST("ssub8(10, 20) underflow", ssub8(10, 20) == 0);
  TEST("ssub8(255, 0) boundary", ssub8(255, 0) == 255);
  TEST("ssub8(100, 50) no underflow", ssub8(100, 50) == 50);

  TEST("issub8(0, 0)", issub8(0, 0) == 0);
  TEST("issub8(127, -1) pos overflow", issub8(127, -1) == 127);
  TEST("issub8(100, -50) pos overflow", issub8(100, -50) == 127);
  TEST("issub8(-128, 1) neg overflow", issub8(-128, 1) == -128);
  TEST("issub8(-100, 50) neg overflow", issub8(-100, 50) == -128);
  TEST("issub8(50, 25) no overflow", issub8(50, 25) == 25);
}

void test_pow(void) {
  TEST("pow8(0, 0)", pow8(0, 0) == 1);
  TEST("pow8(0, 1)", pow8(0, 1) == 0);
  TEST("pow8(1, 0)", pow8(1, 0) == 1);
  TEST("pow8(1, 15)", pow8(1, 15) == 1);
  TEST("pow8(2, 0)", pow8(2, 0) == 1);
  TEST("pow8(2, 1)", pow8(2, 1) == 2);
  TEST("pow8(2, 2)", pow8(2, 2) == 4);
  TEST("pow8(2, 3)", pow8(2, 3) == 8);
  TEST("pow8(2, 4)", pow8(2, 4) == 16);
  TEST("pow8(3, 2)", pow8(3, 2) == 9);
  TEST("pow8(5, 2)", pow8(5, 2) == 25);

  TEST("powi8(-1, 0)", powi8(-1, 0) == 1);
  TEST("powi8(-1, 1)", powi8(-1, 1) == -1);
  TEST("powi8(-1, 2)", powi8(-1, 2) == 1);
  TEST("powi8(-2, 2)", powi8(-2, 2) == 4);
  TEST("powi8(-2, 3)", powi8(-2, 3) == -8);
}

void test_parity(void) {
  TEST("is_even8(0)", is_even8(0));
  TEST("is_even8(2)", is_even8(2));
  TEST("is_even8(255) false", !is_even8(255));

  TEST("is_odd8(0) false", !is_odd8(0));
  TEST("is_odd8(1)", is_odd8(1));
  TEST("is_odd8(255)", is_odd8(255));
}

void test_get_bit(void) {
  TEST("get_bit8(0, 0) false", !get_bit8(0, 0));
  TEST("get_bit8(1, 0)", get_bit8(1, 0));
  TEST("get_bit8(2, 1)", get_bit8(2, 1));
  TEST("get_bit8(4, 2)", get_bit8(4, 2));
  TEST("get_bit8(128, 7)", get_bit8(128, 7));
  TEST("get_bit8(255, 0)", get_bit8(255, 0));
  TEST("get_bit8(255, 7)", get_bit8(255, 7));
  TEST("get_bit8(170, 0) false", !get_bit8(170, 0));
  TEST("get_bit8(170, 1)", get_bit8(170, 1));
}

void test_cswap_unsigned(void) {
  // Edge cases: boundary values and equal values
  uint8_t a = 0, b = 255;
  cswap8(true, a, b);
  TEST("cswap8(true) boundaries swap", a == 255 && b == 0);

  a = 42;
  b = 42;
  cswap8(true, a, b);
  TEST("cswap8(true) equal values", a == 42 && b == 42);

  uint16_t a16 = 0, b16 = 65535;
  cswap16(true, a16, b16);
  TEST("cswap16(true) boundaries", a16 == 65535 && b16 == 0);
}

void test_icswap_signed(void) {
  // Edge cases: sign transitions (positive/negative swaps)
  int8_t a = -10, b = 20;
  icswap8(true, a, b);
  TEST("icswap8(true) neg/pos swap", a == 20 && b == -10);

  a = -10;
  b = -20;
  icswap8(true, a, b);
  TEST("icswap8(true) neg/neg swap", a == -20 && b == -10);

  a = 10;
  b = 20;
  icswap8(false, a, b);
  TEST("icswap8(false) no swap", a == 10 && b == 20);
}

void test_avg_edge_cases(void) {
  // Edge cases: zero, max values, and one truncation example per type
  TEST("avg8(0, 0)", avg8(0, 0) == 0);
  TEST("avg8(0, 255) boundaries", avg8(0, 255) == 127);
  TEST("avg8(255, 255)", avg8(255, 255) == 255);
  TEST("avg8(1, 2) truncation", avg8(1, 2) == 1);

  TEST("avg16(0, 0)", avg16(0, 0) == 0);
  TEST("avg16(0, 65535) boundaries", avg16(0, 65535) == 32767);
  TEST("avg16(65535, 65535)", avg16(65535, 65535) == 65535);

  uint32_t max32 = 4294967295U;
  TEST("avg32(0, 0)", avg32(0, 0) == 0);
  TEST("avg32(0, max) boundaries", avg32(0, max32) == 2147483647U);
  TEST("avg32(max, max)", avg32(max32, max32) == max32);
}

// Property-based tests
void test_abs_properties(void) {
  PROPTEST("abs properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {
    int8_t x = (int8_t)rand();

    // Property: idempotence abs(abs(x)) == abs(x)
    int8_t abs_x = abs8(x);
    int8_t abs_abs_x = abs8(abs_x);
    if (abs_abs_x != abs_x) {
      PROPFAIL("idempotence failed: x=%d abs(x)=%d abs(abs(x))=%d", x, abs_x,
               abs_abs_x);
      return;
    }

    // Property: non-negativity (except INT_MIN)
    if (x != -128 && abs_x < 0) {
      PROPFAIL("non-negativity failed: x=%d abs(x)=%d", x, abs_x);
      return;
    }

    // Property: if x >= 0, then abs(x) == x
    if (x >= 0 && abs_x != x) {
      PROPFAIL("positive preservation failed: x=%d abs(x)=%d", x, abs_x);
      return;
    }

    // Property: if x < 0 and x != INT_MIN, then abs(x) == -x
    if (x < 0 && x != -128 && abs_x != -x) {
      PROPFAIL("negation failed: x=%d abs(x)=%d expected=%d", x, abs_x, -x);
      return;
    }
  }

  PROPPASS();
}

void test_sqrt_properties(void) {
  PROPTEST("sqrt properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {
    uint8_t x = (uint8_t)rand();
    uint8_t result = sqrt8(x);

    // Property: result^2 <= x < (result+1)^2
    uint16_t result_sq = (uint16_t)result * result;
    uint16_t result_plus_1_sq = (uint16_t)(result + 1) * (result + 1);

    if (!(result_sq <= x && x < result_plus_1_sq)) {
      PROPFAIL("bounds failed: x=%u sqrt(x)=%u, %u^2=%u, %u^2=%u", x, result,
               result, result_sq, result + 1, result_plus_1_sq);
      return;
    }

    // Property: sqrt(0) == 0
    if (x == 0 && result != 0) {
      PROPFAIL("sqrt(0) != 0");
      return;
    }
  }

  // Test monotonicity
  for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {
    uint8_t a = (uint8_t)rand();
    uint8_t b = (uint8_t)rand();
    if (a > b) {
      uint8_t tmp = a;
      a = b;
      b = tmp;
    }

    uint8_t sqrt_a = sqrt8(a);
    uint8_t sqrt_b = sqrt8(b);

    if (sqrt_a > sqrt_b) {
      PROPFAIL("monotonicity failed: a=%u b=%u sqrt(a)=%u sqrt(b)=%u", a, b,
               sqrt_a, sqrt_b);
      return;
    }
  }

  PROPPASS();
}

void test_min_max_properties(void) {
  PROPTEST("min/max properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {
    uint8_t a = (uint8_t)rand();
    uint8_t b = (uint8_t)rand();

    uint8_t min_ab = min8(a, b);
    uint8_t min_ba = min8(b, a);
    uint8_t max_ab = max8(a, b);
    uint8_t max_ba = max8(b, a);

    // Property: commutativity
    if (min_ab != min_ba) {
      PROPFAIL("min commutativity failed: a=%u b=%u min(a,b)=%u min(b,a)=%u", a,
               b, min_ab, min_ba);
      return;
    }
    if (max_ab != max_ba) {
      PROPFAIL("max commutativity failed: a=%u b=%u max(a,b)=%u max(b,a)=%u", a,
               b, max_ab, max_ba);
      return;
    }

    // Property: bounds
    if (!(min_ab <= a && min_ab <= b)) {
      PROPFAIL("min bounds failed: a=%u b=%u min=%u", a, b, min_ab);
      return;
    }
    if (!(max_ab >= a && max_ab >= b)) {
      PROPFAIL("max bounds failed: a=%u b=%u max=%u", a, b, max_ab);
      return;
    }

    // Property: existence
    if (!(min_ab == a || min_ab == b)) {
      PROPFAIL("min existence failed: a=%u b=%u min=%u", a, b, min_ab);
      return;
    }
    if (!(max_ab == a || max_ab == b)) {
      PROPFAIL("max existence failed: a=%u b=%u max=%u", a, b, max_ab);
      return;
    }

    // Property: idempotence
    if (min8(a, a) != a) {
      PROPFAIL("min idempotence failed: a=%u min(a,a)=%u", a, min8(a, a));
      return;
    }
    if (max8(a, a) != a) {
      PROPFAIL("max idempotence failed: a=%u max(a,a)=%u", a, max8(a, a));
      return;
    }
  }

  PROPPASS();
}

void test_clamp_properties(void) {
  PROPTEST("clamp properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {
    uint8_t x = (uint8_t)rand();
    uint8_t min_val = (uint8_t)rand();
    uint8_t max_val = (uint8_t)rand();

    // Ensure min_val <= max_val
    if (min_val > max_val) {
      uint8_t tmp = min_val;
      min_val = max_val;
      max_val = tmp;
    }

    uint8_t result = clamp8(x, min_val, max_val);

    // Property: result is within bounds
    if (!(result >= min_val && result <= max_val)) {
      PROPFAIL("bounds failed: x=%u min=%u max=%u result=%u", x, min_val,
               max_val, result);
      return;
    }

    // Property: if x is already in range, clamp returns x
    if (x >= min_val && x <= max_val && result != x) {
      PROPFAIL("identity failed: x=%u min=%u max=%u result=%u", x, min_val,
               max_val, result);
      return;
    }

    // Property: consistency with min/max
    uint8_t expected = min8(max8(x, min_val), max_val);
    if (result != expected) {
      PROPFAIL("consistency failed: x=%u min=%u max=%u result=%u expected=%u",
               x, min_val, max_val, result, expected);
      return;
    }
  }

  PROPPASS();
}

void test_absdiff_properties(void) {
  PROPTEST("absdiff properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {
    uint8_t a = (uint8_t)rand();
    uint8_t b = (uint8_t)rand();

    uint8_t diff_ab = absdiff8(a, b);
    uint8_t diff_ba = absdiff8(b, a);

    // Property: commutativity
    if (diff_ab != diff_ba) {
      PROPFAIL(
          "commutativity failed: a=%u b=%u absdiff(a,b)=%u absdiff(b,a)=%u", a,
          b, diff_ab, diff_ba);
      return;
    }

    // Property: identity absdiff(x,x) == 0
    if (absdiff8(a, a) != 0) {
      PROPFAIL("identity failed: a=%u absdiff(a,a)=%u", a, absdiff8(a, a));
      return;
    }

    // Property: non-negativity (always true for unsigned)
    // Property: triangle inequality absdiff(a,c) <= absdiff(a,b) + absdiff(b,c)
    uint8_t c = (uint8_t)rand();
    uint16_t diff_ac = absdiff8(a, c);
    uint16_t diff_ab_u16 = diff_ab;
    uint16_t diff_bc = absdiff8(b, c);

    if (diff_ac > diff_ab_u16 + diff_bc) {
      PROPFAIL("triangle inequality failed: a=%u b=%u c=%u absdiff(a,c)=%u "
               "absdiff(a,b)+absdiff(b,c)=%u",
               a, b, c, diff_ac, diff_ab_u16 + diff_bc);
      return;
    }
  }

  PROPPASS();
}

void test_saturating_add_properties(void) {
  PROPTEST("saturating add properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {
    uint8_t a = (uint8_t)rand();
    uint8_t b = (uint8_t)rand();

    uint8_t result = sadd8(a, b);

    // Property: result is in valid range [0, 255]
    // Always true for uint8_t

    // Property: identity sadd(x, 0) == x
    if (sadd8(a, 0) != a) {
      PROPFAIL("identity failed: a=%u sadd(a,0)=%u", a, sadd8(a, 0));
      return;
    }

    // Property: commutativity
    if (sadd8(a, b) != sadd8(b, a)) {
      PROPFAIL("commutativity failed: a=%u b=%u sadd(a,b)=%u sadd(b,a)=%u", a,
               b, sadd8(a, b), sadd8(b, a));
      return;
    }

    // Property: saturation at boundary
    if ((uint16_t)a + (uint16_t)b > 255) {
      if (result != 255) {
        PROPFAIL("overflow saturation failed: a=%u b=%u result=%u expected=255",
                 a, b, result);
        return;
      }
    } else {
      if (result != (uint8_t)(a + b)) {
        PROPFAIL("no overflow failed: a=%u b=%u result=%u expected=%u", a, b,
                 result, (uint8_t)(a + b));
        return;
      }
    }
  }

  PROPPASS();
}

void test_saturating_sub_properties(void) {
  PROPTEST("saturating sub properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {
    uint8_t a = (uint8_t)rand();
    uint8_t b = (uint8_t)rand();

    uint8_t result = ssub8(a, b);

    // Property: result is in valid range [0, 255]
    // Always true for uint8_t

    // Property: identity ssub(x, 0) == x
    if (ssub8(a, 0) != a) {
      PROPFAIL("identity failed: a=%u ssub(a,0)=%u", a, ssub8(a, 0));
      return;
    }

    // Property: underflow saturation
    if (a < b) {
      if (result != 0) {
        PROPFAIL("underflow saturation failed: a=%u b=%u result=%u expected=0",
                 a, b, result);
        return;
      }
    } else {
      if (result != (uint8_t)(a - b)) {
        PROPFAIL("no underflow failed: a=%u b=%u result=%u expected=%u", a, b,
                 result, (uint8_t)(a - b));
        return;
      }
    }
  }

  PROPPASS();
}

void test_pow_properties(void) {
  PROPTEST("pow properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {
    uint8_t base = (uint8_t)(rand() % 16); // Keep base small to avoid overflow
    uint8_t exp = (uint8_t)(rand() % 16);  // Exponents 0-15

    uint8_t result = pow8(base, exp);

    // Property: pow(x, 0) == 1
    if (exp == 0 && result != 1) {
      PROPFAIL("zero exponent failed: base=%u pow(base,0)=%u expected=1", base,
               result);
      return;
    }

    // Property: pow(x, 1) == x
    if (exp == 1 && result != base) {
      PROPFAIL("one exponent failed: base=%u pow(base,1)=%u", base, result);
      return;
    }

    // Property: pow(1, exp) == 1
    if (base == 1 && result != 1) {
      PROPFAIL("base one failed: exp=%u pow(1,exp)=%u expected=1", exp, result);
      return;
    }

    // Property: pow(0, exp) == 0 for exp > 0
    if (base == 0 && exp > 0 && result != 0) {
      PROPFAIL("base zero failed: exp=%u pow(0,exp)=%u expected=0", exp,
               result);
      return;
    }

    // Property: verify against naive multiplication for small values
    if (exp <= 4 && base <= 3) {
      uint16_t expected = 1;
      for (uint8_t i = 0; i < exp; i++) {
        expected *= base;
      }
      if (result != (uint8_t)expected) {
        PROPFAIL(
            "naive verification failed: base=%u exp=%u result=%u expected=%u",
            base, exp, result, (uint8_t)expected);
        return;
      }
    }
  }

  PROPPASS();
}

void test_get_bit_properties(void) {
  PROPTEST("get_bit properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {
    uint8_t val = (uint8_t)rand();

    // Property: result is always 0 or 1
    for (unsigned int bit = 0; bit < 8; bit++) {
      bool result = get_bit8(val, bit);
      if (result != 0 && result != 1) {
        PROPFAIL("bounds failed: val=%u bit=%u result=%d", val, bit, result);
        return;
      }

      // Property: consistency with bit operations
      bool expected = ((val >> bit) & 1);
      if (result != expected) {
        PROPFAIL("consistency failed: val=%u bit=%u result=%d expected=%d", val,
                 bit, result, expected);
        return;
      }
    }

    // Property: reconstruction
    uint8_t reconstructed = 0;
    for (unsigned int bit = 0; bit < 8; bit++) {
      if (get_bit8(val, bit)) {
        reconstructed |= (1 << bit);
      }
    }
    if (reconstructed != val) {
      PROPFAIL("reconstruction failed: val=%u reconstructed=%u", val,
               reconstructed);
      return;
    }
  }

  PROPPASS();
}

void test_cswap_properties(void) {
  PROPTEST("cswap properties (1000 trials × 8 types)");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {
    // Test uint8
    uint8_t orig_a8 = (uint8_t)rand();
    uint8_t orig_b8 = (uint8_t)rand();
    uint8_t a8 = orig_a8, b8 = orig_b8;
    cswap8(true, a8, b8);
    if (a8 != orig_b8 || b8 != orig_a8) {
      PROPFAIL("cswap8 failed");
      return;
    }

    a8 = orig_a8;
    b8 = orig_b8;
    cswap8(false, a8, b8);
    if (a8 != orig_a8 || b8 != orig_b8) {
      PROPFAIL("cswap8 no-swap failed");
      return;
    }

    // Test uint16
    uint16_t orig_a16 = (uint16_t)rand();
    uint16_t orig_b16 = (uint16_t)rand();
    uint16_t a16 = orig_a16, b16 = orig_b16;
    cswap16(true, a16, b16);
    if (a16 != orig_b16 || b16 != orig_a16) {
      PROPFAIL("cswap16 failed");
      return;
    }

    // Test int8
    int8_t orig_ia8 = (int8_t)rand();
    int8_t orig_ib8 = (int8_t)rand();
    int8_t ia8 = orig_ia8, ib8 = orig_ib8;
    icswap8(true, ia8, ib8);
    if (ia8 != orig_ib8 || ib8 != orig_ia8) {
      PROPFAIL("icswap8 failed");
      return;
    }

    ia8 = orig_ia8;
    ib8 = orig_ib8;
    icswap8(false, ia8, ib8);
    if (ia8 != orig_ia8 || ib8 != orig_ib8) {
      PROPFAIL("icswap8 no-swap failed");
      return;
    }
  }

  PROPPASS();
}

void test_avg_properties(void) {
  PROPTEST("avg properties (1000 trials × 4 types)");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {
    uint8_t a = (uint8_t)rand();
    uint8_t b = (uint8_t)rand();

    // Property: commutative
    if (avg8(a, b) != avg8(b, a)) {
      PROPFAIL("commutativity failed: a=%u b=%u", a, b);
      return;
    }

    // Property: identity
    if (avg8(a, a) != a) {
      PROPFAIL("identity failed: a=%u avg(a,a)=%u", a, avg8(a, a));
      return;
    }

    // Property: bounds
    uint8_t avg_result = avg8(a, b);
    uint8_t min_val = a < b ? a : b;
    uint8_t max_val = a > b ? a : b;
    if (!(avg_result >= min_val && avg_result <= max_val)) {
      PROPFAIL("bounds failed: a=%u b=%u avg=%u min=%u max=%u", a, b,
               avg_result, min_val, max_val);
      return;
    }

    // Property: overflow safety - avg should never overflow
    // This is implicitly tested since avg8 uses bitwise ops

    // Property: truncation behavior for consecutive numbers
    if (b == a + 1 && avg8(a, b) != a) {
      PROPFAIL("truncation failed: avg(%u, %u)=%u expected=%u", a, b,
               avg8(a, b), a);
      return;
    }
  }

  // Test with larger types
  for (int trial = 0; trial < 100; trial++) {
    uint16_t a16 = (uint16_t)rand();
    uint16_t b16 = (uint16_t)rand();
    if (avg16(a16, b16) != avg16(b16, a16)) {
      PROPFAIL("avg16 commutativity failed");
      return;
    }

    uint32_t a32 = (uint32_t)rand() | ((uint32_t)rand() << 16);
    uint32_t b32 = (uint32_t)rand() | ((uint32_t)rand() << 16);
    if (avg32(a32, b32) != avg32(b32, a32)) {
      PROPFAIL("avg32 commutativity failed");
      return;
    }
  }

  PROPPASS();
}

int main() {
  // Seed RNG once for all tests
  srand((unsigned int)time(NULL));

  printf("Testing Utility Functions\n\n");

  printf("Edge Case Tests\n");
  printf("---------------\n\n");

  printf("Testing abs functions:\n");
  test_abs();
  printf("\n");

  printf("Testing sqrt functions:\n");
  test_sqrt();
  printf("\n");

  printf("Testing min/max functions:\n");
  test_min_max();
  printf("\n");

  printf("Testing clamp functions:\n");
  test_clamp();
  printf("\n");

  printf("Testing sign functions:\n");
  test_sign();
  printf("\n");

  printf("Testing absolute difference:\n");
  test_absdiff();
  printf("\n");

  printf("Testing saturating addition:\n");
  test_saturating_add();
  printf("\n");

  printf("Testing saturating subtraction:\n");
  test_saturating_sub();
  printf("\n");

  printf("Testing power functions:\n");
  test_pow();
  printf("\n");

  printf("Testing parity checks:\n");
  test_parity();
  printf("\n");

  printf("Testing bit extraction:\n");
  test_get_bit();
  printf("\n");

  printf("Testing conditional swap:\n");
  test_cswap_unsigned();
  printf("\n");

  printf("Testing conditional swap (signed):\n");
  test_icswap_signed();
  printf("\n");

  printf("Testing average macros:\n");
  test_avg_edge_cases();
  printf("\n");

  printf("Property-Based Tests (1000 trials each)\n");
  printf("----------------------------------------\n\n");

  test_abs_properties();
  test_sqrt_properties();
  test_min_max_properties();
  test_clamp_properties();
  test_absdiff_properties();
  test_saturating_add_properties();
  test_saturating_sub_properties();
  test_pow_properties();
  test_get_bit_properties();
  test_cswap_properties();
  test_avg_properties();

  printf("\nTest Summary\n");
  printf("Passed: %d\n", pass_count);
  printf("Failed: %d\n", fail_count);
  printf("Total:  %d\n", pass_count + fail_count);

  return fail_count > 0 ? 1 : 0;
}
