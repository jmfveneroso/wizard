#include "craft.hpp"

const int kWindowWidth = 1280;
const int kWindowHeight = 800;

// Static function to process GLFW char input.
void Craft::PressKeyCallback(int key, int scancode, int action, int mods) {
  if (!enabled) {
    return;
  }

  ProcessDefaultInput(key, scancode, action, mods);
}

void Craft::CraftSpell(int number) {
  vector<tuple<shared_ptr<GameAsset>, int>>& inventory = resources_->GetInventory();
  if (number == 0) {
    int num_items = 0;
    for (auto& [asset, count] : inventory) {
      if (asset->name == "small-rock") {
        num_items++;
      }
    }
    if (num_items < 5) return;

    vector<tuple<shared_ptr<GameAsset>, int>> new_inventory;
    int count = 0;
    for (auto& [asset, count] : inventory) {
      if (asset->name == "small-rock" && count > 0) {
        count--;
        continue;
      }
      new_inventory.push_back({asset, count});
    }
    inventory = new_inventory;

    ObjPtr* cubes = project_4d_->GetCubes();
    for (int i = 0; i < 8; i++) { 
      if (!cubes[i]) continue;
      cubes[i]->draw = true;
    }
    hypercube_life_ = 20;
    creating_spell_ = 0;
    inventory.clear();
    for (int i = 0; i < new_inventory.size(); i++) {
      inventory.push_back(new_inventory[i]);
    }
    cout << "Creating 0" << endl;
  } else if (number == 1) {
    int num_items = 0;

    for (auto& [asset, count] : inventory) {
      if (asset->name == "berry") {
        num_items++;
      }
    }
    if (num_items < 5) return;

    vector<tuple<shared_ptr<GameAsset>, int>> new_inventory;
    int count = 0;
    for (auto& [asset, count] : inventory) {
      if (asset->name == "berry" && count > 0) {
        count--;
        continue;
      }
      new_inventory.push_back({asset, count});
    }
    inventory = new_inventory;

    ObjPtr* cubes = project_4d_->GetCubes();
    for (int i = 0; i < 8; i++) { 
      if (!cubes[i]) continue;
      cubes[i]->draw = true;
    }
    hypercube_life_ = 20;
    creating_spell_ = 1;

    inventory.clear();
    for (int i = 0; i < new_inventory.size(); i++) {
      inventory.push_back(new_inventory[i]);
    }
    cout << "Creating 1" << endl;
  }
}

bool Craft::ProcessDefaultInput(int key, int scancode, int action, 
  int mods) {
  switch (key) {
    case GLFW_KEY_H: {
      cursor_--;
      if (cursor_ < 0) cursor_ = 0;
      return true;
    }
    case GLFW_KEY_L: {
      cursor_++;
      if (cursor_ > 1) cursor_ = 1;
      return true;
    }
    case GLFW_KEY_ENTER: {
      CraftSpell(cursor_);
      Disable();
      return true;
    }
    default:
      return false;
  }
}

Craft::Craft(shared_ptr<Resources> asset_catalog, 
  shared_ptr<Draw2D> draw_2d, shared_ptr<Project4D> project_4d) 
  : resources_(asset_catalog), draw_2d_(draw_2d), project_4d_(project_4d) {
}

void Craft::Draw(int win_x, int win_y) {
  if (!enabled)
    return;

  draw_2d_->DrawRectangle(win_x-1, kWindowHeight - 99, win_x + 602, 602, vec3(1, 0.69, 0.23));
  draw_2d_->DrawRectangle(win_x, kWindowHeight - 100, win_x + 600, 600, vec3(0.3));

  if (cursor_ == 0) {
    draw_2d_->DrawRectangle(win_x + 18, kWindowHeight - 118, 54, 54, vec3(0, 0, 1));
  } else {
    draw_2d_->DrawRectangle(win_x + 73, kWindowHeight - 118, 54, 54, vec3(0, 0, 1));
  }

  draw_2d_->DrawRectangle(win_x + 20, kWindowHeight - 120, 50, 50, vec3(0, 1, 0));
  draw_2d_->DrawRectangle(win_x + 75, kWindowHeight - 120, 50, 50, vec3(1, 0, 0));
}

void Craft::ProcessCrafting() {
  shared_ptr<Player> player = resources_->GetPlayer();
  if (creating_spell_ != -1) {
    hypercube_life_ -= 0.1f;

    if (hypercube_life_ < 0) {
      ObjPtr* cubes = project_4d_->GetCubes();
      for (int i = 0; i < 8; i++) { 
        if (!cubes[i]) continue;
        cubes[i]->draw = false;
      }

      if (creating_spell_ == 0) {
        player->num_spells += 10;
      } else {
        player->num_spells_2 += 2;
      }
      creating_spell_ = -1;
    }
  }
}
