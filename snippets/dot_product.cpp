#include <iostream>

int main() { 
  float x1 = 0.70710678118;
  float y1 = 0.70710678118;
  float z1 = 0;

  float d = -5.65685425;

  float x2 = 4;
  float y2 = 4;
  float z2 = 10;


  float dot_product = x1 * x2 + y1 * y2 + z1 * z2;
  std::cout << "dot_product: " << dot_product << std::endl;

  float res = dot_product + d;
  std::cout << "res: " << res << std::endl;

  return 0;
}
