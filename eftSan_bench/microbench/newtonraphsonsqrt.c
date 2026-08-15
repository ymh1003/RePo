#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define printf(...) (0) // Disable all printf calls
// extern void eftsan_print_error(double);
float f(float x, float v) {
    return x * x - v;
}

float df(float x) {
    return 2 * x;
}

void floatNewtonRaphson() {
	float v = 1771.0;
	float oldX, x = 25.0f;
	float eps = 0.00001f;
	int maxStep = 100;
	int iter = 0;
	float d;
	oldX = x;
	float diffX = df(x);
	if (fabs(diffX) != 0.0 && diffX != 0) {
		x = x - f(x, v) / diffX;
	}
	else {
		printf("Unexpected slope\n");
		exit(1);
	}
	d = fabs(oldX - x);
	while ( d > eps && iter < maxStep){
		printf("Iteration %d: \n", iter);
		oldX = x;
		float diffX = df(x);
		if (fabs(diffX) != 0.0 && diffX != 0) {
			x = x - f(x, v) / diffX;
		}
		else {
			printf("Unexpected slope\n");
			exit(1);
		}
		iter ++;
		d = fabs(oldX - x);
	}
	// eftsan_print_error(x);
	// printf("%e\n", x);
}

int main() {
    floatNewtonRaphson();
    
}

