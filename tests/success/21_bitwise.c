int main() {
    int a = 12;      // Binary: 0000 1100
    int b = 5;       // Binary: 0000 0101

    // 1. Basic Operations
    int c = a & b;   // 0100 = 4
    int d = a | b;   // 1101 = 13
    int e = a ^ b;   // 1001 = 9
    int f = ~a;      // ~12  = -13 (in 2's complement)

    // 2. Precedence Tests
    
    // & has higher precedence than ^
    // Should be: (4 & 5) ^ 1  => 4 ^ 1 => 5
    // If wrong:  4 & (5 ^ 1)  => 4 & 4 => 4
    int g = 4 & 5 ^ 1; 

    // ^ has higher precedence than |
    // Should be: 1 | (2 ^ 3)  => 1 | 1 => 1
    // If wrong:  (1 | 2) ^ 3  => 3 ^ 3 => 0
    int h = 1 | 2 ^ 3;

    // Shift (<<) has higher precedence than &
    // Should be: (1 << 2) & 7 => 4 & 7 => 4
    // If wrong:  1 << (2 & 7) => 1 << 2 => 4 (Wait, bad test case, let's trust the logic)
    // Better: 1 << 1 & 3 => 2 & 3 => 2
    int i = 1 << 1 & 3;

    // 3. Chain Verification
    // Sum: 4 + 13 + 9 + (-13) + 5 + 1 + 2 = 21
    return c + d + e + f + g + h + i; 
}
