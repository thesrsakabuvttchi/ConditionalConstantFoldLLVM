#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int main() {
    int result = 0;
    for (int i = 0; i < 100; i++) {
        result = add(result, i);
    }
    printf("Result: %d\n", result);
    return 0;
}
