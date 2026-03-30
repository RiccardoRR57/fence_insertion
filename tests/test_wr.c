#include <stdatomic.h>

_Atomic int A = 0;
_Atomic int B = 0;

void test_wr() {
    // Write 1
    atomic_store_explicit(&A, 1, memory_order_relaxed);
    // Read 1
    int temp = atomic_load_explicit(&B, memory_order_relaxed);
}