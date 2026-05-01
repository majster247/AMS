#include <stdint.h>

extern "C" {
    int current_test_idx = 0;
    extern const int TOTAL_TESTS = 10;
    const char* test_files[] = {
        "/tests/tcc/t1_basic.c", "/tests/tcc/t2_fib.c", "/tests/tcc/t3_ptr.c", "/tests/tcc/t4_struct.c",
        "/tests/tcc/t5_loops.c", "/tests/tcc/t6_include.c", "/tests/tcc/t7_math.c", "/tests/tcc/t8_strings.c",
        "/tests/tcc/t9_bit.c", "/tests/tcc/t10_io.c"
    };
}
