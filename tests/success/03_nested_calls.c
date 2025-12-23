int add(int a, int b) {
    return a + b;
}

int mul(int a, int b) {
    return a * b;
}

int main() {
    return add(mul(2, 3), mul(4, 5));  // add(6, 20) = 26
}
