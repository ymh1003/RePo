#include <stdio.h>
#include <math.h> 
#define printf(...) (0) // Disable all printf calls

int main(){
  float x = 1.0;
  float y = 0.9999999;
  float z = x - y;
  // printf("z:%e", z);
  printf("%e\n", z);
}
