#ifndef _INVENTORY_HPP_
#define _INVENTORY_HPP_

#include "2d.hpp"
#include "resources.hpp"

using namespace std;
using namespace glm;

class Inventory {
  shared_ptr<Draw2D> draw_2d_;
  shared_ptr<Resources> resources_;

 public:
  bool enabled;

  Inventory(shared_ptr<Resources> asset_catalog, shared_ptr<Draw2D> draw_2d);

  void Draw(int win_x = 200, int win_y = 100);

  void Enable() { enabled = true; }
  void Disable() { enabled = false; }
  bool Close() { return !enabled; }
};

#endif
