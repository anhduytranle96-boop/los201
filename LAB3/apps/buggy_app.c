#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void process_data(int *data, int len) {
    int sum = 0;
    for (int i = 0; i <= len; i++) {   /* Bug 1: Out of bounds */
        sum += data[i];
    }
    printf("Sum = %d\n", sum);
}

char *create_message(const char *text) {
    char *buf = malloc(strlen(text) + 1);
    strcpy(buf, text);
    free(buf);
    return buf;   /* Bug 2: Return freed pointer */
}

int calculate(int a, int b) {
    int result = a * b;   /* Bug 3: Integer overflow */
    printf("calculate(%d, %d) = %d\n", a, b, result);
    return result;
}

int main(int argc, char *argv[]) {
    printf("=== Buggy App v1.0 – LAB-03 Debug Target ===\n");
    int arr[5] = {10, 20, 30, 40, 50};
    process_data(arr, 5);
    char *msg = create_message("Hello Embedded Linux");
    printf("Message: %s\n", msg);
    calculate(100000, 100000);
    return 0;
}
