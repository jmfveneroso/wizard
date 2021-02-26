#include "inventory.hpp"

const int kWindowWidth = 1280;
const int kWindowHeight = 800;

Inventory::Inventory(shared_ptr<Resources> asset_catalog, 
  shared_ptr<Draw2D> draw_2d) : resources_(asset_catalog), 
  draw_2d_(draw_2d) {
}

bool IsMouseInRectangle(int mouse_x, int mouse_y, int left, int right, 
  int bottom, int top) {
  return (mouse_x > left && mouse_x < right && 
         (mouse_y - 64) > bottom && (mouse_y - 64) < top);
}

void Inventory::Draw(const Camera& camera, int win_x, int win_y, GLFWwindow* window) {
  if (!enabled)
    return;

  vector<tuple<shared_ptr<GameAsset>, int>>& inventory = resources_->GetInventory();
  draw_2d_->DrawImage("inventory", win_x, - win_y, 800, 800, 1.0);
  draw_2d_->DrawImage("spell_bar", 400, -530, 600, 600, 1.0);

  double x_pos, y_pos;
  glfwGetCursorPos(window, &x_pos, &y_pos);

  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[8][7] = configs->item_matrix;

  int mouse_x = x_pos;
  int mouse_y = y_pos;
  bool mouse_lft_click = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  bool mouse_rgt_click = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
  if (mouse_x < 0) glfwSetCursorPos(window, 0, mouse_y);
  if (mouse_x > kWindowWidth) glfwSetCursorPos(window, kWindowWidth - 64, mouse_y);
  if (mouse_y < 0) glfwSetCursorPos(window, mouse_x, 0);
  if (mouse_y > kWindowHeight) glfwSetCursorPos(window, mouse_x, kWindowHeight - 64);
   
  const int tile_size = 52;
  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 7; y++) {
      int top = kWindowHeight - win_y - 154 - tile_size * y;
      int bottom = kWindowHeight - win_y - 154 - tile_size * y - 46;
      int left = win_x + 52 + tile_size * x;
      int right = left + 46;

      if (IsMouseInRectangle(x_pos, y_pos, left, right, bottom, top)) {
        if (mouse_rgt_click) {
          draw_2d_->DrawImage("context-panel", win_x + 52 + tile_size * x, kWindowHeight - win_y - 154 - 312 + tile_size * y, 400, 400, 1.0); 
        } else if (mouse_lft_click && selected_item == 0) {
          selected_item = item_matrix[x][y];
          item_matrix[x][y] = 0;
          old_pos_x = x;
          old_pos_y = y;
          hold_offset_x = x_pos - left;
          hold_offset_y = top - (y_pos - 64);
        } else if (!mouse_lft_click && selected_item != 0) {
          if (item_matrix[x][y] == 0) {
            item_matrix[x][y] = selected_item;
          } else {
            item_matrix[old_pos_x][old_pos_y] = selected_item;
          }
          selected_item = 0;
          old_pos_x = 0;
          old_pos_y = 0;
        }
      }

      if (item_matrix[x][y] == 0) continue;
      int item_id = item_matrix[x][y];
      draw_2d_->DrawImage(icons[item_id], win_x + 52 + tile_size * x, kWindowHeight - win_y - 154 - 312 + tile_size * y, 64, 64, 1.0); 
    }
  }

  if (!mouse_lft_click && selected_item != 0) {
    int top = kWindowHeight - win_y;
    int bottom = kWindowHeight - win_y;
    int left = win_x;
    int right = left + 800;
    if (IsMouseInRectangle(x_pos, y_pos, left, right, bottom, top)) {
      item_matrix[old_pos_x][old_pos_y] = selected_item;
    } else {
      vec3 position = camera.position + camera.direction * 10.0f;

      // TODO: if the item is another item, drop item instead.
      resources_->CreateGameObjFromAsset("spell-crystal", position);
    }

    selected_item = 0;
    old_pos_x = 0;
    old_pos_y = 0;
  }

  if (selected_item != 0) {
    draw_2d_->DrawImage(icons[selected_item], mouse_x - hold_offset_x, kWindowHeight - mouse_y + hold_offset_y, 64, 64, 1.0); 
  }

  draw_2d_->DrawImage("cursor", mouse_x, kWindowHeight - mouse_y, 64, 64, 1.0);

  // draw_2d_->DrawRectangle(win_x-1, kWindowHeight - 99, win_x + 602, 602, vec3(1, 0.69, 0.23));
  // draw_2d_->DrawRectangle(win_x, kWindowHeight - 100, win_x + 600, 600, vec3(0.3));

  // int x = win_x+1;
  // int y = kWindowHeight - 101;
  // for (auto& [asset, count] : inventory) {
  //   if (asset->name == "small-rock") {
  //     draw_2d_->DrawRectangle(x, y, 30, 30, vec3(0, 1, 0));
  //   } else if (asset->name == "berry") {
  //     draw_2d_->DrawRectangle(x, y, 30, 30, vec3(1, 0, 0));
  //   } else {
  //     draw_2d_->DrawRectangle(x, y, 30, 30, vec3(0, 0, 1));
  //   }

  //   x += 32;
  // }
}
