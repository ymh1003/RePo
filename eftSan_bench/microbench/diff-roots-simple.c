#include <stdio.h>
#include <math.h>
#define printf(...) (0) // Disable all printf calls

int main() {
  volatile double x,y;
  x = 1e99;
  y = sqrt(x + 1) - sqrt(x);
  printf("%e\n", y);
  return 0;
}
