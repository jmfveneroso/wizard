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
  INVENTORY_QUEST_LOG,
  INVENTORY_STATS,
  INVENTORY_STORE,
  INVENTORY_SPELL_SELECTION
};

enum ItemOrigin {
  ITEM_ORIGIN_INVENTORY = 0,
  ITEM_ORIGIN_SPELLBAR,
  ITEM_ORIGIN_STORE,
  ITEM_ORIGIN_EQUIPMENT,
  ITEM_ORIGIN_NONE
};

struct DraggedItem {
  ItemOrigin origin = ITEM_ORIGIN_NONE;
  int item_id = 0;
  int quantity = 0;
  int old_pos_x = 0;
  int old_pos_y = 0;
  int hold_offset_x = 0;
  int hold_offset_y = 0;
};

// struct ItemSlot {
//   int x;
//   int y;
//   string callback;
// };
// 
// struct UiLayout {
//   string background;
//   vector<ItemSlot> item_slots;
// };

class Inventory;
typedef void (Inventory::*CallbackFunction)(void); // function pointer type
typedef unordered_map<string, CallbackFunction> CallbackMap;

class Inventory {
  shared_ptr<Draw2D> draw_2d_;
  shared_ptr<Resources> resources_;

  InventoryState state_ = INVENTORY_ITEMS;
  DraggedItem dragged_item;
  Camera camera_;
  int win_x_ = 0;
  int win_y_ = 0;
  int debounce_ = 0;

  int selected_item_ = 0;
  int selected_qnty_ = 0;
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

  ivec2 spell_selection_cursor_ = ivec2(5, 5);

  CallbackMap callback_map_;

  void UpdateMouse(GLFWwindow* window);
  bool IsMouseInRectangle(int left, int right, int bottom, int top);
  void DrawItemMatrix();
  void DrawSpellbar();
  void DrawContextPanel(int x, int y, const string& name, 
    const string& description);
  void DrawSpellSelectionInterface(const Camera& camera, int win_x, int win_y, 
    GLFWwindow* window);
  void MoveItemBack();
  void DrawInventory(const Camera& camera, int win_x, int win_y, 
    GLFWwindow* window);
  void DrawSpellPage();
  void DrawSpellbook();
  void DrawStore(const Camera& camera, int win_x, int win_y, 
    GLFWwindow* window);
  void DrawEquipment(const Camera& camera, int win_x, int win_y, 
    GLFWwindow* window);
  void DrawDialog(GLFWwindow* window);
  void DrawQuestLog(GLFWwindow* window);
  void NextPhrase(GLFWwindow* window, const string& next_phrase_name = "");
  void DrawStats(const Camera& camera, int win_x, int win_y, GLFWwindow* window);
  void TryToCombineCrystals(int x, int y);

 public:
  bool enabled;

  Inventory(shared_ptr<Resources> asset_catalog, shared_ptr<Draw2D> draw_2d);

  void Draw(const Camera& camera, int win_x = 200, int win_y = 100, GLFWwindow* window = nullptr);

  void Enable(GLFWwindow* window, InventoryState state = INVENTORY_ITEMS);
  void Disable();
  bool Close() { return !enabled; }
};

#endif
