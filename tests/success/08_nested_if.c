int main() {
	int x = 5;
	
	if (x > 0) {
		if (x > 2) {
			if (x > 4) {
				if (x > 6) {
					return 1;
				} else {
					return 2;  // Should return this
				}
			}
		}
	}
	return 0;
}
