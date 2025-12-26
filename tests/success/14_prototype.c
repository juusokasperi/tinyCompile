int foo(int x);  // Prototype

int bar(int x) {
	return foo(x) + 1;
}

int foo(int x) {  // Definition
	return x * 2;
}

int main() {
	return bar(20);  // Should return 41
}
