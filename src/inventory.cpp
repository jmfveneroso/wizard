#include <boost/algorithm/string.hpp>
#include "inventory.hpp"

const int kWindowWidth = 1280;
const int kWindowHeight = 800;
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

void Inventory::DrawContextPanel(int x, int y, const string& name, 
  const string& description) {
  int width = 261;
  int height = 320;
  if (x + width > kWindowWidth) x = kWindowWidth - width;
  if (x < 0) x = 0;
  if (y + height > kWindowHeight) x = kWindowHeight - height;
  if (y < 0) y = 0;

  draw_2d_->DrawImage("context-panel", x, y, 400, 400, 1.0); 

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

void Inventory::TryToCombineCrystals(int x, int y) {
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[8][7] = configs->item_matrix;
  int (&item_quantities)[8][7] = configs->item_quantities;

  int item_a = item_matrix[x][y];
  int item_b = selected_item_;

  // TODO: check if quantities == crytal max stash size.

  int combination = resources_->CrystalCombination(item_a, item_b);
  if (combination != -1) {
    const ItemData& spell_data = item_data[combination];

    item_matrix[x][y] = combination;
    item_quantities[x][y] = spell_data.max_stash;

    if (dragged_item.origin == ITEM_ORIGIN_STORE) {
      const int price = item_data[selected_item_].price;
      resources_->TakeGold(price);
    }
    selected_item_ = 0;
    selected_qnty_ = 0;
  }
}

void Inventory::DrawItemMatrix() {
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[8][7] = configs->item_matrix;
  int (&item_quantities)[8][7] = configs->item_quantities;

  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 7; y++) {
      int left = win_x_ + 52 + kTileSize * x;
      int right = left + 46;
      int top = win_y_ + 90 + kTileSize * y;
      int bottom = top + 46;
 
      if (IsMouseInRectangle(left, right, bottom, top)) {
        if (lft_click_ && selected_item_ == 0) {
          dragged_item.origin = ITEM_ORIGIN_INVENTORY;
          selected_item_ = item_matrix[x][y];
          selected_qnty_ = item_quantities[x][y];
          item_matrix[x][y] = 0;
          item_quantities[x][y] = 0;
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
              item_matrix[x][y] = selected_item_;
              item_quantities[x][y] = selected_qnty_;
              selected_item_ = 0;
              selected_qnty_ = 0;
              old_pos_x_ = 0;
              old_pos_y_ = 0;
              if (dragged_item.origin == ITEM_ORIGIN_STORE) resources_->TakeGold(price);
            } else if (item_matrix[x][y] == selected_item_) {
              const int max_stash = item_data[selected_item_].max_stash;
              if (item_quantities[x][y] < max_stash) {
                if (item_quantities[x][y] + selected_qnty_ <= max_stash) {
                  item_quantities[x][y] += selected_qnty_;
                  selected_item_ = 0;
                  selected_qnty_ = 0;
                  if (dragged_item.origin == ITEM_ORIGIN_STORE) resources_->TakeGold(price);
                } else {
                  selected_qnty_ = item_quantities[x][y] + selected_qnty_ - max_stash;
                  item_quantities[x][y] = max_stash;
                }
              }
            } else {
              TryToCombineCrystals(x, y);
            }
          }
        }
      }

      const int item_id = item_matrix[x][y];
      const int item_quantity = item_quantities[x][y];
      if (item_id != 0) {
        draw_2d_->DrawImage(item_data[item_id].icon, left, top, 64, 64, 1.0); 

        string qnty = boost::lexical_cast<string>(item_quantity);
        draw_2d_->DrawText(qnty, left + 7, kWindowHeight - (top + 25), 
              vec4(1), 1.0, false, "avenir_light_oblique");
      }
    }
  }

  // Context panel.
  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 7; y++) {
      const int item_id = item_matrix[x][y];
      const int item_quantity = item_quantities[x][y];

      int left = win_x_ + 52 + kTileSize * x;
      int right = left + 46;
      int top = win_y_ + 90 + kTileSize * y;
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

void Inventory::DrawSpellbar() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  int (&item_matrix)[8][7] = configs->item_matrix;
  int (&item_quantities)[8][7] = configs->item_quantities;
  int (&spellbar)[8] = configs->spellbar;
  int (&spellbar_quantities)[8] = configs->spellbar_quantities;

  for (int x = 0; x < 8; x++) {
    int top = 738;
    int bottom = top + 46;
    int left = 427 + kTileSize * x;
    int right = left + 46;

    if (IsMouseInRectangle(left, right, bottom, top)) {
      if (lft_click_ && selected_item_ == 0) {
        dragged_item.origin = ITEM_ORIGIN_SPELLBAR;
        selected_item_ = spellbar[x];
        selected_qnty_ = spellbar_quantities[x];
        spellbar[x] = 0;
        old_pos_x_ = x;
        old_pos_y_ = -1;
        hold_offset_x_ = mouse_x_ - left;
        hold_offset_y_ = mouse_y_ - top;
      } else if (!lft_click_ && selected_item_ != 0) {
        if (spellbar[x] == 0) {
          spellbar[x] = selected_item_;
          spellbar_quantities[x] = selected_qnty_;
          selected_item_ = 0;
          selected_qnty_ = 0;
        } else if (spellbar[x] == selected_item_) {
          const int max_stash = item_data[selected_item_].max_stash;
          if (spellbar[x] < max_stash) {
            if (spellbar_quantities[x] + selected_qnty_ <= max_stash) {
              spellbar_quantities[x] += selected_qnty_;
              selected_item_ = 0;
              selected_qnty_ = 0;
            } else {
              selected_qnty_ = spellbar_quantities[x] + selected_qnty_ - max_stash;
              spellbar_quantities[x] = max_stash;
            }
          }
        }
      }
    }
  }
}

void Inventory::MoveItemBack() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[8][7] = configs->item_matrix;
  int (&item_quantities)[8][7] = configs->item_quantities;
  int (&spellbar)[8] = configs->spellbar;
  int (&spellbar_quantities)[8] = configs->spellbar_quantities;
  int (&equipment)[7] = configs->equipment;
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
      item_quantities[old_pos_x_][old_pos_y_] = selected_qnty_;
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

void Inventory::DrawInventory(const Camera& camera, int win_x, int win_y, 
  GLFWwindow* window) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  draw_2d_->DrawImage("inventory", win_x, win_y, 800, 800, 1.0);
  
  DrawItemMatrix(); 
  DrawSpellbar(); 

  if (selected_item_ == 0 && --throttle_ < 0) {
    if (IsMouseInRectangle(win_x_ + 65, win_x_ + 105, win_y_ + 62, win_y_ + 46) && 
      lft_click_) {
      state_ = INVENTORY_STATS;
      throttle_ = 20;
    } else if (IsMouseInRectangle(win_x + 244, win_x + 287, win_y + 62, win_y + 46) && 
      lft_click_) {
      state_ = INVENTORY_SPELLBOOK;
      throttle_ = 20;
    } else if (IsMouseInRectangle(win_x + 376, win_x + 442, win_y + 62, 
      win_y + 46) && lft_click_) {
      state_ = INVENTORY_QUEST_LOG;
      throttle_ = 20;
    }
  }
}

void Inventory::DrawSpellPage() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();

  for (int i = 1; i < 5; i++) {
    if (!configs->learned_spells[i]) continue;

    // const ArcaneSpellData& spell_data = resources_->GetArcaneSpellData()[i];
    // draw_2d_->DrawImage(spell_data.image_name, 
    //   win_x_ + spell_data.image_position.x, 
    //   win_y_ + spell_data.image_position.y, spell_data.image_size.x, 
    //   spell_data.image_size.y, 1.0);

    // if (rgt_click_) {
    //   int left = win_x_ + spell_data.image_position.x;
    //   int right = left + spell_data.image_size.x;
    //   int top = win_y_ + spell_data.image_position.y;
    //   int bottom = top + spell_data.image_size.y;
    //   if (IsMouseInRectangle(left, right, bottom, top)) {
    //     const string& description = resources_->GetString(spell_data.description);
    //     DrawContextPanel(left + 64, top - 160, spell_data.name, 
    //       description);

    //     for (int j = 0; j < spell_data.formula.size(); j++) {
    //       int item_id = spell_data.formula[j];
    //       if (item_id == 0) continue;

    //       int x = left + 128 + 10;
    //       int y = top - 160 + 250;
    //       draw_2d_->DrawImage(item_data[item_id].icon, x + j * 74, y, 64, 64, 1.0); 
    //     }
    //   }
    // }
  }
}

void Inventory::DrawSpellbook() {
  draw_2d_->DrawImage("spellbook", win_x_, win_y_, 800, 800, 1.0);
  DrawSpellPage();
  if (--throttle_ < 0 && selected_item_ == 0) {
    if (IsMouseInRectangle(win_x_ + 65, win_x_ + 105, win_y_ + 62, win_y_ + 46) && 
      lft_click_) {
      state_ = INVENTORY_STATS;
      throttle_ = 20;
    } else if (
      IsMouseInRectangle(win_x_ + 132, win_x_ + 216, win_y_ + 62, win_y_ +  
        46) && lft_click_) {
      state_ = INVENTORY_ITEMS;
      throttle_ = 20;
    } else if (IsMouseInRectangle(win_x_ + 376, win_x_ + 442, win_y_ + 62, 
      win_y_ + 46) && lft_click_) {
      state_ = INVENTORY_QUEST_LOG;
      throttle_ = 20;
    }
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

void Inventory::DrawEquipment(const Camera& camera, int win_x, int win_y, 
  GLFWwindow* window) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&equipment)[7] = configs->equipment;
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();

  draw_2d_->DrawImage("craft", win_x_ + 600, win_y_ + 30, 512, 512, 1.0);

  vector<tuple<ivec2, ItemType>> slots { 
    { ivec2(win_x_ + 631, win_y_ + 76),  ITEM_AMULET }, // Amulet.
    { ivec2(win_x_ + 631, win_y_ + 320), ITEM_RING   }, // Ring 1.
    { ivec2(win_x_ + 631, win_y_ + 378), ITEM_RING   }, // Ring 2.
    { ivec2(win_x_ + 655, win_y_ + 158), ITEM_WEAPON }, // Weapon.
    { ivec2(win_x_ + 749, win_y_ + 77),  ITEM_HELMET }, // Helmet.
    { ivec2(win_x_ + 749, win_y_ + 183), ITEM_ARMOR  }, // Armor.
    { ivec2(win_x_ + 749, win_y_ + 381), ITEM_BOOTS  }, // Boots.
  };

  for (int i = 0; i < 7; i++) {
    const auto& [slot, type] = slots[i];

    int left = slot.x;
    int right = left + 46;
    int top = slot.y;
    int bottom = top + 46;

    int equipment_id = equipment[i];
    if (IsMouseInRectangle(left, right, bottom, top)) {
      if (!lft_click_ && selected_item_ != 0) { // Drop.
        const ItemData& item_data = resources_->GetItemData()[selected_item_];
        if (item_data.type != type) continue;
        equipment[i] = selected_item_;
        selected_item_ = 0;
        selected_qnty_ = 0;
      } else if (lft_click_ && equipment_id != 0) { // Drag.
        dragged_item.origin = ITEM_ORIGIN_EQUIPMENT;
        selected_item_ = equipment_id;
        selected_qnty_ = 1;
        equipment[i] = 0;
        old_pos_x_ = i;
      }
    }

    if (equipment_id == 0) continue;

    const ItemData& item_data = resources_->GetItemData()[equipment_id];
    draw_2d_->DrawImage(item_data.icon, left, top, 64, 64, 1.0); 
  }
}

void Inventory::DrawQuestLog(GLFWwindow* window) {
  draw_2d_->DrawImage("quest_log", win_x_, win_y_, 800, 800, 1.0);
  if (--throttle_ < 0 && selected_item_ == 0) {
    if (IsMouseInRectangle(win_x_ + 65, win_x_ + 105, win_y_ + 62, win_y_ + 46) && 
      lft_click_) {
      state_ = INVENTORY_STATS;
      throttle_ = 20;
    } else if (
      IsMouseInRectangle(win_x_ + 132, win_x_ + 216, win_y_ + 62, win_y_ +  
        46) && lft_click_) {
      state_ = INVENTORY_ITEMS;
      throttle_ = 20;
    } else if (IsMouseInRectangle(win_x_ + 244, win_x_ + 287, win_y_ + 62, 
      win_y_ + 46) && lft_click_) {
      state_ = INVENTORY_SPELLBOOK;
      throttle_ = 20;
    }
  }

  unordered_map<string, shared_ptr<Quest>> quests = resources_->GetQuests();
  vector<shared_ptr<Quest>> active_quests;
  for (const auto& [quest_name, quest] : quests) {
    if (!quest->active) continue;
    active_quests.push_back(quest);
  }

  int x = win_x_;
  int y = win_y_;
  for (const auto& quest : active_quests) {
    draw_2d_->DrawText(quest->title, x + 80, kWindowHeight - (y + 130), 
      vec4(1), 1.0, false, "avenir_light_oblique");
    y += 20;
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

void Inventory::DrawStats(const Camera& camera, int win_x, int win_y, 
  GLFWwindow* window) {
  draw_2d_->DrawImage("stats", win_x_, win_y_, 800, 800, 1.0);
  if (--throttle_ < 0 && selected_item_ == 0) {
    if (
      IsMouseInRectangle(win_x_ + 132, win_x_ + 216, win_y_ + 62, win_y_ +  
        46) && lft_click_) {
      state_ = INVENTORY_ITEMS;
      throttle_ = 20;
    } else if (IsMouseInRectangle(win_x_ + 376, win_x_ + 442, win_y_ + 62, 
      win_y_ + 46) && lft_click_) {
      state_ = INVENTORY_QUEST_LOG;
      throttle_ = 20;
    }
  }

  shared_ptr<Configs> configs = resources_->GetConfigs();
  int pos_x = win_x + 70;
  int pos_y = win_y_ + 112;
  draw_2d_->DrawText(string("HP: ") + 
    boost::lexical_cast<string>(configs->max_life), pos_x, 
    kWindowHeight - pos_y, vec4(1), 
    1.0, false,  "avenir_light_oblique");

  pos_y += 30;
  draw_2d_->DrawText(string("Armor class: ") + 
    boost::lexical_cast<string>(configs->armor_class), pos_x, 
    kWindowHeight - pos_y, vec4(1), 
    1.0, false,  "avenir_light_oblique");

  pos_y += 30;
  draw_2d_->DrawText(string("Level: ") + 
    boost::lexical_cast<string>(configs->level), pos_x, 
    kWindowHeight - pos_y, vec4(1), 
    1.0, false,  "avenir_light_oblique");

  int next_level = kLevelUps[configs->level];

  pos_y += 30;
  draw_2d_->DrawText(string("Experience: ") + 
    boost::lexical_cast<string>(configs->experience) + " / " +
    boost::lexical_cast<string>(next_level), pos_x, 
    kWindowHeight - pos_y, vec4(1), 
    1.0, false,  "avenir_light_oblique");

  pos_y += 30;
  draw_2d_->DrawText(string("Skill points: ") + 
    boost::lexical_cast<string>(configs->skill_points), pos_x,
    kWindowHeight - pos_y, vec4(1), 
    1.0, false,  "avenir_light_oblique");

  pos_y += 30;
  draw_2d_->DrawText(string("Meelee: ") + 
    boost::lexical_cast<string>(DiceFormulaToStr(configs->base_damage)), pos_x, 
    kWindowHeight - pos_y, vec4(1), 
    1.0, false,  "avenir_light_oblique");

  pos_y += 30;
  draw_2d_->DrawText(string("Arcane: ") + 
    boost::lexical_cast<string>(configs->arcane_level), pos_x, 
    kWindowHeight - pos_y, vec4(1), 
    1.0, false,  "avenir_light_oblique");

  int left = pos_x;
  int right = left + 100;
  int top = pos_y - 20;
  int bottom = top + 20;
  debounce_--;
  if (lft_click_ && IsMouseInRectangle(left, right, bottom, top)) {
    if (debounce_ < 0) {
      resources_->AddSkillPoint();
    }
    debounce_ = 60;
  }
}

void Inventory::Draw(const Camera& camera, int win_x, int win_y, 
  GLFWwindow* window) {
  if (!enabled) return;
 
  camera_ = camera;
  win_x_ = win_x;
  win_y_ = win_y;

  UpdateMouse(window);

  switch (state_) {
    case INVENTORY_ITEMS:
      DrawInventory(camera, win_x, win_y, window);
      DrawEquipment(camera, win_x, win_y, window);
      break;
    case INVENTORY_STORE:
      DrawInventory(camera, win_x, win_y, window);
      DrawStore(camera, win_x, win_y, window);
      break;
    case INVENTORY_SPELLBOOK:
      DrawSpellbook();
      break;
    case INVENTORY_QUEST_LOG:
      DrawQuestLog(window);
      break;
    case INVENTORY_CRAFT:
      DrawInventory(camera, win_x, win_y, window);
      DrawEquipment(camera, win_x, win_y, window);
      break;
    case INVENTORY_STATS:
      DrawStats(camera, win_x, win_y, window);
      break;
    case INVENTORY_DIALOG:
      DrawDialog(window);
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
  glfwSetCursorPos(window, 640 - 32, 400 + 32);
  enabled = true; 
  state_ = state; 
}

void Inventory::Disable() { 
  enabled = false; 
  shared_ptr<CurrentDialog> current_dialog = resources_->GetCurrentDialog();
  current_dialog->enabled = false;
}
