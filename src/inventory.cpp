#include "inventory.hpp"

const int kWindowWidth = 1280;
const int kWindowHeight = 800;

Inventory::Inventory(shared_ptr<Resources> asset_catalog, 
  shared_ptr<Draw2D> draw_2d) : resources_(asset_catalog), 
  draw_2d_(draw_2d) {
}

void Inventory::Draw(int win_x, int win_y, GLFWwindow* window) {
  if (!enabled)
    return;

  vector<tuple<shared_ptr<GameAsset>, int>>& inventory = resources_->GetInventory();
  draw_2d_->DrawImage("inventory", win_x, - win_y, 800, 800, 1.0);


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
      if (item_matrix[x][y] == 0) {
      } else {
        draw_2d_->DrawImage("flower-icon", win_x + 52 + tile_size * x, kWindowHeight - win_y - 154 - 312 + tile_size * y, 64, 64, 1.0); 

        int top = kWindowHeight - win_y - 154 - tile_size * y;
        int bottom = kWindowHeight - win_y - 154 - tile_size * y - 46;
        int left = win_x + 52 + tile_size * x;
        int right = left + 46;

        if (x_pos > left && x_pos < right && (y_pos - 64) > bottom && (y_pos - 64) < top) {
          if (mouse_rgt_click) {
            draw_2d_->DrawImage("context-panel", win_x + 52 + tile_size * x, kWindowHeight - win_y - 154 - 312 + tile_size * y, 400, 400, 1.0); 
          }
        }
      }
    }
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
