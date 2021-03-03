#include <boost/algorithm/string.hpp>
#include "inventory.hpp"

const int kWindowWidth = 1280;
const int kWindowHeight = 800;
const int kTileSize = 52;

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

void Inventory::DrawContextPanel(int x, int y, int item_id) {
  int width = 261;
  int height = 320;
  if (x + width > kWindowWidth) x = kWindowWidth - width;
  if (x < 0) x = 0;
  if (y + height > kWindowHeight) x = kWindowHeight - height;
  if (y < 0) y = 0;

  draw_2d_->DrawImage("context-panel", x, y, 400, 400, 1.0); 

  const ItemData& item_data = resources_->GetItemData()[item_id];
  draw_2d_->DrawText(item_data.name, x + 131, kWindowHeight - (y + 40), 
    vec3(1, 0.69, 0.23), 1.0, true, "avenir_light_oblique");

  const string& description = resources_->GetString(item_data.description);

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
      vec3(1), 1.0, false, "avenir_light_oblique");
    y += 20;

    line = w;
  }

  if (line.size() > 0) {
    draw_2d_->DrawText(line, x + 20, kWindowHeight - (y + 70), 
      vec3(1), 1.0, false, "avenir_light_oblique");
  }
}

void Inventory::DrawItemMatrix() {
  vector<ItemData>& item_data = resources_->GetItemData();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[8][7] = configs->item_matrix;

  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 7; y++) {
      int left = win_x_ + 52 + kTileSize * x;
      int right = left + 46;
      int top = win_y_ + 90 + kTileSize * y;
      int bottom = top + 46;
 
      if (IsMouseInRectangle(left, right, bottom, top)) {
        if (lft_click_ && selected_item_ == 0) {
          selected_item_ = item_matrix[x][y];
          item_matrix[x][y] = 0;
          old_pos_x_ = x;
          old_pos_y_ = y;
          hold_offset_x_ = mouse_x_ - left;
          hold_offset_y_ = mouse_y_ - top;
        }
      }

      if (!lft_click_ && selected_item_ != 0) {
        if (IsMouseInRectangle(left, right, bottom, top)) {
          if (item_matrix[x][y] == 0) {
            item_matrix[x][y] = selected_item_;
          } else {
            item_matrix[old_pos_x_][old_pos_y_] = selected_item_;
          }
          selected_item_ = 0;
          old_pos_x_ = 0;
          old_pos_y_ = 0;
        }
      }

      const int item_id = item_matrix[x][y];
      if (item_id != 0) {
        draw_2d_->DrawImage(item_data[item_id].icon, left, top, 64, 64, 1.0); 
      }

      if (rgt_click_) {
        if (IsMouseInRectangle(left, right, bottom, top)) {
          DrawContextPanel(left + kTileSize, top - 160, item_id);
        }
      }
    }
  }
}

void Inventory::DrawSpellbar() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  vector<ItemData>& item_data = resources_->GetItemData();
  int (&item_matrix)[8][7] = configs->item_matrix;
  int (&spellbar)[8] = configs->spellbar;

  for (int x = 0; x < 8; x++) {
    int top = 742;
    int bottom = top + 46;
    int left = 433 + kTileSize * x;
    int right = left + 46;

    if (IsMouseInRectangle(left, right, bottom, top)) {
      if (lft_click_ && selected_item_ == 0) {
        selected_item_ = spellbar[x];
        spellbar[x] = 0;
        old_pos_x_ = x;
        old_pos_y_ = -1;
        hold_offset_x_ = mouse_x_ - left;
        hold_offset_y_ = mouse_y_ - top;
      } else if (!lft_click_ && selected_item_ != 0) {
        if (spellbar[x] == 0) {
          spellbar[x] = selected_item_;
        } else {
          item_matrix[old_pos_x_][old_pos_y_] = selected_item_;
        }
        selected_item_ = 0;
        old_pos_x_ = 0;
        old_pos_y_ = 0;
      }
    }
  }
}

void Inventory::MoveItemBack() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[8][7] = configs->item_matrix;
  int (&spellbar)[8] = configs->spellbar;

  int top = win_y_ + 16;
  int bottom = bottom + 464;
  int left = win_x_ + 16;
  int right = left + 784;
  if (IsMouseInRectangle(left, right, bottom, top)) {
    if (old_pos_y_ == -1) {
      spellbar[old_pos_x_] = selected_item_;
    } else {
      item_matrix[old_pos_x_][old_pos_y_] = selected_item_;
    }
  } else {
    vec3 position = camera_.position + camera_.direction * 10.0f;

    // TODO: if the item is another item, drop it instead.
    resources_->CreateGameObjFromAsset("spell-crystal", position);
  }

  selected_item_ = 0;
  old_pos_x_ = 0;
  old_pos_y_ = 0;
}

void Inventory::Draw(const Camera& camera, int win_x, int win_y, 
  GLFWwindow* window) {
  if (!enabled) return;
 
  camera_ = camera;
  win_x_ = win_x;
  win_y_ = win_y;

  UpdateMouse(window);

  shared_ptr<Configs> configs = resources_->GetConfigs();
  vector<ItemData>& item_data = resources_->GetItemData();
  vector<tuple<shared_ptr<GameAsset>, int>>& inventory = 
    resources_->GetInventory();
  int (&item_matrix)[8][7] = configs->item_matrix;
  draw_2d_->DrawImage("inventory", win_x, win_y, 800, 800, 1.0);
  
  DrawItemMatrix(); 
  DrawSpellbar(); 

  if (!lft_click_ && selected_item_ != 0) {
    MoveItemBack();
  }

  if (selected_item_ != 0) {
    draw_2d_->DrawImage(item_data[selected_item_].icon, mouse_x_ - hold_offset_x_, 
      mouse_y_ - hold_offset_y_, 64, 64, 1.0); 
  }

  draw_2d_->DrawImage("cursor", mouse_x_, mouse_y_, 64, 64, 1.0);
}
