#include <stdio.h>
#define SIZE 1024

int main() {
    int data[SIZE][SIZE];
    int sum = 0;

    // Accessing array row-wise
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            sum += data[i][j];
        }
    }

    printf("Sum: %d\n", sum);
    return 0;
}
