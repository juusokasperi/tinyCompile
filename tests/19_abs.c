int abs(int x) {
    if (x < 0)
        return -x;
    return x;
}

int main() {
    int a = abs(-42);
    int b = abs(17);
    return a + b;  // 42 + 17 = 59
}
