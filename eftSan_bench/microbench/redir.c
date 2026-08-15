#include <stdio.h>
#include <math.h>
#define printf(...) (0) // Disable all printf calls
// extern void eftsan_print_error(double);
int main() {
  volatile double x = 0.0;
  for (int i = 0; i < 2; ++i) {
    double x = i;
    x += sqrt(x);
  }
  // eftsan_print_error(x);
  printf("%e\n", x);
}
