#include "inventory.hpp"

const int kWindowWidth = 1280;
const int kWindowHeight = 800;

Inventory::Inventory(shared_ptr<Resources> asset_catalog, 
  shared_ptr<Draw2D> draw_2d) : resources_(asset_catalog), 
  draw_2d_(draw_2d) {
}

void Inventory::Draw(int win_x, int win_y) {
  if (!enabled)
    return;

  draw_2d_->DrawRectangle(win_x-1, kWindowHeight - 99, win_x + 602, 602, vec3(1, 0.69, 0.23));
  draw_2d_->DrawRectangle(win_x, kWindowHeight - 100, win_x + 600, 600, vec3(0.3));

  vector<tuple<shared_ptr<GameAsset>, int>>& inventory = resources_->GetInventory();

  int x = win_x+1;
  int y = kWindowHeight - 101;
  for (auto& [asset, count] : inventory) {
    if (asset->name == "small-rock") {
      draw_2d_->DrawRectangle(x, y, 30, 30, vec3(0, 1, 0));
    } else if (asset->name == "berry") {
      draw_2d_->DrawRectangle(x, y, 30, 30, vec3(1, 0, 0));
    } else {
      draw_2d_->DrawRectangle(x, y, 30, 30, vec3(0, 0, 1));
    }

    x += 32;
  }
}
