#ifndef _INVENTORY_HPP_
#define _INVENTORY_HPP_

#include "2d.hpp"
#include "resources.hpp"

using namespace std;
using namespace glm;

enum InventoryState {
  INVENTORY_ITEMS = 0,
  INVENTORY_SPELLBOOK,
  INVENTORY_CRAFT,
  INVENTORY_DIALOG,
  INVENTORY_QUEST_LOG
};

class Inventory {
  shared_ptr<Draw2D> draw_2d_;
  shared_ptr<Resources> resources_;

  InventoryState state_ = INVENTORY_ITEMS;

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
  int throttle_ = 0;
  int cursor_pos_ = 0;

  void UpdateMouse(GLFWwindow* window);
  bool IsMouseInRectangle(int left, int right, int bottom, int top);
  void DrawItemMatrix();
  void DrawSpellbar();
  void DrawContextPanel(int x, int y, const string& name, 
    const string& description);
  void MoveItemBack();
  void DrawInventory(const Camera& camera, int win_x, int win_y, 
    GLFWwindow* window);
  void DrawSpellPage();
  void DrawSpellbook();
  void DrawCraftTable(const Camera& camera, int win_x, int win_y, 
    GLFWwindow* window);
  void DrawDialog(GLFWwindow* window);
  void DrawQuestLog(GLFWwindow* window);
  void NextPhrase(GLFWwindow* window, const string& next_phrase_name = "");

 public:
  bool enabled;

  Inventory(shared_ptr<Resources> asset_catalog, shared_ptr<Draw2D> draw_2d);

  void Draw(const Camera& camera, int win_x = 200, int win_y = 100, GLFWwindow* window = nullptr);

  void Enable(GLFWwindow* window, InventoryState state = INVENTORY_ITEMS);
  void Disable();
  bool Close() { return !enabled; }
};

#endif
