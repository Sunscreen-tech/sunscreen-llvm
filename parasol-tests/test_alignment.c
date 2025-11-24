#include <stdint.h>

// Alignment verification tests for i128 constants
// This file tests whether Parasol ISA requires strict 16-byte alignment for i128
// or if 8-byte alignment is sufficient

#ifdef __SIZEOF_INT128__

// Test with compiler-default alignment (should use current implementation: 16 bytes)
__uint128_t test_default_align(__uint128_t a) {
    __uint128_t c = ((__uint128_t)0x1122334455667788ULL << 64 | 0x99AABBCCDDEEFF00ULL);
    return a + c;
}

// Test with explicit 1-byte alignment (stress test)
__attribute__((aligned(1)))
static const __uint128_t const_align_1 = ((__uint128_t)0x1122334455667788ULL << 64 | 0x99AABBCCDDEEFF00ULL);

__uint128_t test_align_1(__uint128_t a) {
    return a + const_align_1;
}

// Test with explicit 4-byte alignment
__attribute__((aligned(4)))
static const __uint128_t const_align_4 = ((__uint128_t)0x1122334455667788ULL << 64 | 0x99AABBCCDDEEFF00ULL);

__uint128_t test_align_4(__uint128_t a) {
    return a + const_align_4;
}

// Test with explicit 8-byte alignment
__attribute__((aligned(8)))
static const __uint128_t const_align_8 = ((__uint128_t)0x1122334455667788ULL << 64 | 0x99AABBCCDDEEFF00ULL);

__uint128_t test_align_8(__uint128_t a) {
    return a + const_align_8;
}

// Test with explicit 16-byte alignment
__attribute__((aligned(16)))
static const __uint128_t const_align_16 = ((__uint128_t)0x1122334455667788ULL << 64 | 0x99AABBCCDDEEFF00ULL);

__uint128_t test_align_16(__uint128_t a) {
    return a + const_align_16;
}

// Test with explicit 32-byte alignment (over-aligned)
__attribute__((aligned(32)))
static const __uint128_t const_align_32 = ((__uint128_t)0x1122334455667788ULL << 64 | 0x99AABBCCDDEEFF00ULL);

__uint128_t test_align_32(__uint128_t a) {
    return a + const_align_32;
}

#endif // __SIZEOF_INT128__

// i64 alignment tests for comparison
uint64_t test_i64_default(uint64_t a) {
    return a + 0x123456789ABCDEF0ULL;
}

__attribute__((aligned(1)))
static const uint64_t const64_align_1 = 0x123456789ABCDEF0ULL;

uint64_t test_i64_align_1(uint64_t a) {
    return a + const64_align_1;
}

__attribute__((aligned(4)))
static const uint64_t const64_align_4 = 0x123456789ABCDEF0ULL;

uint64_t test_i64_align_4(uint64_t a) {
    return a + const64_align_4;
}

__attribute__((aligned(8)))
static const uint64_t const64_align_8 = 0x123456789ABCDEF0ULL;

uint64_t test_i64_align_8(uint64_t a) {
    return a + const64_align_8;
}

__attribute__((aligned(16)))
static const uint64_t const64_align_16 = 0x123456789ABCDEF0ULL;

uint64_t test_i64_align_16(uint64_t a) {
    return a + const64_align_16;
}
