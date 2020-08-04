#include <iostream>

[2 3 4 1]  [1 3 4 4]  [2 3 4 1]
[5 1 6 2]  [5 5 6 6]  [5 3 6 2]
[2 4 7 5]  [2 4 7 7]  [5 4 7 5]
[1 3 6 3]  [1 3 6 6]  [5 4 7 5]

[0 3 8 2]  [0 3 8 8]  [2 3 4 1]
[5 1 6 2]  [5 5 6 6]  [5 3 6 2]
[2 4 7 5]  [2 4 7 7]  [5 4 7 5]
[1 3 6 3]  [1 3 6 6]  [5 4 7 5]

max_height = 7

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
