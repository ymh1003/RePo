#include <stdio.h>
#define printf(...) (0) // Disable all printf calls
// extern void eftsan_print_error(double);
int main() {
  volatile double x = 1.0;
  double y = x + x;
  printf("%e\n", y);
  // eftsan_print_error(y);
  return 0;
}
