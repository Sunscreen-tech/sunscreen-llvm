// test_array_sort.c - Tests for parasol.h bitonic sort functions
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
// 100 trials for fast sorts (uint8, int8) which execute quickly
// 50 trials for slower sorts (larger types like uint32, uint64, int64) where
// bitonic sort has more overhead due to larger element size and more
// comparison/swap operations
#define PROPERTY_TEST_TRIALS_FAST 100
#define PROPERTY_TEST_TRIALS_SLOW 50

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

// Helper: check if array is sorted ascending
static bool is_sorted_asc_uint8(const uint8_t *arr, uint16_t len) {
  for (uint16_t i = 0; i < len - 1; i++) {
    if (arr[i] > arr[i + 1])
      return false;
  }
  return true;
}

// Helper: check if array is sorted descending
static bool is_sorted_desc_uint8(const uint8_t *arr, uint16_t len) {
  for (uint16_t i = 0; i < len - 1; i++) {
    if (arr[i] < arr[i + 1])
      return false;
  }
  return true;
}

// Helper: check if array is sorted ascending (signed)
static bool is_sorted_asc_int8(const int8_t *arr, uint16_t len) {
  for (uint16_t i = 0; i < len - 1; i++) {
    if (arr[i] > arr[i + 1])
      return false;
  }
  return true;
}

// Helper: check if array is sorted descending (signed)
static bool is_sorted_desc_int8(const int8_t *arr, uint16_t len) {
  for (uint16_t i = 0; i < len - 1; i++) {
    if (arr[i] < arr[i + 1])
      return false;
  }
  return true;
}

// Helper: check if two arrays are permutations of each other
static bool is_permutation_uint8(const uint8_t *a, const uint8_t *b,
                                 uint16_t len) {
  uint8_t counts[256] = {0};

  for (uint16_t i = 0; i < len; i++) {
    counts[a[i]]++;
  }

  for (uint16_t i = 0; i < len; i++) {
    if (counts[b[i]] == 0)
      return false;
    counts[b[i]]--;
  }

  return true;
}

// Helper: check if two arrays are permutations of each other (signed)
static bool is_permutation_int8(const int8_t *a, const int8_t *b,
                                uint16_t len) {
  int counts[256] = {0};

  for (uint16_t i = 0; i < len; i++) {
    counts[(uint8_t)a[i]]++;
  }

  for (uint16_t i = 0; i < len; i++) {
    if (counts[(uint8_t)b[i]] == 0)
      return false;
    counts[(uint8_t)b[i]]--;
  }

  return true;
}

// Comparison functions for qsort
static int cmp_uint16(const void *a, const void *b) {
  uint16_t va = *(uint16_t *)a;
  uint16_t vb = *(uint16_t *)b;
  return (va > vb) - (va < vb);
}

static int cmp_uint32(const void *a, const void *b) {
  uint32_t va = *(uint32_t *)a;
  uint32_t vb = *(uint32_t *)b;
  return (va > vb) - (va < vb);
}

static int cmp_uint64(const void *a, const void *b) {
  uint64_t va = *(uint64_t *)a;
  uint64_t vb = *(uint64_t *)b;
  return (va > vb) - (va < vb);
}

static int cmp_int32(const void *a, const void *b) {
  int32_t va = *(int32_t *)a;
  int32_t vb = *(int32_t *)b;
  return (va > vb) - (va < vb);
}

static int cmp_int64(const void *a, const void *b) {
  int64_t va = *(int64_t *)a;
  int64_t vb = *(int64_t *)b;
  return (va > vb) - (va < vb);
}

static int cmp_int16(const void *a, const void *b) {
  int16_t va = *(int16_t *)a;
  int16_t vb = *(int16_t *)b;
  return (va > vb) - (va < vb);
}

// Generic permutation checker using sorting
static bool is_permutation_uint16(const uint16_t *a, const uint16_t *b,
                                  uint16_t len) {
  uint16_t *a_sorted = malloc(len * sizeof(uint16_t));
  uint16_t *b_sorted = malloc(len * sizeof(uint16_t));

  memcpy(a_sorted, a, len * sizeof(uint16_t));
  memcpy(b_sorted, b, len * sizeof(uint16_t));

  // Sort both arrays
  qsort(a_sorted, len, sizeof(uint16_t), cmp_uint16);
  qsort(b_sorted, len, sizeof(uint16_t), cmp_uint16);

  // Compare sorted arrays
  bool result = (memcmp(a_sorted, b_sorted, len * sizeof(uint16_t)) == 0);

  free(a_sorted);
  free(b_sorted);
  return result;
}

static bool is_permutation_uint32(const uint32_t *a, const uint32_t *b,
                                  uint16_t len) {
  uint32_t *a_sorted = malloc(len * sizeof(uint32_t));
  uint32_t *b_sorted = malloc(len * sizeof(uint32_t));

  memcpy(a_sorted, a, len * sizeof(uint32_t));
  memcpy(b_sorted, b, len * sizeof(uint32_t));

  qsort(a_sorted, len, sizeof(uint32_t), cmp_uint32);
  qsort(b_sorted, len, sizeof(uint32_t), cmp_uint32);

  bool result = (memcmp(a_sorted, b_sorted, len * sizeof(uint32_t)) == 0);

  free(a_sorted);
  free(b_sorted);
  return result;
}

static bool is_permutation_uint64(const uint64_t *a, const uint64_t *b,
                                  uint16_t len) {
  uint64_t *a_sorted = malloc(len * sizeof(uint64_t));
  uint64_t *b_sorted = malloc(len * sizeof(uint64_t));

  memcpy(a_sorted, a, len * sizeof(uint64_t));
  memcpy(b_sorted, b, len * sizeof(uint64_t));

  qsort(a_sorted, len, sizeof(uint64_t), cmp_uint64);
  qsort(b_sorted, len, sizeof(uint64_t), cmp_uint64);

  bool result = (memcmp(a_sorted, b_sorted, len * sizeof(uint64_t)) == 0);

  free(a_sorted);
  free(b_sorted);
  return result;
}

static bool is_permutation_int32(const int32_t *a, const int32_t *b,
                                 uint16_t len) {
  int32_t *a_sorted = malloc(len * sizeof(int32_t));
  int32_t *b_sorted = malloc(len * sizeof(int32_t));

  memcpy(a_sorted, a, len * sizeof(int32_t));
  memcpy(b_sorted, b, len * sizeof(int32_t));

  qsort(a_sorted, len, sizeof(int32_t), cmp_int32);
  qsort(b_sorted, len, sizeof(int32_t), cmp_int32);

  bool result = (memcmp(a_sorted, b_sorted, len * sizeof(int32_t)) == 0);

  free(a_sorted);
  free(b_sorted);
  return result;
}

static bool is_permutation_int64(const int64_t *a, const int64_t *b,
                                 uint16_t len) {
  int64_t *a_sorted = malloc(len * sizeof(int64_t));
  int64_t *b_sorted = malloc(len * sizeof(int64_t));

  memcpy(a_sorted, a, len * sizeof(int64_t));
  memcpy(b_sorted, b, len * sizeof(int64_t));

  qsort(a_sorted, len, sizeof(int64_t), cmp_int64);
  qsort(b_sorted, len, sizeof(int64_t), cmp_int64);

  bool result = (memcmp(a_sorted, b_sorted, len * sizeof(int64_t)) == 0);

  free(a_sorted);
  free(b_sorted);
  return result;
}

static bool is_permutation_int16(const int16_t *a, const int16_t *b,
                                 uint16_t len) {
  int16_t *a_sorted = malloc(len * sizeof(int16_t));
  int16_t *b_sorted = malloc(len * sizeof(int16_t));

  memcpy(a_sorted, a, len * sizeof(int16_t));
  memcpy(b_sorted, b, len * sizeof(int16_t));

  qsort(a_sorted, len, sizeof(int16_t), cmp_int16);
  qsort(b_sorted, len, sizeof(int16_t), cmp_int16);

  bool result = (memcmp(a_sorted, b_sorted, len * sizeof(int16_t)) == 0);

  free(a_sorted);
  free(b_sorted);
  return result;
}

// Test uint8 ascending sort
static void test_uint8_sort_asc(void) {
  TEST("uint8 ascending sort");

  // Already sorted
  uint8_t arr1[] = {1, 2, 3, 4};
  uint8_t original1[] = {1, 2, 3, 4};
  bitonic_sort_uint8_asc(arr1, 4);

  if (!is_sorted_asc_uint8(arr1, 4) ||
      !is_permutation_uint8(arr1, original1, 4)) {
    FAIL("already sorted case");
    return;
  }

  // Reverse sorted
  uint8_t arr2[] = {4, 3, 2, 1};
  uint8_t original2[] = {4, 3, 2, 1};
  bitonic_sort_uint8_asc(arr2, 4);

  if (!is_sorted_asc_uint8(arr2, 4) ||
      !is_permutation_uint8(arr2, original2, 4)) {
    FAIL("reverse sorted case");
    return;
  }

  // Random order
  uint8_t arr3[] = {42, 17, 99, 3, 85, 61, 28, 156};
  uint8_t original3[] = {42, 17, 99, 3, 85, 61, 28, 156};
  bitonic_sort_uint8_asc(arr3, 8);

  if (!is_sorted_asc_uint8(arr3, 8) ||
      !is_permutation_uint8(arr3, original3, 8)) {
    FAIL("random order case");
    return;
  }

  // All same value
  uint8_t arr4[] = {42, 42, 42, 42};
  uint8_t original4[] = {42, 42, 42, 42};
  bitonic_sort_uint8_asc(arr4, 4);

  if (!is_sorted_asc_uint8(arr4, 4) ||
      !is_permutation_uint8(arr4, original4, 4)) {
    FAIL("all same value case");
    return;
  }

  // Edge values
  uint8_t arr5[] = {255, 0, 128, 1, 254, 2, 127, 129};
  uint8_t original5[] = {255, 0, 128, 1, 254, 2, 127, 129};
  bitonic_sort_uint8_asc(arr5, 8);

  if (!is_sorted_asc_uint8(arr5, 8) ||
      !is_permutation_uint8(arr5, original5, 8)) {
    FAIL("edge values case");
    return;
  }

  // Size 2
  uint8_t arr6[] = {100, 50};
  uint8_t original6[] = {100, 50};
  bitonic_sort_uint8_asc(arr6, 2);

  if (!is_sorted_asc_uint8(arr6, 2) ||
      !is_permutation_uint8(arr6, original6, 2)) {
    FAIL("size 2 case");
    return;
  }

  // Size 16
  uint8_t arr7[] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  uint8_t original7[] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  bitonic_sort_uint8_asc(arr7, 16);

  if (!is_sorted_asc_uint8(arr7, 16) ||
      !is_permutation_uint8(arr7, original7, 16)) {
    FAIL("size 16 case");
    return;
  }

  PASS();
}

// Test uint8 descending sort
static void test_uint8_sort_desc(void) {
  TEST("uint8 descending sort");

  // Already sorted descending
  uint8_t arr1[] = {4, 3, 2, 1};
  uint8_t original1[] = {4, 3, 2, 1};
  bitonic_sort_uint8_desc(arr1, 4);

  if (!is_sorted_desc_uint8(arr1, 4) ||
      !is_permutation_uint8(arr1, original1, 4)) {
    FAIL("already sorted descending case");
    return;
  }

  // Sorted ascending
  uint8_t arr2[] = {1, 2, 3, 4};
  uint8_t original2[] = {1, 2, 3, 4};
  bitonic_sort_uint8_desc(arr2, 4);

  if (!is_sorted_desc_uint8(arr2, 4) ||
      !is_permutation_uint8(arr2, original2, 4)) {
    FAIL("sorted ascending case");
    return;
  }

  // Random order
  uint8_t arr3[] = {42, 17, 99, 3, 85, 61, 28, 156};
  uint8_t original3[] = {42, 17, 99, 3, 85, 61, 28, 156};
  bitonic_sort_uint8_desc(arr3, 8);

  if (!is_sorted_desc_uint8(arr3, 8) ||
      !is_permutation_uint8(arr3, original3, 8)) {
    FAIL("random order case");
    return;
  }

  PASS();
}

// Test int8 with negatives
static void test_int8_sort(void) {
  TEST("int8 sort with negatives");

  // Mix of positive and negative
  int8_t arr1[] = {-50, 30, -100, 75};
  int8_t original1[] = {-50, 30, -100, 75};
  bitonic_sort_int8_asc(arr1, 4);

  if (!is_sorted_asc_int8(arr1, 4) ||
      !is_permutation_int8(arr1, original1, 4)) {
    FAIL("mixed signs ascending");
    return;
  }

  // All negative
  int8_t arr2[] = {-1, -5, -3, -10};
  int8_t original2[] = {-1, -5, -3, -10};
  bitonic_sort_int8_asc(arr2, 4);

  if (!is_sorted_asc_int8(arr2, 4) ||
      !is_permutation_int8(arr2, original2, 4)) {
    FAIL("all negative ascending");
    return;
  }

  // Edge values
  int8_t arr3[] = {INT8_MIN, INT8_MAX, 0, -1, 1, -2, 2, -3};
  int8_t original3[] = {INT8_MIN, INT8_MAX, 0, -1, 1, -2, 2, -3};
  bitonic_sort_int8_asc(arr3, 8);

  if (!is_sorted_asc_int8(arr3, 8) ||
      !is_permutation_int8(arr3, original3, 8)) {
    FAIL("edge values ascending");
    return;
  }

  // Descending
  int8_t arr4[] = {-50, 30, -100, 75};
  int8_t original4[] = {-50, 30, -100, 75};
  bitonic_sort_int8_desc(arr4, 4);

  if (!is_sorted_desc_int8(arr4, 4) ||
      !is_permutation_int8(arr4, original4, 4)) {
    FAIL("mixed signs descending");
    return;
  }

  PASS();
}

// Property-based test for uint16
static void test_uint16_sort_properties(void) {
  TEST("uint16 sort properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS_FAST; trial++) {
    // Power of 2 sizes: 2, 4, 8, 16, 32, 64, 128, 256
    uint16_t sizes[] = {2, 4, 8, 16, 32, 64, 128, 256};
    uint16_t len = sizes[rand() % 8];

    uint16_t *arr = malloc(len * sizeof(uint16_t));
    uint16_t *original = malloc(len * sizeof(uint16_t));

    for (uint16_t i = 0; i < len; i++) {
      arr[i] = rand() & 0xFFFF;
      original[i] = arr[i];
    }

    // Test ascending
    bitonic_sort_uint16_asc(arr, len);

    // Check sorted
    for (uint16_t i = 0; i < len - 1; i++) {
      if (arr[i] > arr[i + 1]) {
        FAIL("uint16 not sorted ascending at index %u", i);
        free(arr);
        free(original);
        return;
      }
    }

    // Check permutation
    if (!is_permutation_uint16(arr, original, len)) {
      FAIL("uint16 permutation check failed - elements lost or changed");
      free(arr);
      free(original);
      return;
    }

    free(arr);
    free(original);
  }

  PASS();
}

// Property-based test for uint32
static void test_uint32_sort_properties(void) {
  TEST("uint32 sort properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS_SLOW; trial++) {
    uint16_t sizes[] = {2, 4, 8, 16, 32, 64};
    uint16_t len = sizes[rand() % 6];

    uint32_t *arr = malloc(len * sizeof(uint32_t));
    uint32_t *original = malloc(len * sizeof(uint32_t));

    for (uint16_t i = 0; i < len; i++) {
      arr[i] = ((uint32_t)rand() << 16) | (rand() & 0xFFFF);
      original[i] = arr[i];
    }

    // Test descending
    bitonic_sort_uint32_desc(arr, len);

    // Check sorted descending
    for (uint16_t i = 0; i < len - 1; i++) {
      if (arr[i] < arr[i + 1]) {
        FAIL("uint32 not sorted descending at index %u", i);
        free(arr);
        free(original);
        return;
      }
    }

    // Check permutation
    if (!is_permutation_uint32(arr, original, len)) {
      FAIL("uint32 permutation check failed - elements lost or changed");
      free(arr);
      free(original);
      return;
    }

    free(arr);
    free(original);
  }

  PASS();
}

// Property-based test for int32
static void test_int32_sort_properties(void) {
  TEST("int32 sort properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS_SLOW; trial++) {
    uint16_t sizes[] = {2, 4, 8, 16, 32};
    uint16_t len = sizes[rand() % 5];

    int32_t *arr = malloc(len * sizeof(int32_t));
    int32_t *original = malloc(len * sizeof(int32_t));

    for (uint16_t i = 0; i < len; i++) {
      arr[i] = (int32_t)(((uint32_t)rand() << 16) | (rand() & 0xFFFF));
      original[i] = arr[i];
    }

    // Test ascending
    bitonic_sort_int32_asc(arr, len);

    // Check sorted ascending
    for (uint16_t i = 0; i < len - 1; i++) {
      if (arr[i] > arr[i + 1]) {
        FAIL("int32 not sorted ascending at index %u: %d > %d", i, arr[i],
             arr[i + 1]);
        free(arr);
        free(original);
        return;
      }
    }

    // Check permutation
    if (!is_permutation_int32(arr, original, len)) {
      FAIL("int32 permutation check failed - elements lost or changed");
      free(arr);
      free(original);
      return;
    }

    free(arr);
    free(original);
  }

  PASS();
}

// Property-based test for int16
static void test_int16_sort_properties(void) {
  TEST("int16 sort properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS_FAST; trial++) {
    uint16_t sizes[] = {2, 4, 8, 16, 32, 64, 128, 256};
    uint16_t len = sizes[rand() % 8];

    int16_t *arr = malloc(len * sizeof(int16_t));
    int16_t *original = malloc(len * sizeof(int16_t));

    for (uint16_t i = 0; i < len; i++) {
      arr[i] = (int16_t)(rand() & 0xFFFF);
      original[i] = arr[i];
    }

    // Test ascending
    bitonic_sort_int16_asc(arr, len);

    // Check sorted ascending
    for (uint16_t i = 0; i < len - 1; i++) {
      if (arr[i] > arr[i + 1]) {
        FAIL("int16 not sorted ascending at index %u: %d > %d", i, arr[i],
             arr[i + 1]);
        free(arr);
        free(original);
        return;
      }
    }

    // Check permutation
    if (!is_permutation_int16(arr, original, len)) {
      FAIL("int16 permutation check failed - elements lost or changed");
      free(arr);
      free(original);
      return;
    }

    free(arr);
    free(original);
  }

  PASS();
}

// Property-based test for uint64
static void test_uint64_sort_properties(void) {
  TEST("uint64 sort properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS_FAST; trial++) {
    uint16_t sizes[] = {2, 4, 8, 16, 32};
    uint16_t len = sizes[rand() % 5];

    uint64_t *arr = malloc(len * sizeof(uint64_t));
    uint64_t *original = malloc(len * sizeof(uint64_t));

    for (uint16_t i = 0; i < len; i++) {
      arr[i] = ((uint64_t)rand() << 32) | ((uint64_t)rand() << 16) |
               (rand() & 0xFFFF);
      original[i] = arr[i];
    }

    // Test ascending
    bitonic_sort_uint64_asc(arr, len);

    // Check sorted ascending
    for (uint16_t i = 0; i < len - 1; i++) {
      if (arr[i] > arr[i + 1]) {
        FAIL("uint64 not sorted ascending at index %u", i);
        free(arr);
        free(original);
        return;
      }
    }

    // Check permutation
    if (!is_permutation_uint64(arr, original, len)) {
      FAIL("uint64 permutation check failed - elements lost or changed");
      free(arr);
      free(original);
      return;
    }

    free(arr);
    free(original);
  }

  PASS();
}

// Property-based test for int64
static void test_int64_sort_properties(void) {
  TEST("int64 sort properties");

  for (int trial = 0; trial < PROPERTY_TEST_TRIALS_FAST; trial++) {
    uint16_t sizes[] = {2, 4, 8, 16, 32};
    uint16_t len = sizes[rand() % 5];

    int64_t *arr = malloc(len * sizeof(int64_t));
    int64_t *original = malloc(len * sizeof(int64_t));

    for (uint16_t i = 0; i < len; i++) {
      arr[i] = (int64_t)(((uint64_t)rand() << 32) | ((uint64_t)rand() << 16) |
                         (rand() & 0xFFFF));
      original[i] = arr[i];
    }

    // Test descending
    bitonic_sort_int64_desc(arr, len);

    // Check sorted descending
    for (uint16_t i = 0; i < len - 1; i++) {
      if (arr[i] < arr[i + 1]) {
        FAIL("int64 not sorted descending at index %u", i);
        free(arr);
        free(original);
        return;
      }
    }

    // Check permutation
    if (!is_permutation_int64(arr, original, len)) {
      FAIL("int64 permutation check failed - elements lost or changed");
      free(arr);
      free(original);
      return;
    }

    free(arr);
    free(original);
  }

  PASS();
}

// Test duplicates
static void test_duplicates(void) {
  TEST("sort with duplicates");

  // Many duplicates
  uint8_t arr1[] = {5, 2, 5, 2, 5, 2, 5, 2};
  uint8_t original1[] = {5, 2, 5, 2, 5, 2, 5, 2};
  bitonic_sort_uint8_asc(arr1, 8);

  if (!is_sorted_asc_uint8(arr1, 8) ||
      !is_permutation_uint8(arr1, original1, 8)) {
    FAIL("duplicates case");
    return;
  }

  // Check counts
  int count_2 = 0, count_5 = 0;
  for (int i = 0; i < 8; i++) {
    if (arr1[i] == 2)
      count_2++;
    if (arr1[i] == 5)
      count_5++;
  }

  if (count_2 != 4 || count_5 != 4) {
    FAIL("duplicate counts wrong: 2=%d 5=%d", count_2, count_5);
    return;
  }

  PASS();
}

// Test comparison with qsort
static int cmp_uint8_asc(const void *a, const void *b) {
  uint8_t va = *(uint8_t *)a;
  uint8_t vb = *(uint8_t *)b;
  return (va > vb) - (va < vb);
}

static int cmp_uint8_desc(const void *a, const void *b) {
  uint8_t va = *(uint8_t *)a;
  uint8_t vb = *(uint8_t *)b;
  return (vb > va) - (vb < va);
}

static void test_comparison_qsort(void) {
  TEST("comparison with qsort");

  for (int trial = 0; trial < 20; trial++) {
    uint8_t arr_bitonic[16];
    uint8_t arr_qsort[16];

    for (int i = 0; i < 16; i++) {
      uint8_t val = rand() & 0xFF;
      arr_bitonic[i] = val;
      arr_qsort[i] = val;
    }

    bitonic_sort_uint8_asc(arr_bitonic, 16);
    qsort(arr_qsort, 16, sizeof(uint8_t), cmp_uint8_asc);

    for (int i = 0; i < 16; i++) {
      if (arr_bitonic[i] != arr_qsort[i]) {
        FAIL("mismatch with qsort at trial %d index %d: bitonic=%u qsort=%u",
             trial, i, arr_bitonic[i], arr_qsort[i]);
        return;
      }
    }

    // Test descending
    for (int i = 0; i < 16; i++) {
      uint8_t val = rand() & 0xFF;
      arr_bitonic[i] = val;
      arr_qsort[i] = val;
    }

    bitonic_sort_uint8_desc(arr_bitonic, 16);
    qsort(arr_qsort, 16, sizeof(uint8_t), cmp_uint8_desc);

    for (int i = 0; i < 16; i++) {
      if (arr_bitonic[i] != arr_qsort[i]) {
        FAIL("descending mismatch with qsort at trial %d index %d", trial, i);
        return;
      }
    }
  }

  PASS();
}

int main() {
  // Seed RNG once for all tests
  srand((unsigned int)time(NULL));

  printf("Bitonic Sort Tests\n\n");

  test_uint8_sort_asc();
  test_uint8_sort_desc();
  test_int8_sort();
  test_uint16_sort_properties();
  test_uint32_sort_properties();
  test_int32_sort_properties();
  test_int16_sort_properties();
  test_uint64_sort_properties();
  test_int64_sort_properties();
  test_duplicates();
  test_comparison_qsort();

  printf("\nResults: %d passed, %d failed\n", pass_count, fail_count);

  return fail_count > 0 ? 1 : 0;
}
