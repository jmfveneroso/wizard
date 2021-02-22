#include <iostream>

using namespace std;

int main () {
  int x = 24445;

  unsigned char b1 = (x >> 8);
  unsigned char b2 = (x & 255);

  int y = (b1 << 8) + (b2);
  cout << y << endl; 
}
