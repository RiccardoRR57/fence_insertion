#include <stdio.h>
#include <stdlib.h>

int compute(int x) {
    int result = 0;
    
    if (x > 10) {
        result = x * 2;
        if (x > 20) {
            result += 5;
        } else {
            result -= 3;
        }
    } else if (x > 0) {
        result = x + 10;
    } else {
        result = -x;
    }
    
    return result;
}

int loop_example(int n) {
    int sum = 0;
    
    for (int i = 0; i < n; i++) {
        if (i % 2 == 0) {
            sum += i;
        } else {
            sum -= i;
        }
    }
    
    return sum;
}

int main() {
    int a = 15;
    int b = 5;
    int c = -3;
    
    printf("compute(%d) = %d\n", a, compute(a));
    printf("compute(%d) = %d\n", b, compute(b));
    printf("compute(%d) = %d\n", c, compute(c));
    
    printf("loop_example(10) = %d\n", loop_example(10));
    
    return 0;
}