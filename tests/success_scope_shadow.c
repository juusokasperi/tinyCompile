int main(void) {
    int x = 10;
    {
        int x = 5;
        x = x + 1;
        // Should be 6 here, but outer x is still 10
    }
    return (x); // Should return 10
}
