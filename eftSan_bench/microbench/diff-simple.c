#include <stdio.h>
#include <math.h>
#define printf(...) (0) // Disable all printf calls

int main() {
  double x,y;
  x = 1e16;
  y = (x + 1) - x;
  printf("%e\n", y);
  return 0;
}
