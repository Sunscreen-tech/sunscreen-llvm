// test_large_constants.c - Tests for 64-bit and 128-bit constant generation
//
// Created by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information

#include <stdint.h>

// Test basic 64-bit constant usage
uint64_t test_64bit_constant(uint64_t a) {
    return a + 0x123456789ABCDEF0ULL;
}

// Test with zero
uint64_t test_zero(uint64_t a) {
    return a + 0ULL;
}

// Test with small value that fits in 32 bits
uint64_t test_small(uint64_t a) {
    return a + 42ULL;
}

// Test with max value
uint64_t test_max(uint64_t a) {
    return a + 0xFFFFFFFFFFFFFFFFULL;
}

// Test with value requiring only high bits
uint64_t test_high_only(uint64_t a) {
    return a + 0x100000000ULL;
}

// 128-bit constant tests
#ifdef __SIZEOF_INT128__

// Test basic 128-bit constant
__uint128_t test_128bit_constant(__uint128_t a) {
    return a + ((__uint128_t)0x123456789ABCDEF0ULL << 64 | 0xFEDCBA9876543210ULL);
}

// Test 128-bit zero (should optimize to X0 register)
__uint128_t test_128bit_zero(__uint128_t a) {
    return a + 0;
}

// Test 128-bit max value
__uint128_t test_128bit_max(__uint128_t a) {
    __uint128_t max = ~((__uint128_t)0);
    return a + max;
}

// Test 128-bit high 64 bits only
__uint128_t test_128bit_high_only(__uint128_t a) {
    return a + ((__uint128_t)1 << 64);
}

// Test 128-bit low 64 bits only
__uint128_t test_128bit_low_only(__uint128_t a) {
    return a + 0xFFFFFFFFFFFFFFFFULL;
}

// Test 128-bit alternating bit pattern
__uint128_t test_128bit_alternating(__uint128_t a) {
    return a + ((__uint128_t)0xAAAAAAAAAAAAAAAAULL << 64 | 0x5555555555555555ULL);
}

// Test 128-bit small constant
__uint128_t test_128bit_small(__uint128_t a) {
    return a + 42;
}

// Test 128-bit power of two
__uint128_t test_128bit_power_of_two(__uint128_t a) {
    return a + ((__uint128_t)1 << 100);
}

// Test multiple 128-bit constants in one function (constant pool merging)
__uint128_t test_128bit_multiple(__uint128_t a, __uint128_t b) {
    __uint128_t c1 = ((__uint128_t)0x1111111111111111ULL << 64 | 0x2222222222222222ULL);
    __uint128_t c2 = ((__uint128_t)0x3333333333333333ULL << 64 | 0x4444444444444444ULL);
    return a + c1 + b + c2;
}

#endif // __SIZEOF_INT128__
