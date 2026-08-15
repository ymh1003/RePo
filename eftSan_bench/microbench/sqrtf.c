#include <stdio.h>
#include <math.h>
#define printf(...) (0) // Disable all printf calls
// extern void eftsan_print_error(double);
int main() {
  volatile float x, y;
  x = 4.0f;
  y = sqrtf(x);
  // eftsan_print_error(y);
  printf("%e\n", y);
}
