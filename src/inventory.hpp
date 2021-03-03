#ifndef _INVENTORY_HPP_
#define _INVENTORY_HPP_

#include "2d.hpp"
#include "resources.hpp"

using namespace std;
using namespace glm;

class Inventory {
  shared_ptr<Draw2D> draw_2d_;
  shared_ptr<Resources> resources_;

  Camera camera_;
  int win_x_ = 0;
  int win_y_ = 0;

  int selected_item_ = 0;
  int old_pos_x_ = 0;
  int old_pos_y_ = 0;
  int hold_offset_x_ = 0;
  int hold_offset_y_ = 0;

  int mouse_x_ = 0;
  int mouse_y_ = 0;
  bool lft_click_ = false;
  bool rgt_click_ = false;

  void UpdateMouse(GLFWwindow* window);
  bool IsMouseInRectangle(int left, int right, int bottom, int top);
  void DrawItemMatrix();
  void DrawSpellbar();
  void DrawContextPanel(int x, int y, int item_id);
  void MoveItemBack();

 public:
  bool enabled;

  Inventory(shared_ptr<Resources> asset_catalog, shared_ptr<Draw2D> draw_2d);

  void Draw(const Camera& camera, int win_x = 200, int win_y = 100, GLFWwindow* window = nullptr);

  void Enable() { enabled = true; }
  void Disable() { enabled = false; }
  bool Close() { return !enabled; }
};

#endif
