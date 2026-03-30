#include <stdatomic.h>

_Atomic int x = 0;
_Atomic int y = 0;
_Atomic int z = 0;

void mixed_stress_test() {
    // 1. Store-Store Pair
    atomic_store_explicit(&x, 1, memory_order_relaxed);
    atomic_store_explicit(&y, 2, memory_order_relaxed);
    
    // 2. Store-Load Pair (Store Buffer)
    int r1 = atomic_load_explicit(&z, memory_order_relaxed);
    
    // 3. Load-Load Pair
    int r2 = atomic_load_explicit(&x, memory_order_relaxed);
    
    // 4. Load-Store Pair
    atomic_store_explicit(&z, 3, memory_order_relaxed);
}