#include <stdio.h>
#define printf(...) (0) // Disable all printf calls
// extern void eftsan_print_error(double);
int main() {
  double xs[2] = {1.2, 1e-12};
  double ys[2] = {0.5, 1e12};
  double x, y, z;
  for(int i = 0; i < 2; ++i){
    x = xs[i];
    y = ys[i];
    z = ( x + y ) - y;
    printf("%e\n", z);
    // eftsan_print_error(z);
  }
}
