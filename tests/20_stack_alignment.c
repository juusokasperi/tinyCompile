// Test 1: Odd number of stack args (1 arg on stack)
// This verifies that you correctly add 8 bytes of padding BEFORE pushing the 7th arg.
// If padding is missing or in the wrong place, 'g' will read garbage.
int test_padding(int a, int b, int c, int d, int e, int f, int g) {
    if (g != 77) return 100; // Fail code
    return 0;
}

// Test 2: Even number of stack args (2 args on stack)
// This verifies that you pushed them in Reverse Order.
// Correct Stack: [ ... | Arg 7 | Arg 8 | RetAddr ]
// If you pushed (7, 8) in forward order, they will be swapped in memory.
int test_order(int a, int b, int c, int d, int e, int f, int g, int h) {
    // We expect g=10, h=20.
    // If swapped: g=20, h=10 -> returns 10.
    return h - g; 
}

int main() {
    // Case 1: 7th arg is 77.
    // If stack is misaligned, we get 100.
    int res1 = test_padding(1, 2, 3, 4, 5, 6, 77);

    // Case 2: 7th=10, 8th=20.
    // Expected: 20 - 10 = 10.
    int res2 = test_order(1, 2, 3, 4, 5, 6, 10, 20);

    // Total should be 0 + 10 = 10.
    return res1 + res2;
}
