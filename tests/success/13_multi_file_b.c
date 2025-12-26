int double_it(int x);

int helper(int x) {
	return x * 2;
}

int main() {
	return double_it(21);  // Should return 42
}
