#include <stdio.h>
#define printf(...) (0) // Disable all printf calls

int main() {
  volatile double x;
  int cmp;
  x = 0.0;
  cmp = (x == 0.0);
  printf("%d\n", cmp);
}
