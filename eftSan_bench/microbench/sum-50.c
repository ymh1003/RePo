#include <stdio.h>
#define printf(...) (0) // Disable all printf calls
// extern void eftsan_print_error(double);
int main() {
  volatile double x = 0;
  while(x < 10.0){
    x += 0.2;
  }
  // eftsan_print_error(x);
  // printf("%.20g\n", x);
  printf("%e\n", x);
}
