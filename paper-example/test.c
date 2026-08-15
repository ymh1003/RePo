#include <stdio.h>
#include <math.h>
#define printf(...) (0) // Disable all printf calls

int main() {
  volatile double a,b,c,x,y,z;
  x = 1e99;
  a = x + 1;
  b = sqrt(x);
  c = sqrt(a);
  y = c - b;
  z = y * y;
  return 0;
}
