#include <boost/algorithm/string.hpp>
#include "inventory.hpp"

// const int kWindowWidth = 1280;
// const int kWindowHeight = 800;
const int kWindowWidth = 1440;
const int kWindowHeight = 900;
const int kTileSize = 52;
const int kMaxStash = 100;

Inventory::Inventory(shared_ptr<Resources> asset_catalog, 
  shared_ptr<Draw2D> draw_2d) : resources_(asset_catalog), 
  draw_2d_(draw_2d) {
}

int Inventory::WhichItem(const int item_matrix[10][5], int x, int y, 
  ivec2& pos) {
  if (x < 0 || x >= 10 || y < 0 || y >= 5) return 0;

  if (item_matrix[x][y] != -1) return item_matrix[x][y];

  for (int x_ = x; x_ >= 0; x_--) {
    for (int y_ = y; y_ >= 0; y_--) {
      int item_id = item_matrix[x_][y_];
      if (item_id == 0) continue;
      if (item_id == -1) continue;
      
      const ItemData& item_data = resources_->GetItemData()[item_id];
      const ivec2& size = item_data.size;
      if (x >= x_ && y >= y_ && x < x_ + size.x && y < y_ + size.y) {
        pos = ivec2(x_, y_);
        return item_id;
      }
    }
  }

  return 0;
}

void Inventory::UpdateMouse(GLFWwindow* window) {
  double x_pos, y_pos;
  glfwGetCursorPos(window, &x_pos, &y_pos);
  mouse_x_ = x_pos;
  mouse_y_ = y_pos;

  lft_click_ = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  rgt_click_ = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
  if (mouse_x_ < 0) mouse_x_ = 0; 
  if (mouse_x_ > kWindowWidth - 1) mouse_x_ = kWindowWidth - 1; 
  if (mouse_y_ < 0) mouse_y_ = 0; 
  if (mouse_y_ > kWindowHeight) mouse_y_ = kWindowHeight; 
  glfwSetCursorPos(window, mouse_x_, mouse_y_); 
}

bool Inventory::IsMouseInRectangle(int left, int right, int bottom, int top) {
  return (mouse_x_ > left && mouse_x_ < right && 
          mouse_y_ < bottom && mouse_y_ > top);
}

bool Inventory::IsMouseInCircle(const ivec2& pos, float circle) {
  return length2(vec2(mouse_x_, mouse_y_) - vec2(pos)) < circle * circle;
}

bool Inventory::TryToCombineCrystals(int x, int y) {
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[10][5] = configs->item_matrix;
  int (&item_quantities)[10][5] = configs->item_quantities;

  int item_a = item_matrix[x][y];
  int item_b = selected_item_;

  // TODO: check if quantities == crytal max stash size.

  int combination = resources_->CrystalCombination(item_a, item_b);
  if (combination != -1) {
    ivec2 pos = vec2(win_x_, win_y_) + inventory_pos_;
    int left = pos.x + 34 + kTileSize * x;
    int top = pos.y + 547 + kTileSize * y;
    crystal_combination_anim_name_ = "item-combination";
    crystal_combination_pos_ = ivec2(left - 40, top - 27);
    crystal_combination_frame_ = 0;
    crystal_after_pos_ = ivec2(x, y);
    crystal_after_ = combination;

    item_matrix[x][y] = 0;
    item_quantities[x][y] = 0;
    // const ItemData& spell_data = item_data[combination];

    // item_matrix[x][y] = combination;
    // item_quantities[x][y] = spell_data.max_stash;

    // if (dragged_item.origin == ITEM_ORIGIN_STORE) {
    //   const int price = item_data[selected_item_].price;
    //   resources_->TakeGold(price);
    // }
    selected_item_ = 0;
    selected_qnty_ = 0;
    return true;
  }
  return false;
}

void Inventory::DrawSpellbar() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  int (&spellbar)[8] = configs->spellbar;
  int (&spellbar_quantities)[8] = configs->spellbar_quantities;

  draw_2d_->DrawImage("spellbar", 0, 0, 1440, 1440, 1.0);

  int top = 852;
  int left = 463;
  for (int x = 0; x < 7; x++) {
    int item_id = configs->spellbar[x];
    int item_quantity = configs->spellbar_quantities[x];
    if (item_id != 0) {
      auto spell = resources_->WhichArcaneSpell(item_id);
      if (spell) {
        draw_2d_->DrawImageWithMask(spell->image_name,
          "spellbar_item_mask", left, top, 44, 44, 1.0); 
      } else {
        float alpha = (resources_->InventoryHasItem(item_id)) ? 1.0 : 0.3;
        draw_2d_->DrawImageWithMask(item_data[item_id].image, 
          "spellbar_item_mask", left, top, 44, 44, alpha); 
        // draw_2d_->DrawImageWithMask("red", "spellbar_mask", 0, 0, w * 1440, 
        //   h * 1440, 1.0, vec2(0, 0), vec2(w, h));
      } 
    }

    if (configs->selected_spell == x) {
      draw_2d_->DrawImage("selected_item", left, top, 64, 64, 1.0); 
    }

    int bottom = top + 50;
    int right = left + 50;
    if (dragged_item.origin != ITEM_ORIGIN_NONE && 
        IsMouseInRectangle(left, right, bottom, top) && !lft_click_) {
      auto spell = resources_->WhichArcaneSpell(dragged_item.item_id);
      if (!spell) {
        return;
      }

      if (dragged_item.origin != ITEM_ORIGIN_SPELL_GRAPH) {
        return;
      }

      spellbar[x] = dragged_item.item_id;
      spellbar_quantities[x] = 1;
    }
    left += 52;
  }

  int (&active_items)[3] = configs->active_items;
  for (int x = 0; x < 3; x++) {
    int item_id = active_items[x];
    if (item_id != 0) {
      draw_2d_->DrawImageWithMask(item_data[item_id].image, 
        "spellbar_item_mask", left, top, 44, 44, 1.0); 
    }
    left += 52;
  }
}

void Inventory::MoveItemBack() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[10][5] = configs->item_matrix;
  int (&item_quantities)[10][5] = configs->item_quantities;
  int (&store)[6] = configs->store;
  int (&spellbar)[8] = configs->spellbar;
  int (&spellbar_quantities)[8] = configs->spellbar_quantities;
  int (&equipment)[4] = configs->equipment;
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();

  int top = win_y_;
  int bottom = top + 900;
  int left = win_x_;
  int right = left + 900;
  if (state_ == INVENTORY_STORE) {
    right = left + 1160;
  }

  if (IsMouseInRectangle(left, right, bottom, top)) {
    switch (dragged_item.origin) {
      case ITEM_ORIGIN_EQUIPMENT: {
        const ItemData& item_data = resources_->GetItemData()[selected_item_];
        equipment[old_pos_x_] = selected_item_;
        break;
      }
      case ITEM_ORIGIN_STORE: {
        store[old_pos_x_] = selected_item_;
        break;
      } 
      case ITEM_ORIGIN_ACTIVE: {
        configs->active_items[old_pos_x_] = selected_item_;
        break;
      }
      case ITEM_ORIGIN_PASSIVE: {
        configs->passive_items[old_pos_x_] = selected_item_;
        break;
      }
      default: {
        if (old_pos_y_ == -1) {
          spellbar[old_pos_x_] = selected_item_;
          spellbar_quantities[old_pos_x_] = selected_qnty_;
          break;
        } else if (old_pos_x_ == -1) {
          break;
        } else {
          item_matrix[old_pos_x_][old_pos_y_] = selected_item_;

          const ivec2& size = item_data[selected_item_].size;
          for (int step_x = 0; step_x < size.x; step_x++) {
            for (int step_y = 0; step_y < size.y; step_y++) {
              if (step_x == 0 && step_y == 0) continue;
              item_matrix[old_pos_x_ + step_x][old_pos_y_ + step_y] = -1;
            }
          }
        }
        break;
      }
    }
  } else if (selected_item_ != -1) {
    // Drop item.
    vec3 position = camera_.position + camera_.direction * 5.0f;
    ObjPtr obj = CreateGameObjFromAsset(resources_.get(), 
      item_data[selected_item_].asset_name, position);
    obj->quantity = selected_qnty_;

    float x = Random(0, 11) * .05f;
    float y = Random(0, 11) * .05f;
    float z = Random(0, 11) * .05f;
    obj->torque = normalize(vec3(x, y, z)) * 2.0f;

    obj->CalculateCollisionData();
  }

  dragged_item.origin = ITEM_ORIGIN_NONE;
  selected_item_ = 0;
  selected_qnty_ = 0;
  old_pos_x_ = 0;
  old_pos_y_ = 0;
}

void Inventory::DrawCogs() {
  ivec2 pos = vec2(win_x_, win_y_) + inventory_pos_;
  draw_2d_->DrawImage("cog", pos.x - 60, pos.y - 60, 150, 150, 1.0);
  draw_2d_->DrawImage("cog", pos.x - 40, pos.y - 40, 110, 110, 1.0);
}

void Inventory::DrawEquipment(const ivec2& pos) {
  // shared_ptr<Configs> configs = resources_->GetConfigs();
  // int (&passive_items)[3] = configs->passive_items;
  // unordered_map<int, ItemData>& item_data = resources_->GetItemData();

  // vector<tuple<ivec2, ivec2, ItemType>> slots { 
  //   { pos + ivec2(80, 716), ivec2(112, 112), ITEM_ARMOR}, 
  //   { pos + ivec2(80, 716), ivec2(112, 112), ITEM_ARMOR}, 
  //   { pos + ivec2(80, 716), ivec2(112, 112), ITEM_ARMOR}, 
  // };

  // for (int i = 0; i < 3; i++) {
  //   const auto& [slot, dimensions, type] = slots[i];

  //   int left = slot.x;
  //   int right = left + dimensions.x;
  //   int top = slot.y;
  //   int bottom = top + dimensions.y;
  //   
  //   int equipment_id = passive_items[i];

  //   if (IsMouseInRectangle(left, right, bottom, top)) {
  //     if (!lft_click_ && selected_item_ != 0) { // Drop.
  //       const ItemData& item_data = resources_->GetItemData()[selected_item_];
  //       if (item_data.type != type) continue;
  //       passive_items[i] = selected_item_;
  //       selected_item_ = 0;
  //       selected_qnty_ = 0;
  //     } else if (lft_click_ && equipment_id != 0 && selected_item_ == 0) { // Drag.
  //       dragged_item.origin = ITEM_ORIGIN_EQUIPMENT;
  //       selected_item_ = equipment_id;
  //       selected_qnty_ = 1;
  //       passive_items[i] = 0;
  //       old_pos_x_ = i;
  //     }
  //   }

  //   if (equipment_id == 0) {
  //     continue;
  //   }

  //   const ItemData& item = resources_->GetItemData()[equipment_id];

  //   const string& icon = item.icon;
  //   draw_2d_->DrawImage(item.icon, left, top, 112, 112, 1.0);
  // }
}

void Inventory::DrawStats(const ivec2& pos) {
  auto player = resources_->GetPlayer();

  shared_ptr<Configs> configs = resources_->GetConfigs();

  draw_2d_->DrawText(
    boost::lexical_cast<string>(configs->dungeon_level),
    pos.x + 262, 
    kWindowHeight - (pos.y + 90), vec4(1), 
    1.0, false,  "avenir_light_oblique");

  draw_2d_->DrawText(boost::lexical_cast<string>(configs->max_dungeon_level),
    pos.x + 316, 
    kWindowHeight - (pos.y + 56), vec4(1), 
    1.0, false,  "avenir_light_oblique");

  draw_2d_->DrawText(
    boost::lexical_cast<string>(player->max_life), 
    pos.x + 140, 
    kWindowHeight - (pos.y + 174), 
    vec4(1), 
    1.0, false,  "avenir_light_oblique");

  draw_2d_->DrawText(
    boost::lexical_cast<string>(player->max_mana), 
    pos.x + 164, 
    kWindowHeight - (pos.y + 215), 
    vec4(1), 
    1.0, false,  "avenir_light_oblique");

  draw_2d_->DrawText(
    boost::lexical_cast<string>(configs->armor_class),
    pos.x + 180, 
    kWindowHeight - (pos.y + 262), 
    vec4(1), 
    1.0, false,  "avenir_light_oblique");

  draw_2d_->DrawText(
    boost::lexical_cast<string>(configs->gold),
    pos.x + 164, 
    kWindowHeight - (pos.y + 308), 
    vec4(1), 
    1.0, false,  "avenir_light_oblique");
}

void Inventory::DrawItems(const ivec2& pos) {
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  shared_ptr<Configs> configs = resources_->GetConfigs();

  int (&active_items)[3] = configs->active_items;
  for (int x = 0; x < 3; x++) {
    int item_id = active_items[x];
    if (item_id == 0) continue;

    const string& icon = item_data[item_id].icon;
  
    int size = 100;
    int left = pos.x + 80 + x * (size + 60);
    int right = pos.y + left + size;
    int top = 460;
    int bottom = top + size;

    if (IsMouseInRectangle(left, right, bottom, top)) {
      // Drag existing item.
      if (lft_click_ && selected_item_ == 0) {
        if (item_id > 0) {
          dragged_item.origin = ITEM_ORIGIN_ACTIVE;
          dragged_item.item_id = item_id;
          selected_item_ = item_id;
          selected_qnty_ = 1;

          old_pos_x_ = x;
          old_pos_y_ = -1;
          hold_offset_x_ = mouse_x_ - left;
          hold_offset_y_ = mouse_y_ - top;
          active_items[x] = 0;
        }
      }
    }

    draw_2d_->DrawImage(icon, left, top, size, size, 1.0);
  }

  int (&passive_items)[3] = configs->passive_items;
  for (int x = 0; x < 3; x++) {
    int item_id = passive_items[x];
    if (item_id == 0) continue;

    const string& icon = item_data[item_id].icon;
  
    int size = 100;
    int left = pos.x + 80 + x * (size + 60);
    int right = pos.y + left + size;
    int top = 660;
    int bottom = top + size;

    if (IsMouseInRectangle(left, right, bottom, top)) {
      // Drag existing item.
      if (lft_click_ && selected_item_ == 0) {
        if (item_id > 0) {
          dragged_item.origin = ITEM_ORIGIN_PASSIVE;
          dragged_item.item_id = item_id;
          selected_item_ = item_id;
          selected_qnty_ = 1;

          old_pos_x_ = x;
          old_pos_y_ = -1;
          hold_offset_x_ = mouse_x_ - left;
          hold_offset_y_ = mouse_y_ - top;
          passive_items[x] = 0;
        }
      }
    }

    draw_2d_->DrawImage(icon, left, top, size, size, 1.0);
  }
}

void Inventory::DrawStoreItems(const ivec2& pos) {
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&active_items)[3] = configs->active_items;
  int (&passive_items)[3] = configs->passive_items;
  int (&store)[6] = configs->store;

  for (int x = 0; x < 6; x++) {
    int item_id = store[x];
    if (item_id == 0) continue;

    const string& icon = item_data[item_id].icon;
 
    int x_ = x % 3; 
    int y_ = x / 3; 

    int size = 100;
    int left = pos.x + 660 + x_ * (size + 60);
    int right = pos.y + left + size;
    int top = 155 + y_ * (size + 60);
    int bottom = top + size;

    if (IsMouseInRectangle(left, right, bottom, top) && lft_click_) {
      const int price = item_data[item_id].price;
      if (configs->gold < price) continue;

      if (resources_->InsertItemInInventory(item_id, 1)) {
        configs->gold -= price;
        store[x] = 0;
      }
    }

    draw_2d_->DrawImage(icon, left, top, size, size, 1.0);
  }
}

void Inventory::DrawOverlay(const ivec2& pos) {
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[10][5] = configs->item_matrix;
  int (&item_quantities)[10][5] = configs->item_quantities;

  auto particle_type = resources_->GetParticleTypeByName(crystal_combination_anim_name_);
  int frame = int(crystal_combination_frame_);
  float w = 1.0 / float(particle_type->grid_size);
  float x = (frame % particle_type->grid_size) * w;
  float y = 0.875 - (frame / particle_type->grid_size) * w;
  if (frame >= 0) {
    draw_2d_->DrawImage("crystal_combination", crystal_combination_pos_.x, 
      crystal_combination_pos_.y, 128, 128, 1.0, vec2(x, y), vec2(w, w)); 
  }

  particle_type = resources_->GetParticleTypeByName("level-up");
  frame = int(crystal_combination_frame_);
  w = 1.0 / float(particle_type->grid_size);
  x = (frame % particle_type->grid_size) * w;
  y = 0.875 - (frame / particle_type->grid_size) * w;
  if (frame >= 0) {
    draw_2d_->DrawImage("level_up", 720 - 128,
      450 - 128, 256, 256, 1.0, vec2(x, y), vec2(w, w)); 
  }
}

void Inventory::DrawInventory() {
  ivec2 pos = vec2(win_x_, win_y_) + inventory_pos_;
  draw_2d_->DrawImage("inventory_stats_screen", pos.x, pos.y, 900, 
    900, 1.0);

  DrawStats(pos);
  DrawEquipment(pos);
  DrawItems(pos);
  DrawOverlay(pos);

  if (state_ != INVENTORY_ITEMS) return;
  
  if (IsMouseInRectangle(win_x_ + 572, win_x_ + 900, win_y_ + 291, win_y_ + 0) && 
    lft_click_) {
    state_ = INVENTORY_SPELLBOOK;

    inventory_pos_ = vec2(0, 0);
    inventory_pos_start_ = vec2(0, 0);
    inventory_pos_target_ = vec2(-562, 0);
    inventory_animation_start_ = glfwGetTime();

    item_description_screen_pos_ = vec2(0, 0);
    item_description_screen_pos_start_ = vec2(0, 0);
    item_description_screen_pos_target_ = vec2(0, 589);
    item_description_screen_animation_start_ = glfwGetTime();

    spell_description_pos_ = vec2(0, 0);
    spell_description_pos_start_ = vec2(0, 0);
    spell_description_pos_target_ = vec2(358, 0);
    spell_description_animation_start_ = glfwGetTime();

    throttle_ = 20;
  }
}

void Inventory::DrawContextPanel(int x, int y, const string& name, 
  const string& description) {
  draw_2d_->DrawText(name, x + 131, kWindowHeight - (y + 40), 
    vec4(1, 0.69, 0.23, 1), 1.0, true, "avenir_light_oblique");

  vector<string> words;
  boost::split(words, description, boost::is_any_of(" "));

  string line = "";
  const int kMaxLineSize = 25;
  for (const auto& w : words) {
    if (line.empty()) {
      line = w;
      continue;
    }

    if (line.size() + w.size() < kMaxLineSize) {
      line += " " + w;
      continue;
    }

    draw_2d_->DrawText(line, x + 20, kWindowHeight - (y + 70), 
      vec4(1), 1.0, false, "avenir_light_oblique");
    y += 20;

    line = w;
  }

  if (line.size() > 0) {
    draw_2d_->DrawText(line, x + 20, kWindowHeight - (y + 70), 
      vec4(1), 1.0, false, "avenir_light_oblique");
  }
}

// struct ParticleType {
//   int id;
//   string name;
//   ParticleBehavior behavior = PARTICLE_FALL;
//   int grid_size;
//   int first_frame;
//   int num_frames;
//   int keep_frame = 1;
//   string mesh = "";
//   GLuint texture_id = 0;
// };

void Inventory::DrawAnimation(const string& animation_name, const ivec2& pos, 
  int frame) {
  auto particle_type = resources_->GetParticleTypeByName(animation_name);

  float w = 1.0 / float(particle_type->grid_size);
  float x = (frame % particle_type->grid_size) * w;
  float y = (frame / particle_type->grid_size) * w;

  // draw_2d_->DrawImage("item_combination", pos.x, pos.y, 64, 64, 1.0, vec2(x, y), vec2(w, w));

  // draw_2d_->DrawImage("inventory_item_description_screen", pos.x, pos.y, 64, 64, 1.0, vec2(0, 0), vec2(1, 1));

  draw_2d_->DrawImage("black", pos.x, pos.y, 200, 200, 1.0);
}

void Inventory::DrawItemDescriptionScreen() {
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[10][5] = configs->item_matrix;

  ivec2 pos = vec2(win_x_, win_y_) + item_description_screen_pos_;
  if (state_ == INVENTORY_STORE) {
    pos = vec2(win_x_, win_y_) + store_pos_;
  } else {
    draw_2d_->DrawImage("inventory_item_description_screen", pos.x, 
      pos.y, 900, 900, 1.0);
  }

  if (selected_item_ != 0) {
    const ItemData& item_data = resources_->GetItemData()[selected_item_];
    const string& description = resources_->GetString(item_data.description);
    if (state_ == INVENTORY_STORE) {
      draw_2d_->DrawImage("black", pos.x + 789, pos.y + 105, 128, 128, 1.0); 
      draw_2d_->DrawImage(item_data.image, pos.x + 789, pos.y + 105, 128, 128, 1.0); 
      DrawContextPanel(pos.x + 725, pos.y + 263, item_data.name, description);
    } else {
      draw_2d_->DrawImage("black", pos.x + 667, pos.y + 397, 128, 128, 1.0); 
      draw_2d_->DrawImage(item_data.image, pos.x + 667, pos.y + 397, 128, 128, 1.0); 
      DrawContextPanel(pos.x + 615, pos.y + 568, item_data.name, description);
    }
    return;
  }

  for (int x = 0; x < 10; x++) {
    for (int y = 0; y < 5; y++) {
      int item_id = item_matrix[x][y];

      int left = pos.x + 34 + kTileSize * x;
      int right = left + 46;
      int top = pos.y + 557 + kTileSize * y;
      int bottom = top + 46;
      if (IsMouseInRectangle(left, right, bottom, top) && item_id != 0) {
        if (item_id == -1) {
          ivec2 pos;
          item_id = WhichItem(item_matrix, x, y, pos);
        }

        const ItemData& item_data = resources_->GetItemData()[item_id];
        const string& description = resources_->GetString(item_data.description);

        if (state_ == INVENTORY_STORE) {
          draw_2d_->DrawImage("black", pos.x + 789, pos.y + 105, 128, 128, 1.0); 
          draw_2d_->DrawImage(item_data.image, pos.x + 789, pos.y + 105, 128, 128, 1.0); 
          DrawContextPanel(pos.x + 725, pos.y + 263, item_data.name, description);
        } else {
          draw_2d_->DrawImage("black", pos.x + 667, pos.y + 397, 128, 128, 1.0); 
          draw_2d_->DrawImage(item_data.image, pos.x + 667, pos.y + 397, 128, 128, 1.0); 
          DrawContextPanel(pos.x + 615, pos.y + 568, item_data.name, description);
        }

        if (rgt_click_) {
          UseItem(x, y);
        }
      }
    }
  }

  int (&equipment)[4] = configs->equipment;
  vector<tuple<ivec2, ivec2, ItemType>> slots { 
    { pos + ivec2(83, 405), ivec2(61, 61), ITEM_RING  }, 
    { pos + ivec2(182, 405), ivec2(61, 61), ITEM_RING  }, 
    { pos + ivec2(381, 381), ivec2(112, 112), ITEM_ORB   }, 
    { pos + ivec2(379, 153), ivec2(136, 171), ITEM_ARMOR }, 
  };
  for (int i = 0; i < 4; i++) {
    const auto& [slot, dimensions, type] = slots[i];

    int left = slot.x;
    int right = left + dimensions.x;
    int top = slot.y;
    int bottom = top + dimensions.y;

    int equipment_id = equipment[i];
    if (IsMouseInRectangle(left, right, bottom, top)) {
      const ItemData& item_data = resources_->GetItemData()[equipment_id];
      const string& description = resources_->GetString(item_data.description);

      if (state_ == INVENTORY_STORE) {
        draw_2d_->DrawImage("black", pos.x + 789, pos.y + 105, 128, 128, 1.0); 
        draw_2d_->DrawImage(item_data.image, pos.x + 789, pos.y + 105, 128, 128, 1.0); 
        DrawContextPanel(pos.x + 725, pos.y + 263, item_data.name, description);
      } else {
        draw_2d_->DrawImage("black", pos.x + 667, pos.y + 397, 128, 128, 1.0); 
        draw_2d_->DrawImage(item_data.image, pos.x + 667, pos.y + 397, 128, 128, 1.0); 
        DrawContextPanel(pos.x + 615, pos.y + 568, item_data.name, description);
      }
    }
  }

  if (state_ == INVENTORY_STORE) {
    int (&store)[6] = configs->store;
    for (int x = 0; x < 6; x++) {
      int item_id = store[x];

      int x_ = x % 3; 
      int y_ = x / 3; 

      int size = 100;
      int left = pos.x + 660 + x_ * (size + 60);
      int right = pos.y + left + size;
      int top = 155 + y_ * (size + 60);
      int bottom = top + size;
      if (IsMouseInRectangle(left, right, bottom, top) && item_id != 0) {
        const ItemData& item_data = resources_->GetItemData()[item_id];
        const string& description = resources_->GetString(item_data.description);

        DrawContextPanel(pos.x + 660, pos.y + 463, item_data.name, description);
      }
    }
  }
}

// void Inventory::DrawStoreItemDescription(const ivec2& pos) {
//   unordered_map<int, ItemData>& item_data = resources_->GetItemData();
//   shared_ptr<Configs> configs = resources_->GetConfigs();
//   int (&item_matrix)[10][5] = configs->item_matrix;
//   int (&store_matrix)[10][5] = configs->store_matrix;
// 
//   // Inventory.
//   for (int x = 0; x < 10; x++) {
//     for (int y = 0; y < 5; y++) {
//       const int item_id = item_matrix[x][y];
// 
//       int left = pos.x + 34 + kTileSize * x;
//       int right = left + 46;
//       int top = pos.y + 557 + kTileSize * y;
//       int bottom = top + 46;
//       if (IsMouseInRectangle(left, right, bottom, top) && item_id != 0) {
//         const ItemData& item_data = resources_->GetItemData()[item_id];
//         const string& description = resources_->GetString(item_data.description);
// 
//         draw_2d_->DrawImage("black", pos.x + 789, pos.y + 105, 128, 128, 1.0); 
//         draw_2d_->DrawImage(item_data.image, pos.x + 789, pos.y + 105, 128, 128, 1.0); 
// 
//         DrawContextPanel(pos.x + 725, pos.y + 263, item_data.name, description);
//       }
//     }
//   }
//  
//   // Equipment.
//   int (&equipment)[4] = configs->equipment;
//   vector<tuple<ivec2, ivec2, ItemType>> slots { 
//     { pos + ivec2(83, 405), ivec2(61, 61), ITEM_RING  }, 
//     { pos + ivec2(182, 405), ivec2(61, 61), ITEM_RING  }, 
//     { pos + ivec2(381, 381), ivec2(112, 112), ITEM_ORB   }, 
//     { pos + ivec2(379, 153), ivec2(136, 171), ITEM_ARMOR }, 
//   };
//   for (int i = 0; i < 4; i++) {
//     const auto& [slot, dimensions, type] = slots[i];
// 
//     int left = slot.x;
//     int right = left + dimensions.x;
//     int top = slot.y;
//     int bottom = top + dimensions.y;
// 
//     int equipment_id = equipment[i];
//     if (IsMouseInRectangle(left, right, bottom, top)) {
//       const ItemData& item_data = resources_->GetItemData()[equipment_id];
//       const string& description = resources_->GetString(item_data.description);
// 
//       draw_2d_->DrawImage("black", pos.x + 789, pos.y + 105, 128, 128, 1.0); 
//       draw_2d_->DrawImage(item_data.image, pos.x + 789, pos.y + 105, 128, 128, 1.0); 
// 
//       DrawContextPanel(pos.x + 725, pos.y + 263, item_data.name, description);
//     }
//   }
// 
//   // Store.
//   for (int x = 0; x < 10; x++) {
//     for (int y = 0; y < 5; y++) {
//       const int item_id = store_matrix[x][y];
// 
//       int left = pos.x + 604 + kTileSize * x;
//       int right = left + 46;
//       int top = pos.y + 557 + kTileSize * y;
//       int bottom = top + 46;
//       if (IsMouseInRectangle(left, right, bottom, top) && item_id != 0) {
//         const ItemData& item_data = resources_->GetItemData()[item_id];
//         const string& description = resources_->GetString(item_data.description);
// 
//         draw_2d_->DrawImage("black", pos.x + 789, pos.y + 105, 128, 128, 1.0); 
//         draw_2d_->DrawImage(item_data.image, pos.x + 789, pos.y + 105, 128, 128, 1.0); 
// 
//         DrawContextPanel(pos.x + 725, pos.y + 263, item_data.name, description);
//       }
//     }
//   }
// }


void Inventory::UseItem(int x, int y) {
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[10][5] = configs->item_matrix;

  int item_id = item_matrix[x][y];
  resources_->LearnSpell(item_id);

  item_matrix[x][y] = 0;
  ivec2 pos = vec2(win_x_, win_y_) + inventory_pos_;
  int left = pos.x + 34 + kTileSize * x;
  int top = pos.y + 547 + kTileSize * y;
  crystal_combination_anim_name_ = "particle-smoke-0";
  crystal_combination_pos_ = ivec2(left - 40, top - 27);
  crystal_combination_frame_ = 0;
}

void Inventory::DrawSpellbook() {
  ivec2 pos = vec2(win_x_, win_y_) + spellbook_pos_;
  draw_2d_->DrawImage("inventory_spell_graph", pos.x, pos.y, 900, 900, 1.0);

  if (state_ != INVENTORY_SPELLBOOK) return;

  // Go back to inventory.
  if ((IsMouseInRectangle(win_x_, win_x_ + 10, win_y_ + 900, win_y_ + 0) ||
    IsMouseInRectangle(win_x_, win_x_ + 900, win_y_ + 900, win_y_ + 890)) &&
    lft_click_) {
    state_ = INVENTORY_ITEMS;

    inventory_pos_start_ = inventory_pos_;
    inventory_pos_target_ = vec2(0, 0);
    inventory_animation_start_ = glfwGetTime();

    item_description_screen_pos_start_ = item_description_screen_pos_;
    item_description_screen_pos_target_ = vec2(0, 0);
    item_description_screen_animation_start_ = glfwGetTime();

    spell_description_pos_start_ = spell_description_pos_;
    spell_description_pos_target_ = vec2(0, 0);
    spell_description_animation_start_ = glfwGetTime();

    throttle_ = 20;
  }

  unordered_map<int, shared_ptr<ArcaneSpellData>>& arcane_spell_data = 
    resources_->GetArcaneSpellData();

  shared_ptr<ArcaneSpellData> selected_spell = nullptr;
  for (auto [id, spell] : arcane_spell_data) {
    if (!spell->learned) continue;
    if (spell->spell_id == 9) continue;

    // TODO: draw with mask.
    ivec2 image_pos = pos + spell->spell_graph_pos;
    const ItemData& item_data = resources_->GetItemData()[spell->item_id];
    draw_2d_->DrawImage(item_data.image, image_pos.x - 19, image_pos.y - 20, 49, 49, 1.0); 
  }
}

void Inventory::DrawSpellDescription() {
  ivec2 spellbook_pos = vec2(win_x_, win_y_) + spellbook_pos_;

  ivec2 pos = vec2(win_x_, win_y_) + spell_description_pos_;
  draw_2d_->DrawImage("inventory_spell_description", pos.x, pos.y, 900, 900, 1.0);

  unordered_map<int, shared_ptr<ArcaneSpellData>>& arcane_spell_data = 
    resources_->GetArcaneSpellData();

  unordered_map<int, ItemData>& item_data = resources_->GetItemData();

  shared_ptr<ArcaneSpellData> selected_spell = nullptr;
  for (auto [id, spell] : arcane_spell_data) {
    if (!spell->learned) continue;

    string item_image_name = item_data[spell->item_id].image;
    if (IsMouseInCircle(spellbook_pos + spell->spell_graph_pos, 25)) {
      draw_2d_->DrawImage(item_image_name, pos.x + 577, pos.y + 137, 128, 128, 
        1.0); 
      draw_2d_->DrawImage(spell->image_name, pos.x + 731, pos.y + 136, 128, 128, 
        1.0); 

      const string& description = resources_->GetString(spell->description);
      DrawContextPanel(pos.x + 595, pos.y + 312, spell->name, description);

      if (lft_click_ && selected_item_ == 0) {
        selected_item_ = spell->item_id;
        selected_qnty_ = 0;
        old_pos_x_ = -1;
        old_pos_y_ = -1;
        hold_offset_x_ = -25;
        hold_offset_y_ = -25;
        dragged_item.origin = ITEM_ORIGIN_SPELL_GRAPH;
        dragged_item.item_id = spell->item_id;
      }
    }
  }

  // Spell shot.
  int left = spellbook_pos.x + 38;
  int right = left + 76;
  int top = spellbook_pos.y + 38;
  int bottom = top + 76;
  if (IsMouseInRectangle(left, right, bottom, top)) {
    auto spell = arcane_spell_data[9];

    draw_2d_->DrawImage(spell->image_name, pos.x + 731, pos.y + 136, 128, 128, 
      1.0); 

    const string& description = resources_->GetString(spell->description);
    DrawContextPanel(pos.x + 595, pos.y + 312, spell->name, description);

    if (lft_click_ && selected_item_ == 0) {
      selected_item_ = spell->item_id;
      selected_qnty_ = 0;
      old_pos_x_ = -1;
      old_pos_y_ = -1;
      hold_offset_x_ = -25;
      hold_offset_y_ = -25;
      dragged_item.origin = ITEM_ORIGIN_SPELL_GRAPH;
      dragged_item.item_id = spell->item_id;
    }
  }

}

void Inventory::DrawStore(const Camera& camera, int win_x, int win_y, 
  GLFWwindow* window) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&craft_table)[5] = configs->craft_table;
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();

  ivec2 pos = vec2(win_x_, win_y_) + store_pos_;
  draw_2d_->DrawImage("store", pos.x, pos.y, 1226, 1226, 1.0);

  DrawStoreItems(pos);
  // DrawStoreItemDescription(pos);
  DrawItemDescriptionScreen();
}

void Inventory::NextPhrase(GLFWwindow* window, const string& next_phrase_name) {
  shared_ptr<CurrentDialog> current_dialog = resources_->GetCurrentDialog();
  const Phrase& cur_phrase = current_dialog->dialog->phrases[current_dialog->current_phrase];
  const string dialog_name = current_dialog->dialog->name;
  const int num_phrases = current_dialog->dialog->phrases.size();
  const int num_options = cur_phrase.options.size();
  ObjPtr npc = current_dialog->npc;

  // Run events.
  const string& phrase_name = 
    current_dialog->dialog->phrases[current_dialog->current_phrase].name;
  if (current_dialog->dialog->on_finish_phrase_events.find(phrase_name) != 
    current_dialog->dialog->on_finish_phrase_events.end()) {
  }
  if (phrase_name == "buy-stuff") {
    resources_->GetConfigs()->show_store = true;
  }

  if (next_phrase_name.empty()) {
    if (current_dialog->current_phrase < num_phrases - 1) {
      current_dialog->current_phrase++;
      current_dialog->processed_animation = false;
    } else {
      if (current_dialog->on_finish_dialog_events.find(dialog_name) != 
        current_dialog->on_finish_dialog_events.end()) {

      }
      npc->ClearActions();

      enabled = false; 
      shared_ptr<CurrentDialog> current_dialog = resources_->GetCurrentDialog();
      current_dialog->enabled = false;
      glfwSetCursorPos(window_, 0, 0);
      
      glfwSetCursorPos(window, 0, 0);
      resources_->SetGameState(STATE_GAME);
    }
  } else {
    bool found = false;
    for (int i = 0; i < current_dialog->dialog->phrases.size(); i++) {
      const Phrase& phrase = current_dialog->dialog->phrases[i];
      if (phrase.name == next_phrase_name) {
        current_dialog->current_phrase = i;
        found = true;
        break;
      }
    }
    if (!found) {
      throw runtime_error(string("Dialog with name ") + next_phrase_name +
        " does not exist.");
    }
  }
}

void Inventory::DrawDialog(GLFWwindow* window) {
  shared_ptr<CurrentDialog> current_dialog = resources_->GetCurrentDialog();
  const Phrase& cur_phrase = current_dialog->dialog->phrases[current_dialog->current_phrase];
  int num_phrases = current_dialog->dialog->phrases.size();
  int num_options = cur_phrase.options.size();

  ObjPtr npc = current_dialog->npc;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && throttle_ < 0) {
    throttle_ = 10;

    if (num_options > 0) {
      if (++cursor_pos_ > num_options - 1) cursor_pos_ = 0;
    }
  } else if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && throttle_ < 0) {
    throttle_ = 10;

    if (num_options > 0) {
      if (--cursor_pos_ < 0) cursor_pos_ = num_options - 1;
    }
  } else if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS && throttle_ < 0) {
    if (num_options > 0) {
      if (cursor_pos_ > cur_phrase.options.size()) {
        throw runtime_error("Dialog option beyond scope.");
      }
      NextPhrase(window, get<0>(cur_phrase.options[cursor_pos_]));
    } else {
      NextPhrase(window, "");
    }
  }

  if (npc && !current_dialog->processed_animation) {
    npc->ClearActions();
    npc->LookAt(resources_->GetPlayer()->position);
    string animation_name = current_dialog->dialog->phrases[current_dialog->current_phrase].animation;
    npc->actions.push(make_shared<AnimationAction>(animation_name));
    current_dialog->processed_animation = true;
  }

  int pos_y = 420;
  draw_2d_->DrawImage("transparent_gray", 220, pos_y, 700, 240, 1.0);

  vec2 pos1 = vec2(225, pos_y + 5);
  vec2 pos2 = vec2(225 + 690, pos_y + 235);
  draw_2d_->DrawLine(pos1, vec2(pos2.x, pos1.y), 1, vec3(0.6));
  draw_2d_->DrawLine(pos1, vec2(pos1.x, pos2.y), 1, vec3(0.6));
  draw_2d_->DrawLine(vec2(pos2.x, pos1.y), pos2, 1, vec3(0.6));
  draw_2d_->DrawLine(vec2(pos1.x, pos2.y), pos2, 1, vec3(0.6));

  pos_y += 50;

  string phrase = current_dialog->dialog->phrases[current_dialog->current_phrase].content;

  vector<string> words;
  boost::split(words, phrase, boost::is_any_of(" \n\r\t"));

  int pos_x = 320;

  string line = "";
  const int kMaxLineSize = 72;
  for (const auto& w : words) {
    if (w.empty()) continue;

    if (line.empty()) {
      line = w;
      continue;
    }

    if (line.size() + w.size() < kMaxLineSize) {
      line += " " + w;
      continue;
    }

    draw_2d_->DrawText(line, pos_x, kWindowHeight - pos_y, vec4(1), 1.0, false, 
      "avenir_light_oblique");
    pos_y += 20;

    line = w;
  }

  if (!line.empty()) {
    draw_2d_->DrawText(line, pos_x, kWindowHeight - pos_y, vec4(1), 1.0, false, 
      "avenir_light_oblique");
    pos_y += 20;
  }

  int pos = 0;

  pos_y += 40;
  for (const auto& [next, content] : cur_phrase.options) {
    if (cursor_pos_ == pos) {
      draw_2d_->DrawImage("white_circle", pos_x - 24, pos_y - 13, 16, 16, 1.0);
    }

    draw_2d_->DrawText(content, pos_x, kWindowHeight - pos_y, vec4(1), 1.0, false, 
      "avenir_light_oblique");
    pos_y += 30;
    pos++;
  }
}

void Inventory::DrawSpellSelectionInterface(const Camera& camera, int win_x, int win_y, 
  GLFWwindow* window) {
  // shared_ptr<Configs> configs = resources_->GetConfigs();
  // int (&spellbar)[8] = configs->spellbar;
  // int (&spellbar_quantities)[8] = configs->spellbar_quantities;

  // draw_2d_->DrawImage("spell_interface", win_x_, win_y_, 550, 550, 1.0);

  // vector<shared_ptr<ArcaneSpellData>>& arcane_spell_data = 
  //   resources_->GetArcaneSpellData();

  // shared_ptr<ArcaneSpellData> selected_spell = nullptr;
  // for (auto spell : arcane_spell_data) {
  //   if (!spell->learned) continue;

  //   ivec2 pos = ivec2(win_x_, win_y_) + spell->spell_selection_pos * 50;
  //   draw_2d_->DrawImage(spell->image_name, pos.x, pos.y, 64, 64, 1.0);

  //   if (spell_selection_cursor_.x == spell->spell_selection_pos.x &&
  //       spell_selection_cursor_.y == spell->spell_selection_pos.y) {
  //     selected_spell = spell;
  //   }
  // }

  // if (throttle_ < 0) {
  //   if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
  //     throttle_ = 15;
  //     spell_selection_cursor_ += ivec2(0, -1);
  //   } else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
  //     throttle_ = 15;
  //     spell_selection_cursor_ += ivec2(0, +1);
  //   } else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
  //     throttle_ = 15;
  //     spell_selection_cursor_ += ivec2(-1, 0);
  //   } else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
  //     throttle_ = 15;
  //     spell_selection_cursor_ += ivec2(+1, 0);
  //   } else if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS ||
  //     glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
  //     throttle_ = 15;
  //     Disable();
  //     glfwSetCursorPos(window, 0, 0);
  //     resources_->SetGameState(STATE_GAME);

  //     if (selected_spell) {
  //       spellbar[configs->selected_spell] = selected_spell->item_id;
  //       spellbar_quantities[configs->selected_spell] = 1;
  //     }
  //   } 
  // }

  // ivec2 pos = ivec2(win_x_, win_y_) + spell_selection_cursor_ * 50;
  // draw_2d_->DrawImage("selected_item", pos.x, pos.y, 64, 64, 1.0);
}

void Inventory::UpdateAnimations() {
  float seconds = glfwGetTime() - inventory_animation_start_;
  if (seconds <= inventory_animation_duration_) {
    float x = clamp(seconds / inventory_animation_duration_, 0, 1);
    float y = 1 / (1 + exp(10 * (0.5 - x)));

    vec2 step = inventory_pos_ + (inventory_pos_target_ - inventory_pos_) * y;
    inventory_pos_ += (inventory_pos_target_ - inventory_pos_) * y;
  }

  seconds = glfwGetTime() - item_description_screen_animation_start_;
  if (seconds <= item_description_screen_animation_duration_) {
    float x = clamp(seconds / item_description_screen_animation_duration_, 0, 1);
    float y = 1 / (1 + exp(10 * (0.5 - x)));

    vec2 step = item_description_screen_pos_ + (item_description_screen_pos_target_ - item_description_screen_pos_) * y;
    item_description_screen_pos_ += (item_description_screen_pos_target_ - item_description_screen_pos_) * y;
  }

  seconds = glfwGetTime() - spellbook_animation_start_;
  if (seconds <= spellbook_animation_duration_) {
    float x = clamp(seconds / spellbook_animation_duration_, 0, 1);
    float y = 1 / (1 + exp(10 * (0.5 - x)));

    vec2 step = spellbook_pos_ + (spellbook_pos_target_ - spellbook_pos_) * y;
    spellbook_pos_ += (spellbook_pos_target_ - spellbook_pos_) * y;
  }

  seconds = glfwGetTime() - spell_description_animation_start_;
  if (seconds <= spell_description_animation_duration_) {
    float x = clamp(seconds / spell_description_animation_duration_, 0, 1);
    float y = 1 / (1 + exp(10 * (0.5 - x)));

    vec2 step = spell_description_pos_ + (spell_description_pos_target_ - spell_description_pos_) * y;
    spell_description_pos_ += (spell_description_pos_target_ - spell_description_pos_) * y;
  }

  seconds = glfwGetTime() - store_animation_start_;
  if (seconds <= store_animation_duration_) {
    float x = clamp(seconds / store_animation_duration_, 0, 1);
    float y = 1 / (1 + exp(10 * (0.5 - x)));

    vec2 step = store_pos_ + (store_pos_target_ - store_pos_) * y;
    store_pos_ += (store_pos_target_ - store_pos_) * y;
  }

  if (closing > 0.0 && glfwGetTime() > closing) {
    enabled = false; 
    shared_ptr<CurrentDialog> current_dialog = resources_->GetCurrentDialog();
    current_dialog->enabled = false;
    glfwSetCursorPos(window_, 0, 0);
  }
}

void Inventory::DrawMap() {
  Dungeon& dungeon = resources_->GetDungeon(); 
  auto player = resources_->GetPlayer();
  ivec2 player_tile = dungeon.GetDungeonTile(player->position);

  if (lft_click_ && !map_dragging_) {
    map_dragging_ = true;
    map_drag_origin_ = ivec2(mouse_x_, mouse_y_);
  } else if (lft_click_ && map_dragging_) {
    map_offset_ += ivec2((vec2(mouse_x_, mouse_y_) - vec2(map_drag_origin_)));
    map_drag_origin_ = ivec2(mouse_x_, mouse_y_);
  } else {
    map_dragging_ = false;
    map_drag_origin_ = ivec2(0, 0);
  }

  if (glfwGetTime() < show_map_after_) {
    return;
  }

  unordered_map<char, string> image_map {
    { '+', "map_interface_corner" },
    { '|', "map_interface_vertical_wall" },
    { '-', "map_interface_horizontal_wall" },
    { 'o', "map_interface_vertical_arch" },
    { 'O', "map_interface_horizontal_arch" },
    { 'g', "map_interface_vertical_gate" },
    { 'G', "map_interface_horizontal_gate" },
    { 'd', "map_interface_vertical_door" },
    { 'D', "map_interface_horizontal_door" },
    { 'P', "map_interface_pillar" },
    { 'p', "map_interface_pillar" },
    { '<', "map_interface_upstairs" },
    { '>', "map_interface_downstairs" },
    { '*', "map_interface_uncharted" },
    { '^', "map_interface_horizontal_wall" },
    { '/', "map_interface_horizontal_wall" },
  };

  vec3 player_off = (player->position - kDungeonOffset - vec3(5, 0, 5)) * 1.5f;
  vec2 map_center = vec2(player_off.x, player_off.z) + vec2(map_offset_);

  vec2 dungeon_origin = vec2(720 - map_center.x, 400 - map_center.y);

  ivec2 dungeon_top_left = player_tile + ivec2(vec2(map_offset_) * 0.0667f) - ivec2(30, 20);

  char** dungeon_map = dungeon.GetDungeon();
  char** monsters_and_objs = dungeon.GetMonstersAndObjs();
  for (int y = dungeon_top_left.y; y < dungeon_top_left.y + 40; y++) {
    for (int x = dungeon_top_left.x; x < dungeon_top_left.x + 60; x++) {
      if (x < 0 || y < 0 || x >= kDungeonSize || y >= kDungeonSize) continue;

      vec2 pos = dungeon_origin + vec2(x * 15, y * 15);

      char code = dungeon_map[x][y];
      if (code != '.') {
        int relevance = dungeon.GetRelevance(ivec2(x, y));
        // draw_2d_->DrawText(boost::lexical_cast<string>(relevance),
        //   pos.x, kWindowHeight - pos.y, vec4(1), 0.5, false, 
        //   "avenir_light_oblique");
      }

      if (!dungeon.IsTileDiscovered(ivec2(x, y))) {
        code = '*';
        continue;
      }

      if (image_map.find(code) == image_map.end()) continue;

      float k = 1.0f - std::max(float(std::abs(x - dungeon_top_left.x - 30)) / 30.0f, 
        std::abs(float(y - dungeon_top_left.y - 20) / 20.0f));
      float transparency = clamp(k, 0.0f, 0.2f) * 5.0f;

      if (code == '<' || code == '>') {
        draw_2d_->DrawImage(image_map[code], pos.x, pos.y, 30, 30, transparency); 
      } else {
        draw_2d_->DrawImage(image_map[code], pos.x, pos.y, 15, 15, transparency); 
      }
    }
  }
 
  float rotation = -atan2(camera_.direction.x, camera_.direction.z);
  draw_2d_->DrawRotatedImage("map_interface_cursor", dungeon_origin.x + player_off.x, 
    dungeon_origin.y + player_off.z, 15, 15, 1.0, rotation); 

  for (ObjPtr obj : resources_->GetMovingObjects()) {
    if (!obj->IsCreature()) continue;
    vec3 spider_off = (obj->position - kDungeonOffset - vec3(5, 0, 5)) * 1.5f;

    vec3 dir = normalize(vec3(obj->speed.x, 0, obj->speed.z));
    float rotation = -atan2(dir.x, dir.z);
    draw_2d_->DrawRotatedImage("map_interface_spider", dungeon_origin.x + spider_off.x, 
      dungeon_origin.y + spider_off.z, 15, 15, 1.0, rotation); 
  }
}

void Inventory::Draw(const Camera& camera, int win_x, int win_y, 
  GLFWwindow* window) {
  if (!enabled) return;
 
  camera_ = camera;
  win_x_ = win_x;
  win_y_ = win_y;
  window_ = window;

  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[10][5] = configs->item_matrix;
  if (crystal_combination_frame_ >= 0) {
    crystal_combination_frame_ += 1.0;
    if (crystal_combination_frame_ == 32 && crystal_combination_anim_name_ == "item-combination") {
      item_matrix[crystal_after_pos_.x][crystal_after_pos_.y] = crystal_after_;
    } else if (crystal_combination_frame_ >= 64) {
      crystal_combination_frame_ = -1;
    }
  }

  UpdateMouse(window);

  UpdateAnimations();

  switch (state_) {
    case INVENTORY_ITEMS:
      DrawSpellDescription();
      DrawSpellbook();
      DrawItemDescriptionScreen();
      DrawInventory();
      DrawCogs();
      DrawSpellbar();
      break;
    case INVENTORY_STORE:
      DrawStore(camera, win_x, win_y, window);
      DrawInventory();
      break;
    case INVENTORY_SPELLBOOK:
      DrawSpellDescription();
      DrawSpellbook();
      DrawItemDescriptionScreen();
      DrawInventory();
      DrawCogs();
      DrawSpellbar();
      break;
    case INVENTORY_DIALOG:
      DrawDialog(window);
      break;
    case INVENTORY_SPELL_SELECTION:
      DrawSpellSelectionInterface(camera, win_x, win_y, window);
      break;
    case INVENTORY_MAP:
      DrawMap();
      break;
    default:
      break;
  }

  if (state_ != INVENTORY_DIALOG || state_ != INVENTORY_MAP) {
    if (!lft_click_ && selected_item_ != 0) {
      MoveItemBack();
    }

    unordered_map<int, ItemData>& item_data = resources_->GetItemData();
    if (selected_item_ != 0) {
      const ItemData& data = item_data[selected_item_];

      int off_x = 0;
      int off_y = 0;
      if (data.size.x != data.size.y) {
        int max_size = std::max(data.size.x, data.size.y);
        float padding_x = float(max_size) - float(data.size.x);
        float padding_y = float(max_size) - float(data.size.y);
        off_x = (padding_x / float(max_size)) * 32.0f;
        off_y = (padding_y / float(max_size)) * 32.0f;
      }

      draw_2d_->DrawImage(data.icon, mouse_x_ - 32 + off_x, mouse_y_ - 32 + off_y, 64, 64, 1.0); 
    }
    draw_2d_->DrawImage("cursor", mouse_x_, mouse_y_, 64, 64, 1.0);
  }

  throttle_--;
}

void Inventory::Enable(GLFWwindow* window, InventoryState state) { 
  closing = 0.0f;
  glfwSetCursorPos(window, 640 - 32, 400 + 32);
  enabled = true; 
  state_ = state; 
  spell_selection_cursor_ = ivec2(5, 5);
  throttle_ = 20;

  inventory_pos_ = vec2(-572, 0);
  inventory_pos_start_ = vec2(-572, 0);
  inventory_pos_target_ = vec2(0, 0);
  inventory_animation_start_ = glfwGetTime();

  item_description_screen_pos_ = vec2(-900, 609);
  item_description_screen_pos_start_ = vec2(-900, 609);
  item_description_screen_pos_target_ = vec2(0, 0);
  item_description_screen_animation_start_ = glfwGetTime();

  spellbook_pos_ = vec2(-900, 0);
  spellbook_pos_start_ = vec2(-900, 0);
  spellbook_pos_target_ = vec2(0, 0);
  spellbook_animation_start_ = glfwGetTime();

  spell_description_pos_ = vec2(-900, 0);
  spell_description_pos_start_ = vec2(-900, 0);
  spell_description_pos_target_ = vec2(0, 0);
  spell_description_animation_start_ = glfwGetTime();

  store_pos_ = vec2(-1160, 0);
  store_pos_start_ = vec2(-1160, 0);
  store_pos_target_ = vec2(0, 0);
  store_animation_start_ = glfwGetTime();

  show_map_after_ = glfwGetTime() + 0.5;
}

void Inventory::Disable() { 
  if (closing > 0) return;

  closing = glfwGetTime() + 0.5f;

  inventory_pos_start_ = inventory_pos_;
  inventory_pos_target_ = vec2(-575, 0);
  inventory_animation_start_ = glfwGetTime();

  item_description_screen_pos_start_ = item_description_screen_pos_;
  item_description_screen_pos_target_ = vec2(-900, 609);
  item_description_screen_animation_start_ = glfwGetTime();

  spellbook_pos_start_ = spellbook_pos_;
  spellbook_pos_target_ = vec2(-900, 0);
  spellbook_animation_start_ = glfwGetTime();

  spell_description_pos_start_ = spell_description_pos_;
  spell_description_pos_target_ = vec2(-900, 0);
  spell_description_animation_start_ = glfwGetTime();

  store_pos_start_ = store_pos_;
  store_pos_target_ = vec2(-1160, 0);
  store_animation_start_ = glfwGetTime();
}

