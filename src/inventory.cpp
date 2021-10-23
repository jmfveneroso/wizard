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

void Inventory::TryToCombineCrystals(int x, int y) {
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
  }
}

void Inventory::DrawSpellbar() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  int (&spellbar)[8] = configs->spellbar;
  int (&spellbar_quantities)[8] = configs->spellbar_quantities;

  draw_2d_->DrawImage("spellbar", 0, 0, 1440, 1440, 1.0);

  for (int x = 0; x < 8; x++) {
    int top = 852;
    int left = 515 + 52 * x;
    int item_id = configs->spellbar[x];
    int item_quantity = configs->spellbar_quantities[x];
    if (item_id != 0) {
      auto spell = resources_->WhichArcaneSpell(item_id);
      if (spell) {
        draw_2d_->DrawImageWithMask(spell->image_name,
          "spellbar_item_mask", left, top, 44, 44, 1.0); 
      } else {
        draw_2d_->DrawImageWithMask(item_data[item_id].image, 
          "spellbar_item_mask", left, top, 44, 44, 1.0); 
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
      if ((spell && dragged_item.origin == ITEM_ORIGIN_SPELL_GRAPH) ||
        !spell) {
        if (spell)
          cout << "spell: " << spell->spell_id << endl;
        cout << "item_id: " << dragged_item.item_id << endl;
        spellbar[x] = dragged_item.item_id;
        spellbar_quantities[x] = 1;
        dragged_item.origin = ITEM_ORIGIN_NONE;
        selected_item_ = 0;
        selected_qnty_ = 0;
        old_pos_x_ = 0;
        old_pos_y_ = 0;
      }
    }
  }
}

void Inventory::MoveItemBack() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[10][5] = configs->item_matrix;
  int (&item_quantities)[10][5] = configs->item_quantities;
  int (&spellbar)[8] = configs->spellbar;
  int (&spellbar_quantities)[8] = configs->spellbar_quantities;
  int (&equipment)[4] = configs->equipment;
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();

  int top = win_y_ + 16;
  int bottom = top + 800;
  int left = win_x_ + 16;
  int right = left + 784;
  if (IsMouseInRectangle(left, right, bottom, top)) {
    if (dragged_item.origin == ITEM_ORIGIN_EQUIPMENT) {
      const ItemData& item_data = resources_->GetItemData()[selected_item_];
      equipment[old_pos_x_] = selected_item_;
    } else if (old_pos_y_ == -1) {
      spellbar[old_pos_x_] = selected_item_;
      spellbar_quantities[old_pos_x_] = selected_qnty_;
    } else if (old_pos_x_ == -1) {
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
  } else {
    // // TODO: drop item with quantity.
    // vec3 position = camera_.position + camera_.direction * 10.0f;
    // ObjPtr obj = CreateGameObjFromAsset(resources_.get(), 
    //   item_data[selected_item_].asset_name, position);
    // obj->CalculateCollisionData();
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
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&equipment)[4] = configs->equipment;
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();

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
      if (!lft_click_ && selected_item_ != 0) { // Drop.
        const ItemData& item_data = resources_->GetItemData()[selected_item_];
        if (item_data.type != type) continue;
        equipment[i] = selected_item_;
        selected_item_ = 0;
        selected_qnty_ = 0;
      } else if (lft_click_ && equipment_id != 0 && selected_item_ == 0) { // Drag.
        dragged_item.origin = ITEM_ORIGIN_EQUIPMENT;
        selected_item_ = equipment_id;
        selected_qnty_ = 1;
        equipment[i] = 0;
        old_pos_x_ = i;
      }
    }

    if (equipment_id == 0) continue;

    const ItemData& item = resources_->GetItemData()[equipment_id];

    const string& icon = item.icon;
    const ivec2& size = item.size;

    float max_side = std::max(size.x, size.y);
    draw_2d_->DrawImage(item.icon, left, top, max_side * 51, max_side * 51, 1.0);
  }
}

void Inventory::DrawStats(const ivec2& pos) {
  auto player = resources_->GetPlayer();

  shared_ptr<Configs> configs = resources_->GetConfigs();

  int next_level = kLevelUps[configs->level];
  draw_2d_->DrawText(boost::lexical_cast<string>(configs->level),
    pos.x + 206, 
    kWindowHeight - (pos.y + 56), vec4(1), 
    1.0, false,  "avenir_light_oblique");

  draw_2d_->DrawText(
    boost::lexical_cast<string>(configs->experience),
    pos.x + 162, 
    kWindowHeight - (pos.y + 90), vec4(1), 
    1.0, false,  "avenir_light_oblique");

  draw_2d_->DrawText(
    boost::lexical_cast<string>(next_level),
    pos.x + 358, 
    kWindowHeight - (pos.y + 90), vec4(1), 
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
    boost::lexical_cast<string>(player->max_stamina),
    pos.x + 200, 
    kWindowHeight - (pos.y + 262), 
    vec4(1), 
    1.0, false,  "avenir_light_oblique");

  draw_2d_->DrawText(
    boost::lexical_cast<string>(configs->armor_class),
    pos.x + 255, 
    kWindowHeight - (pos.y + 308), 
    vec4(1), 
    1.0, false,  "avenir_light_oblique");

  draw_2d_->DrawText(
    boost::lexical_cast<string>(configs->gold),
    pos.x + 110, 
    kWindowHeight - (pos.y + 864), 
    vec4(1), 
    1.0, false,  "avenir_light_oblique");
}

void Inventory::DrawItems(const ivec2& pos) {
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[10][5] = configs->item_matrix;
  int (&item_quantities)[10][5] = configs->item_quantities;

  for (int x = 0; x < 10; x++) {
    for (int y = 0; y < 5; y++) {
      int left = pos.x + 34 + kTileSize * x;
      int right = left + 46;
      int top = pos.y + 557 + kTileSize * y;
      int bottom = top + 46;
 
      if (IsMouseInRectangle(left, right, bottom, top)) {
        if (lft_click_ && selected_item_ == 0) {
          dragged_item.origin = ITEM_ORIGIN_INVENTORY;
          dragged_item.item_id = item_matrix[x][y];
          selected_item_ = item_matrix[x][y];
          selected_qnty_ = item_quantities[x][y];

          const ivec2& size = item_data[dragged_item.item_id].size;
          for (int step_x = 0; step_x < size.x; step_x++) {
            for (int step_y = 0; step_y < size.y; step_y++) {
              item_matrix[x + step_x][y + step_y] = 0;
            }
          }

          old_pos_x_ = x;
          old_pos_y_ = y;
          hold_offset_x_ = mouse_x_ - left;
          hold_offset_y_ = mouse_y_ - top;
        }
      }

      if (!lft_click_ && selected_item_ != 0) {
        if (IsMouseInRectangle(left, right, bottom, top)) {
          const int price = item_data[selected_item_].price;

          bool complete_drag = true; 
          if (dragged_item.origin == ITEM_ORIGIN_STORE) {
            // Buy.
            if (resources_->CountGold() < price) {
              complete_drag = false;
            }
          }

          if (complete_drag) {
            if (item_matrix[x][y] == 0) {
              if (resources_->CanPlaceItem(ivec2(x, y), selected_item_)) {
                item_matrix[x][y] = selected_item_;

                const ivec2& size = item_data[selected_item_].size;
                for (int step_x = 0; step_x < size.x; step_x++) {
                  for (int step_y = 0; step_y < size.y; step_y++) {
                    if (step_x == 0 && step_y == 0) continue;
                    item_matrix[x + step_x][y + step_y] = -1;
                  }
                }

                selected_item_ = 0;
                selected_qnty_ = 0;
                old_pos_x_ = 0;
                old_pos_y_ = 0;
                if (dragged_item.origin == ITEM_ORIGIN_STORE) resources_->TakeGold(price);
              }
            // } else if (item_matrix[x][y] == selected_item_) {
            //   const int max_stash = item_data[selected_item_].max_stash;
            //   if (item_quantities[x][y] < max_stash) {
            //     if (item_quantities[x][y] + selected_qnty_ <= max_stash) {
            //       item_quantities[x][y] += selected_qnty_;
            //       selected_item_ = 0;
            //       selected_qnty_ = 0;
            //       if (dragged_item.origin == ITEM_ORIGIN_STORE) resources_->TakeGold(price);
            //     } else {
            //       selected_qnty_ = item_quantities[x][y] + selected_qnty_ - max_stash;
            //       item_quantities[x][y] = max_stash;
            //     }
            //   }
            } else {
              TryToCombineCrystals(x, y);
            }
          }
        }
      }

      const int item_id = item_matrix[x][y];
      const int item_quantity = item_quantities[x][y];
      if (item_id != 0 && item_id != -1) {
        const string& icon = item_data[item_id].icon;
        const ivec2& size = item_data[item_id].size;

        float max_side = std::max(size.x, size.y);
        vec2 dimensions = vec2((float) size.x / (float) max_side, (float) size.y / (float) max_side);
        draw_2d_->DrawImage(icon, left, top, max_side * 51, max_side * 51, 1.0);
      }
    }
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
  DrawItemDescriptionScreen();

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
  draw_2d_->DrawImage("inventory_item_description_screen", pos.x, 
    pos.y, 900, 900, 1.0);

  for (int x = 0; x < 10; x++) {
    for (int y = 0; y < 5; y++) {
      const int item_id = item_matrix[x][y];

      int left = pos.x + 34 + kTileSize * x;
      int right = left + 46;
      int top = pos.y + 557 + kTileSize * y;
      int bottom = top + 46;
      if (IsMouseInRectangle(left, right, bottom, top) && item_id != 0) {
        const ItemData& item_data = resources_->GetItemData()[item_id];
        const string& description = resources_->GetString(item_data.description);

        draw_2d_->DrawImage("black", pos.x + 667, pos.y + 397, 128, 128, 1.0); 
        draw_2d_->DrawImage(item_data.image, pos.x + 667, pos.y + 397, 128, 128, 1.0); 

        DrawContextPanel(pos.x + 615, pos.y + 568, item_data.name, description);

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

      draw_2d_->DrawImage("black", pos.x + 667, pos.y + 397, 128, 128, 1.0); 
      draw_2d_->DrawImage(item_data.image, pos.x + 667, pos.y + 397, 128, 128, 1.0); 

      DrawContextPanel(pos.x + 615, pos.y + 568, item_data.name, description);
    }
  }
}

void Inventory::UseItem(int x, int y) {
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[10][5] = configs->item_matrix;

  int item_id = item_matrix[x][y];
  auto spell = resources_->WhichArcaneSpell(item_id);
  if (!spell) return;

  if (!spell->learned) {
    resources_->AddMessage(string("You learned ") + spell->name);
    spell->learned = true;
    item_matrix[x][y] = 0;

    ivec2 pos = vec2(win_x_, win_y_) + inventory_pos_;
    int left = pos.x + 34 + kTileSize * x;
    int top = pos.y + 547 + kTileSize * y;
    crystal_combination_anim_name_ = "particle-smoke-0";
    crystal_combination_pos_ = ivec2(left - 40, top - 27);
    crystal_combination_frame_ = 0;
  } else {
    resources_->AddMessage(string("You already learned ") + spell->name);
  }
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

  vector<shared_ptr<ArcaneSpellData>>& arcane_spell_data = 
    resources_->GetArcaneSpellData();

  shared_ptr<ArcaneSpellData> selected_spell = nullptr;
  for (auto spell : arcane_spell_data) {
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

  vector<shared_ptr<ArcaneSpellData>>& arcane_spell_data = 
    resources_->GetArcaneSpellData();

  unordered_map<int, ItemData>& item_data = resources_->GetItemData();

  shared_ptr<ArcaneSpellData> selected_spell = nullptr;
  for (auto spell : arcane_spell_data) {
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
  shared_ptr<ArcaneSpellData> spell_shot = nullptr;
  if (IsMouseInRectangle(left, right, bottom, top)) {
    auto spell = arcane_spell_data[3];

    draw_2d_->DrawImage(spell->image_name, pos.x + 731, pos.y + 136, 128, 128, 
      1.0); 

    const string& description = resources_->GetString(spell->description);
    DrawContextPanel(pos.x + 595, pos.y + 312, spell->name, description);
  }

}

void Inventory::DrawStore(const Camera& camera, int win_x, int win_y, 
  GLFWwindow* window) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&craft_table)[5] = configs->craft_table;
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();

  draw_2d_->DrawImage("store", win_x + 500, win_y, 800, 800, 1.0);

  int (&item_matrix)[4][7] = configs->store_items;
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 7; y++) {
      int left = win_x + 552 + kTileSize * x;
      int right = left + 46;
      int top = win_y + 90 + kTileSize * y;
      int bottom = top + 46;
 
      if (IsMouseInRectangle(left, right, bottom, top)) {
        if (lft_click_ && selected_item_ == 0) {
          selected_item_ = item_matrix[x][y];
          selected_qnty_ = item_data[selected_item_].max_stash;
          // item_matrix[x][y] = 0;
          // item_quantities[x][y] = 0;
          old_pos_x_ = -1;
          old_pos_y_ = -1;
          hold_offset_x_ = mouse_x_ - left;
          hold_offset_y_ = mouse_y_ - top;
          dragged_item.origin = ITEM_ORIGIN_STORE;
        }
      }

      if (!lft_click_ && selected_item_ != 0) {
        // TODO: sell item.
      }

      const int item_id = item_matrix[x][y];
      if (item_id != 0) {
        draw_2d_->DrawImage(item_data[item_id].icon, left, top, 64, 64, 1.0); 
      }
    }
  }

  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 7; y++) {
      const int item_id = item_matrix[x][y];

      int left = win_x + 552 + kTileSize * x;
      int right = left + 46;
      int top = win_y + 90 + kTileSize * y;
      int bottom = top + 46;

      if (rgt_click_) {
        if (IsMouseInRectangle(left, right, bottom, top)) {
          const ItemData& item_data = resources_->GetItemData()[item_id];
          const string& description = resources_->GetString(item_data.description);
          DrawContextPanel(left + kTileSize, top - 160, item_data.name, 
            description);
        }
      }
    }
  }
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
    resources_->RunScriptFn(
      current_dialog->dialog->on_finish_phrase_events[phrase_name]);
  }

  if (next_phrase_name.empty()) {
    if (current_dialog->current_phrase < num_phrases - 1) {
      current_dialog->current_phrase++;
      current_dialog->processed_animation = false;
    } else {
      if (current_dialog->on_finish_dialog_events.find(dialog_name) != 
        current_dialog->on_finish_dialog_events.end()) {
        resources_->RunScriptFn(
          current_dialog->on_finish_dialog_events[dialog_name]);
      }
      npc->ClearActions();
      Disable();
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
  if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS && throttle_ < 0) {
    throttle_ = 20;

    if (num_options > 0) {
      if (++cursor_pos_ > num_options - 1) cursor_pos_ = 0;
    } else {
      NextPhrase(window);
    }
  } else if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS && throttle_ < 0) {
    if (num_options > 0) {
      if (cursor_pos_ > cur_phrase.options.size()) {
        throw runtime_error("Dialog option beyond scope.");
      }
      NextPhrase(window, get<0>(cur_phrase.options[cursor_pos_]));
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

  draw_2d_->DrawImage("dialog", 220, pos_y, 800, 800, 1.0);

  pos_y += 100;

  string phrase = current_dialog->dialog->phrases[current_dialog->current_phrase].content;

  vector<string> words;
  boost::split(words, phrase, boost::is_any_of(" \n\r\t"));

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

    draw_2d_->DrawText(line, 300, kWindowHeight - pos_y, vec4(1), 1.0, false, 
      "avenir_light_oblique");
    pos_y += 20;

    line = w;
  }

  if (!line.empty()) {
    draw_2d_->DrawText(line, 300, kWindowHeight - pos_y, vec4(1), 1.0, false, 
      "avenir_light_oblique");
    pos_y += 20;
  }

  int pos = 0;
  for (const auto& [next, content] : cur_phrase.options) {
    if (cursor_pos_ == pos) {
      draw_2d_->DrawImage("next-dialog", 240, pos_y - 30, 64, 64, 1.0);
    }

    draw_2d_->DrawText(content, 300, kWindowHeight - pos_y, vec4(1), 1.0, false, 
      "avenir_light_oblique");
    pos_y += 20;
    pos++;
  }

  if (current_dialog->current_phrase ==
    current_dialog->dialog->phrases.size() - 1) {
    draw_2d_->DrawImage("dialog-end", 900, 620, 64, 64, 1.0);
  } else if (current_dialog->current_phrase < 
    current_dialog->dialog->phrases.size() - 1) {
    draw_2d_->DrawImage("next-dialog", 900, 620, 64, 64, 1.0);
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

  if (closing > 0.0 && glfwGetTime() > closing) {
    enabled = false; 
    shared_ptr<CurrentDialog> current_dialog = resources_->GetCurrentDialog();
    current_dialog->enabled = false;
    glfwSetCursorPos(window_, 0, 0);
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
      DrawInventory();
      DrawCogs();
      DrawSpellbar();
      break;
    case INVENTORY_STORE:
      DrawInventory();
      DrawStore(camera, win_x, win_y, window);
      break;
    case INVENTORY_SPELLBOOK:
      DrawSpellDescription();
      DrawSpellbook();
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
    default:
      break;
  }

  if (state_ != INVENTORY_DIALOG) {
    if (!lft_click_ && selected_item_ != 0) {
      MoveItemBack();
    }

    unordered_map<int, ItemData>& item_data = resources_->GetItemData();
    if (selected_item_ != 0) {
      draw_2d_->DrawImage(item_data[selected_item_].icon, mouse_x_ - hold_offset_x_, 
        mouse_y_ - hold_offset_y_, 64, 64, 1.0); 
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
}

// void Inventory::DrawQuestLog(GLFWwindow* window) {
//   draw_2d_->DrawImage("quest_log", win_x_, win_y_, 800, 800, 1.0);
//   if (--throttle_ < 0 && selected_item_ == 0) {
//     if (IsMouseInRectangle(win_x_ + 65, win_x_ + 105, win_y_ + 62, win_y_ + 46) && 
//       lft_click_) {
//       state_ = INVENTORY_STATS;
//       throttle_ = 20;
//     } else if (
//       IsMouseInRectangle(win_x_ + 132, win_x_ + 216, win_y_ + 62, win_y_ +  
//         46) && lft_click_) {
//       state_ = INVENTORY_ITEMS;
//       throttle_ = 20;
//     } else if (IsMouseInRectangle(win_x_ + 244, win_x_ + 287, win_y_ + 62, 
//       win_y_ + 46) && lft_click_) {
//       state_ = INVENTORY_SPELLBOOK;
//       throttle_ = 20;
//     }
//   }
// 
//   unordered_map<string, shared_ptr<Quest>> quests = resources_->GetQuests();
//   vector<shared_ptr<Quest>> active_quests;
//   for (const auto& [quest_name, quest] : quests) {
//     if (!quest->active) continue;
//     active_quests.push_back(quest);
//   }
// 
//   int x = win_x_;
//   int y = win_y_;
//   for (const auto& quest : active_quests) {
//     draw_2d_->DrawText(quest->title, x + 80, kWindowHeight - (y + 130), 
//       vec4(1), 1.0, false, "avenir_light_oblique");
//     y += 20;
//   }
// }

