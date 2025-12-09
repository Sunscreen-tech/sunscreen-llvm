// Test file for .parasol_meta section type metadata emission
// This file tests that fhe_program functions emit proper type metadata
// to the .parasol_meta ELF section.

#include <stdint.h>

// Simple struct for testing
struct Point {
  int32_t x;
  int32_t y;
};

// Nested struct
struct Rectangle {
  struct Point top_left;
  struct Point bottom_right;
};

// Array containing struct
struct PointArray {
  struct Point points[4];
  uint32_t count;
};

// Test function with basic integer types
[[clang::fhe_program]] uint8_t add_u8(uint8_t a, uint8_t b) { return a + b; }

// Test function with 16-bit integers
[[clang::fhe_program]] int16_t add_i16(int16_t a, int16_t b) { return a + b; }

// Test function with 32-bit integers
[[clang::fhe_program]] uint32_t add_u32(uint32_t a, uint32_t b) { return a + b; }

// Test function with 64-bit integers
[[clang::fhe_program]] uint64_t add_u64(uint64_t a, uint64_t b) { return a + b; }

// Test function with 128-bit integers
[[clang::fhe_program]] __uint128_t add_u128(__uint128_t a, __uint128_t b) {
  return a + b;
}

// Test function with pointer parameter
[[clang::fhe_program]] void write_u32(uint32_t *ptr, uint32_t value) {
  *ptr = value;
}

// Test function with array parameter (decays to pointer)
[[clang::fhe_program]] uint32_t sum_array(uint32_t *arr, uint32_t len) {
  uint32_t sum = 0;
  for (uint32_t i = 0; i < len; i++) {
    sum += arr[i];
  }
  return sum;
}

// Test function with struct parameter
// DISABLED: Parasol backend doesn't support struct-by-value parameters yet
// [[clang::fhe_program]] int32_t point_manhattan_distance(struct Point p) {
//   int32_t ax = p.x < 0 ? -p.x : p.x;
//   int32_t ay = p.y < 0 ? -p.y : p.y;
//   return ax + ay;
// }

// Test function with struct pointer parameter
// DISABLED: Struct pointer types show as void* in metadata (known limitation)
// [[clang::fhe_program]] void translate_point(struct Point *p, int32_t dx,
//                                             int32_t dy) {
//   p->x += dx;
//   p->y += dy;
// }

// Test function with nested struct
// DISABLED: Parasol backend doesn't support struct-by-value parameters yet
// [[clang::fhe_program]] int32_t rectangle_area(struct Rectangle r) {
//   int32_t width = r.bottom_right.x - r.top_left.x;
//   int32_t height = r.bottom_right.y - r.top_left.y;
//   if (width < 0)
//     width = -width;
//   if (height < 0)
//     height = -height;
//   return width * height;
// }

// Test function with struct containing fixed-size array
// DISABLED: Parasol backend doesn't support struct-by-value parameters yet
// [[clang::fhe_program]] uint32_t point_array_count(struct PointArray pa) {
//   return pa.count;
// }

// Test function with void return type
[[clang::fhe_program]] void no_return(uint32_t x) { (void)x; }

// Test function with no parameters
[[clang::fhe_program]] uint32_t constant_value(void) { return 42; }

// Test function with many parameters
[[clang::fhe_program]] uint32_t many_params(uint8_t a, uint16_t b, uint32_t c,
                                            uint64_t d, int8_t e, int16_t f,
                                            int32_t g) {
  return (uint32_t)a + (uint32_t)b + c + (uint32_t)d + (uint32_t)e +
         (uint32_t)f + (uint32_t)g;
}

// Test functions with explicitly signed types (for debug info testing)
[[clang::fhe_program]] int8_t test_signed_i8(int8_t a, int8_t b) {
  return a + b;
}

[[clang::fhe_program]] int16_t test_signed_i16(int16_t a, int16_t b) {
  return a + b;
}

[[clang::fhe_program]] int32_t test_signed_i32(int32_t a, int32_t b) {
  return a + b;
}

[[clang::fhe_program]] int64_t test_signed_i64(int64_t a, int64_t b) {
  return a + b;
}

// Test functions with explicitly unsigned types (for debug info testing)
[[clang::fhe_program]] uint8_t test_unsigned_u8(uint8_t a, uint8_t b) {
  return a + b;
}

[[clang::fhe_program]] uint16_t test_unsigned_u16(uint16_t a, uint16_t b) {
  return a + b;
}

[[clang::fhe_program]] uint32_t test_unsigned_u32(uint32_t a, uint32_t b) {
  return a + b;
}

[[clang::fhe_program]] uint64_t test_unsigned_u64(uint64_t a, uint64_t b) {
  return a + b;
}

// Test functions with pointer types (for debug info testing)
[[clang::fhe_program]] void test_pointer_int32(int32_t *ptr) {
  if (ptr)
    *ptr = 42;
}

[[clang::fhe_program]] void test_pointer_uint8(uint8_t *ptr) {
  if (ptr)
    *ptr = 255;
}

[[clang::fhe_program]] int32_t test_pointer_deref(int32_t *a, int32_t *b) {
  return *a + *b;
}

// Non-fhe_program function for comparison (should NOT be in metadata)
uint32_t regular_function(uint32_t a, uint32_t b) { return a * b; }

// Main for testing with system compiler (not compiled for Parasol target)
#ifndef __PARASOL__
int main(void) {
  // Basic tests
  if (add_u8(1, 2) != 3)
    return 1;
  if (add_i16(-1, 2) != 1)
    return 2;
  if (add_u32(100, 200) != 300)
    return 3;
  if (add_u64(1000, 2000) != 3000)
    return 4;

  // Pointer test
  uint32_t val = 0;
  write_u32(&val, 42);
  if (val != 42)
    return 5;

  // Array test
  uint32_t arr[] = {1, 2, 3, 4, 5};
  if (sum_array(arr, 5) != 15)
    return 6;

  // Struct tests
  // DISABLED: Parasol backend doesn't support struct-by-value parameters yet
  // struct Point p = {3, 4};
  // if (point_manhattan_distance(p) != 7)
  //   return 7;

  // DISABLED: Struct pointer types show as void* in metadata (known limitation)
  // translate_point(&p, 1, 1);
  // if (p.x != 4 || p.y != 5)
  //   return 8;

  // DISABLED: Parasol backend doesn't support struct-by-value parameters yet
  // struct Rectangle r = {{0, 0}, {10, 5}};
  // if (rectangle_area(r) != 50)
  //   return 9;

  // Void return and no params
  no_return(123);
  if (constant_value() != 42)
    return 10;

  // Many params
  if (many_params(1, 2, 3, 4, 5, 6, 7) != 28)
    return 11;

  // Signed types
  if (test_signed_i8(10, 20) != 30)
    return 12;
  if (test_signed_i16(-100, 200) != 100)
    return 13;
  if (test_signed_i32(-1000, 2000) != 1000)
    return 14;
  if (test_signed_i64(-10000, 20000) != 10000)
    return 15;

  // Unsigned types
  if (test_unsigned_u8(100, 50) != 150)
    return 16;
  if (test_unsigned_u16(1000, 2000) != 3000)
    return 17;
  if (test_unsigned_u32(10000, 20000) != 30000)
    return 18;
  if (test_unsigned_u64(100000, 200000) != 300000)
    return 19;

  // Pointer types
  int32_t ptr_val = 0;
  test_pointer_int32(&ptr_val);
  if (ptr_val != 42)
    return 20;

  uint8_t ptr_val_u8 = 0;
  test_pointer_uint8(&ptr_val_u8);
  if (ptr_val_u8 != 255)
    return 21;

  int32_t ptr_a = 100;
  int32_t ptr_b = 200;
  if (test_pointer_deref(&ptr_a, &ptr_b) != 300)
    return 22;

  return 0;
}
#endif // __PARASOL__
