int main() {
	int x = 5;
	int y = 10;
	
	int eq = (x == y);		 // 0
	int neq = (x != y);		 // 1
	int lt = (x < y);		 // 1
	int le = (x <= y);		 // 1
	int gt = (x > y);		 // 0
	int ge = (x >= y);		 // 0
	
	return eq + neq + lt + le + gt + ge;  // 3
}
