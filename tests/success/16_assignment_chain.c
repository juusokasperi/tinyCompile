int main() {
	int x = 5;
	int y = 10;
	
	x = y;		 // x = 10
	y = x + 5;	 // y = 15
	x = y - 3;	 // x = 12
	
	return x + y;  // 27
}
