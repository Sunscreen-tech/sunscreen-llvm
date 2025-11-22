// test_array_reductions.c - Tests for parasol.h array reduction functions
//
// Created by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information

#include "../clang/lib/Headers/parasol.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Property test configuration
// 100 trials used for array reduction operations (sum, min, max) which are more
// expensive than simple operations but still relatively fast compared to
// sorting
#define PROPERTY_TEST_TRIALS 100

static int pass_count = 0;
static int fail_count = 0;

#define TEST(name)                                                             \
  printf("  Testing %s...", name);                                             \
  fflush(stdout);

#define PASS()                                                                 \
  printf(" PASS\n");                                                           \
  pass_count++;

#define FAIL(msg, ...)                                                         \
  printf(" FAIL: " msg "\n", ##__VA_ARGS__);                                   \
  fail_count++;

// Macro-based property test generation for array reductions
// Eliminates duplication across uint16/32/64 and int16/32/64 property tests
#define DEFINE_UNSIGNED_REDUCTION_PROPERTY_TEST(BITS, MAX_LEN, RAND_EXPR)      \
  static void test_uint##BITS##_properties(void) {                             \
    TEST("uint" #BITS " min/max/sum properties");                          \
                                                                               \
    uint##BITS##_t *arr = malloc(MAX_LEN * sizeof(uint##BITS##_t));            \
    uint##BITS##_t *scratch_minmax = malloc(MAX_LEN * sizeof(uint##BITS##_t)); \
    uint64_t *scratch_sum = malloc(MAX_LEN * sizeof(uint64_t));                \
                                                                               \
    for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {               \
      uint16_t len = 1 + (rand() % MAX_LEN);                                   \
                                                                               \
      for (uint16_t i = 0; i < len; i++) {                                     \
        arr[i] = (RAND_EXPR);                                                  \
      }                                                                        \
                                                                               \
      uint##BITS##_t min_result = min##BITS##_array(arr, len, scratch_minmax); \
      uint##BITS##_t max_result = max##BITS##_array(arr, len, scratch_minmax); \
      uint64_t sum_result = sum##BITS##_array(arr, len, scratch_sum);          \
                                                                               \
      bool min_found = false;                                                  \
      bool max_found = false;                                                  \
      uint64_t sum_check = 0;                                                  \
                                                                               \
      for (uint16_t i = 0; i < len; i++) {                                     \
        if (arr[i] < min_result) {                                             \
          FAIL("min property violated at trial %d: arr[%u]=" #BITS             \
               "-bit value < min=" #BITS "-bit value",                         \
               trial, i);                                                      \
          goto cleanup_uint##BITS;                                             \
        }                                                                      \
        if (arr[i] > max_result) {                                             \
          FAIL("max property violated at trial %d: arr[%u]=" #BITS             \
               "-bit value > max=" #BITS "-bit value",                         \
               trial, i);                                                      \
          goto cleanup_uint##BITS;                                             \
        }                                                                      \
        if (arr[i] == min_result)                                              \
          min_found = true;                                                    \
        if (arr[i] == max_result)                                              \
          max_found = true;                                                    \
        sum_check += arr[i];                                                   \
      }                                                                        \
                                                                               \
      if (!min_found) {                                                        \
        FAIL("min not found in array at trial %d", trial);                     \
        goto cleanup_uint##BITS;                                               \
      }                                                                        \
      if (!max_found) {                                                        \
        FAIL("max not found in array at trial %d", trial);                     \
        goto cleanup_uint##BITS;                                               \
      }                                                                        \
      if (sum_check != sum_result) {                                           \
        FAIL("sum mismatch at trial %d: expected=%llu got=%llu", trial,        \
             (unsigned long long)sum_check, (unsigned long long)sum_result);   \
        goto cleanup_uint##BITS;                                               \
      }                                                                        \
    }                                                                          \
                                                                               \
    PASS();                                                                    \
                                                                               \
    cleanup_uint##BITS : free(arr);                                            \
    free(scratch_minmax);                                                      \
    free(scratch_sum);                                                         \
  }

#define DEFINE_SIGNED_REDUCTION_PROPERTY_TEST(BITS, MAX_LEN, RAND_EXPR)        \
  static void test_int##BITS##_properties(void) {                              \
    TEST("int" #BITS " min/max/sum properties");                           \
                                                                               \
    int##BITS##_t *arr = malloc(MAX_LEN * sizeof(int##BITS##_t));              \
    int##BITS##_t *scratch_minmax = malloc(MAX_LEN * sizeof(int##BITS##_t));   \
    int64_t *scratch_sum = malloc(MAX_LEN * sizeof(int64_t));                  \
                                                                               \
    for (int trial = 0; trial < PROPERTY_TEST_TRIALS; trial++) {               \
      uint16_t len = 1 + (rand() % MAX_LEN);                                   \
                                                                               \
      for (uint16_t i = 0; i < len; i++) {                                     \
        arr[i] = (RAND_EXPR);                                                  \
      }                                                                        \
                                                                               \
      int##BITS##_t min_result = imin##BITS##_array(arr, len, scratch_minmax); \
      int##BITS##_t max_result = imax##BITS##_array(arr, len, scratch_minmax); \
      int64_t sum_result = isum##BITS##_array(arr, len, scratch_sum);          \
                                                                               \
      bool min_found = false;                                                  \
      bool max_found = false;                                                  \
      int64_t sum_check = 0;                                                   \
                                                                               \
      for (uint16_t i = 0; i < len; i++) {                                     \
        if (arr[i] < min_result) {                                             \
          FAIL("min property violated at trial %d", trial);                    \
          goto cleanup_int##BITS;                                              \
        }                                                                      \
        if (arr[i] > max_result) {                                             \
          FAIL("max property violated at trial %d", trial);                    \
          goto cleanup_int##BITS;                                              \
        }                                                                      \
        if (arr[i] == min_result)                                              \
          min_found = true;                                                    \
        if (arr[i] == max_result)                                              \
          max_found = true;                                                    \
        sum_check += arr[i];                                                   \
      }                                                                        \
                                                                               \
      if (!min_found) {                                                        \
        FAIL("min not found in array at trial %d", trial);                     \
        goto cleanup_int##BITS;                                                \
      }                                                                        \
      if (!max_found) {                                                        \
        FAIL("max not found in array at trial %d", trial);                     \
        goto cleanup_int##BITS;                                                \
      }                                                                        \
      if (sum_check != sum_result) {                                           \
        FAIL("sum mismatch at trial %d: expected=%lld got=%lld", trial,        \
             (long long)sum_check, (long long)sum_result);                     \
        goto cleanup_int##BITS;                                                \
      }                                                                        \
    }                                                                          \
                                                                               \
    PASS();                                                                    \
                                                                               \
    cleanup_int##BITS : free(arr);                                             \
    free(scratch_minmax);                                                      \
    free(scratch_sum);                                                         \
  }

// Reference implementations for comparison
static uint8_t min_naive_uint8(const uint8_t *arr, uint16_t len) {
  uint8_t result = arr[0];
  for (uint16_t i = 1; i < len; i++) {
    if (arr[i] < result)
      result = arr[i];
  }
  return result;
}

static uint8_t max_naive_uint8(const uint8_t *arr, uint16_t len) {
  uint8_t result = arr[0];
  for (uint16_t i = 1; i < len; i++) {
    if (arr[i] > result)
      result = arr[i];
  }
  return result;
}

static uint64_t sum_naive_uint8(const uint8_t *arr, uint16_t len) {
  uint64_t result = 0;
  for (uint16_t i = 0; i < len; i++) {
    result += arr[i];
  }
  return result;
}

static int8_t min_naive_int8(const int8_t *arr, uint16_t len) {
  int8_t result = arr[0];
  for (uint16_t i = 1; i < len; i++) {
    if (arr[i] < result)
      result = arr[i];
  }
  return result;
}

static int8_t max_naive_int8(const int8_t *arr, uint16_t len) {
  int8_t result = arr[0];
  for (uint16_t i = 1; i < len; i++) {
    if (arr[i] > result)
      result = arr[i];
  }
  return result;
}

static int64_t sum_naive_int8(const int8_t *arr, uint16_t len) {
  int64_t result = 0;
  for (uint16_t i = 0; i < len; i++) {
    result += arr[i];
  }
  return result;
}

// Test uint8 min/max exhaustively for small arrays
static void test_uint8_min_max_small(void) {
  TEST("uint8 min/max small arrays");

  uint8_t scratch[8];

  // Single element
  for (uint16_t val = 0; val <= 255; val++) {
    uint8_t arr[1] = {val};
    uint8_t min_result = min8_array(arr, 1, scratch);
    uint8_t max_result = max8_array(arr, 1, scratch);

    if (min_result != val || max_result != val) {
      FAIL("single element val=%u min=%u max=%u", val, min_result, max_result);
      return;
    }
  }

  // Two elements - test edge cases
  uint8_t test_pairs[][2] = {{0, 0}, {0, 255}, {255, 0}, {127, 128}, {1, 2}};
  for (size_t i = 0; i < sizeof(test_pairs) / sizeof(test_pairs[0]); i++) {
    uint8_t arr[2] = {test_pairs[i][0], test_pairs[i][1]};
    uint8_t expected_min = arr[0] < arr[1] ? arr[0] : arr[1];
    uint8_t expected_max = arr[0] > arr[1] ? arr[0] : arr[1];

    uint8_t min_result = min8_array(arr, 2, scratch);
    uint8_t max_result = max8_array(arr, 2, scratch);

    if (min_result != expected_min || max_result != expected_max) {
      FAIL("pair [%u,%u] expected min=%u max=%u got min=%u max=%u", arr[0],
           arr[1], expected_min, expected_max, min_result, max_result);
      return;
    }
  }

  // Array of all same values
  uint8_t arr_same[8] = {42, 42, 42, 42, 42, 42, 42, 42};
  if (min8_array(arr_same, 8, scratch) != 42 ||
      max8_array(arr_same, 8, scratch) != 42) {
    FAIL("all same values");
    return;
  }

  // Array with min at different positions
  uint8_t arr_min_first[4] = {1, 100, 200, 255};
  uint8_t arr_min_last[4] = {255, 200, 100, 1};
  uint8_t arr_min_middle[4] = {100, 1, 200, 255};

  if (min8_array(arr_min_first, 4, scratch) != 1 ||
      min8_array(arr_min_last, 4, scratch) != 1 ||
      min8_array(arr_min_middle, 4, scratch) != 1) {
    FAIL("min at different positions");
    return;
  }

  PASS();
}

// Test int8 with negative values
static void test_int8_min_max(void) {
  TEST("int8 min/max with negatives");

  int8_t scratch[8];

  // Edge values
  int8_t arr_edges[] = {INT8_MIN, INT8_MAX};
  int8_t min_edges = imin8_array(arr_edges, 2, scratch);
  int8_t max_edges = imax8_array(arr_edges, 2, scratch);

  if (min_edges != INT8_MIN || max_edges != INT8_MAX) {
    FAIL("edges expected min=%d max=%d got min=%d max=%d", INT8_MIN, INT8_MAX,
         min_edges, max_edges);
    return;
  }

  // Mix of positive and negative
  int8_t arr_mixed[] = {-50, 30, -100, 75, 0};
  int8_t min_mixed = imin8_array(arr_mixed, 5, scratch);
  int8_t max_mixed = imax8_array(arr_mixed, 5, scratch);

  if (min_mixed != -100 || max_mixed != 75) {
    FAIL("mixed expected min=-100 max=75 got min=%d max=%d", min_mixed,
         max_mixed);
    return;
  }

  // All negative
  int8_t arr_neg[] = {-1, -5, -3, -10};
  if (imin8_array(arr_neg, 4, scratch) != -10 ||
      imax8_array(arr_neg, 4, scratch) != -1) {
    FAIL("all negative");
    return;
  }

  PASS();
}

// Compare against naive implementations
static void test_comparison_naive(void) {
  TEST("comparison against naive implementations");

  uint8_t arr[] = {42, 17, 99, 3, 85, 61, 28};
  uint8_t scratch[7];
  uint64_t scratch_sum[7];

  uint8_t min_result = min8_array(arr, 7, scratch);
  uint8_t max_result = max8_array(arr, 7, scratch);
  uint64_t sum_result = sum8_array(arr, 7, scratch_sum);

  uint8_t min_expected = min_naive_uint8(arr, 7);
  uint8_t max_expected = max_naive_uint8(arr, 7);
  uint64_t sum_expected = sum_naive_uint8(arr, 7);

  if (min_result != min_expected || max_result != max_expected ||
      sum_result != sum_expected) {
    FAIL("mismatch with naive: min %u!=%u max %u!=%u sum %llu!=%llu",
         min_result, min_expected, max_result, max_expected,
         (unsigned long long)sum_result, (unsigned long long)sum_expected);
    return;
  }

  // Test int8
  int8_t arr_signed[] = {-42, 17, -99, 3, 85, -61, 28};
  int8_t scratch_signed[7];
  int64_t scratch_sum_signed[7];

  int8_t min_signed = imin8_array(arr_signed, 7, scratch_signed);
  int8_t max_signed = imax8_array(arr_signed, 7, scratch_signed);
  int64_t sum_signed = isum8_array(arr_signed, 7, scratch_sum_signed);

  int8_t min_signed_expected = min_naive_int8(arr_signed, 7);
  int8_t max_signed_expected = max_naive_int8(arr_signed, 7);
  int64_t sum_signed_expected = sum_naive_int8(arr_signed, 7);

  if (min_signed != min_signed_expected || max_signed != max_signed_expected ||
      sum_signed != sum_signed_expected) {
    FAIL("signed mismatch with naive");
    return;
  }

  PASS();
}

// Test non-power-of-2 sizes
static void test_non_power_of_2(void) {
  TEST("non-power-of-2 array sizes");

  uint16_t sizes[] = {1, 3, 5, 7, 10, 15, 17, 100, 1000};

  for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
    uint16_t len = sizes[s];
    uint8_t *arr = malloc(len * sizeof(uint8_t));
    uint8_t *scratch = malloc(len * sizeof(uint8_t));
    uint64_t *scratch_sum = malloc(len * sizeof(uint64_t));

    // Sequential values
    for (uint16_t i = 0; i < len; i++) {
      arr[i] = (uint8_t)(i % 256);
    }

    uint8_t min_result = min8_array(arr, len, scratch);
    uint8_t max_result = max8_array(arr, len, scratch);
    uint64_t sum_result = sum8_array(arr, len, scratch_sum);

    uint8_t min_expected = min_naive_uint8(arr, len);
    uint8_t max_expected = max_naive_uint8(arr, len);
    uint64_t sum_expected = sum_naive_uint8(arr, len);

    if (min_result != min_expected || max_result != max_expected ||
        sum_result != sum_expected) {
      FAIL("size %u failed", len);
      free(arr);
      free(scratch);
      free(scratch_sum);
      return;
    }

    free(arr);
    free(scratch);
    free(scratch_sum);
  }

  PASS();
}

int main() {
  // Seed RNG once for all tests
  srand((unsigned int)time(NULL));

  printf("Array Reduction Tests\n\n");

  test_uint8_min_max_small();
  test_int8_min_max();
  test_comparison_naive();
  test_non_power_of_2();

  printf("\nResults: %d passed, %d failed\n", pass_count, fail_count);

  return fail_count > 0 ? 1 : 0;
}
