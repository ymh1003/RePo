#include <stdio.h>
#include <math.h>
#define printf(...) (0) // Disable all printf calls
// extern void eftsan_print_error(double);
int main() {
  volatile float x = 1e-20;
  volatile float y = x * (-1);
  volatile float z = 5.0;
  float w = (y + z) - z;
  // eftsan_print_error(w);
  printf("%e\n", w);
}
