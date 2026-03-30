#include <stdatomic.h>

_Atomic int A = 0;
_Atomic int B = 0;

void test_rw() {
    // Read 1
    int temp = atomic_load_explicit(&A, memory_order_relaxed);
    // Write 1
    atomic_store_explicit(&B, 1, memory_order_relaxed);
}