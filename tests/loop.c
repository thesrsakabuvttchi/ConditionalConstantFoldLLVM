#include <stdio.h>

int computeSquare(int x) {
    return x * x;
}

int computeTriple(int x) {
    return 3 * x;
}

int main() {
    const int a = 10;
    const int b = 5;
    int result1 = computeSquare(a); // Should propagate constant but not inline function.
    int result2 = computeTriple(b); // Should propagate constant but not inline function.

    int complexCalculation = result1 * b + result2 * a;

    // Simplified computation with constants and predictable outcomes
    complexCalculation += a * b;

    // Loop that depends on a constant, potential for loop unrolling and simplification
    for (int i = 0; i < a; i++) {  // 'a' is a constant, loop should optimize well
        complexCalculation += i * computeSquare(b); // Still calling function without inlining
    }

    printf("Result: %d\n", complexCalculation);
    return 0;
}
