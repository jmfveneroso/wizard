#ifndef _INVENTORY_HPP_
#define _INVENTORY_HPP_

#include "2d.hpp"
#include "resources.hpp"

using namespace std;
using namespace glm;

enum InventoryState {
  INVENTORY_ITEMS = 0,
  INVENTORY_SPELLBOOK,
  INVENTORY_DIALOG,
  INVENTORY_STORE,
  INVENTORY_SPELL_SELECTION,
  INVENTORY_MAP
};

enum ItemOrigin {
  ITEM_ORIGIN_INVENTORY = 0,
  ITEM_ORIGIN_SPELLBAR,
  ITEM_ORIGIN_STORE,
  ITEM_ORIGIN_EQUIPMENT,
  ITEM_ORIGIN_SPELL_GRAPH,
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

  bool map_dragging_ = false;
  ivec2 map_drag_origin_ = ivec2(0, 0);
  ivec2 map_offset_ = ivec2(0, 0);

  ivec2 spell_selection_cursor_ = ivec2(5, 5);



  vec2 inventory_pos_;
  vec2 inventory_pos_start_;
  vec2 inventory_pos_target_;
  const float inventory_animation_duration_ = 1.0f;
  float inventory_animation_start_ = 0.0f;

  vec2 item_description_screen_pos_;
  vec2 item_description_screen_pos_start_;
  vec2 item_description_screen_pos_target_;
  const float item_description_screen_animation_duration_ = 1.0f;
  float item_description_screen_animation_start_ = 0.0f;

  vec2 spellbook_pos_;
  vec2 spellbook_pos_start_;
  vec2 spellbook_pos_target_;
  const float spellbook_animation_duration_ = 1.0f;
  float spellbook_animation_start_ = 0.0f;

  vec2 cog_rotation_;
  vec2 cog_rotation_start_;
  vec2 cog_rotation_target_;
  const float cog_animation_duration_ = 1.0f;
  float cog_animation_start_ = 0.0f;

  vec2 spell_description_pos_;
  vec2 spell_description_pos_start_;
  vec2 spell_description_pos_target_;
  const float spell_description_animation_duration_ = 1.0f;
  float spell_description_animation_start_ = 0.0f;

  vec2 store_pos_;
  vec2 store_pos_start_;
  vec2 store_pos_target_;
  const float store_animation_duration_ = 1.0f;
  float store_animation_start_ = 0.0f;

  float closing = 0.0f;
  GLFWwindow* window_;

  string crystal_combination_anim_name_ = "item-combination";
  ivec2 crystal_combination_pos_;
  float crystal_combination_frame_ = -1;
  float level_up_frame_ = -1;
  ivec2 crystal_after_pos_;
  int crystal_after_;
  void DrawAnimation(const string& animation_name, const ivec2& pos, int frame);

  void UpdateMouse(GLFWwindow* window);
  bool IsMouseInRectangle(int left, int right, int bottom, int top);
  bool IsMouseInCircle(const ivec2& pos, float circle);
  void DrawContextPanel(int x, int y, const string& name, 
    const string& description);
  void DrawSpellSelectionInterface(const Camera& camera, int win_x, int win_y, 
    GLFWwindow* window);
  void MoveItemBack();
  void UpdateAnimations();
  void DrawCogs();
  void DrawItemDescriptionScreen();
  void DrawStats(const ivec2& pos);
  void DrawOverlay(const ivec2& pos);
  void DrawItems(const ivec2& pos);
  void DrawInventory();
  void DrawSpellDescription();
  void DrawSpellbook();
  void DrawStore(const Camera& camera, int win_x, int win_y, 
    GLFWwindow* window);
  void DrawStoreItems(const ivec2& pos);
  // void DrawStoreItemDescription(const ivec2& pos);
  void DrawEquipment(const ivec2& pos);
  void DrawDialog(GLFWwindow* window);
  void NextPhrase(GLFWwindow* window, const string& next_phrase_name = "");
  void UseItem(int x, int y);
  bool TryToCombineCrystals(int x, int y);
  void DrawMap();
  int WhichItem(const int item_matrix[10][5], int x, int y, ivec2& pos);

 public:
  bool enabled;

  Inventory(shared_ptr<Resources> asset_catalog, shared_ptr<Draw2D> draw_2d);

  void DrawSpellbar();
  void Draw(const Camera& camera, int win_x = 200, int win_y = 100, GLFWwindow* window = nullptr);

  void Enable(GLFWwindow* window, InventoryState state = INVENTORY_ITEMS);
  void Disable();
  bool Close() { return !enabled; }
};

#endif
