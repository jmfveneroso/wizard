#include "dialog.hpp"

const int kWindowWidth = 1280;
const int kWindowHeight = 800;

// Static function to process GLFW char input.
void Dialog::PressKeyCallback(int key, int scancode, int action, int mods) {
  if (!enabled) {
    return;
  }

  ProcessDefaultInput(key, scancode, action, mods);
}

bool Dialog::ProcessDefaultInput(int key, int scancode, int action, 
  int mods) {
  switch (key) {
    case GLFW_KEY_K: {
      cursor_--;
      if (cursor_ < 0) cursor_ = 0;
      return true;
    }
    case GLFW_KEY_J: {
      cursor_++;
      if (cursor_ >= dialog_options_.size()) cursor_ = dialog_options_.size()-1;
      return true;
    }
    case GLFW_KEY_ENTER: {
      // CraftSpell(cursor_);
      Disable();
      return true;
    }
    default:
      return false;
  }
}

Dialog::Dialog(shared_ptr<AssetCatalog> asset_catalog, 
  shared_ptr<Draw2D> draw_2d)
  : asset_catalog_(asset_catalog), draw_2d_(draw_2d) {
}

void Dialog::SetDialogOptions(vector<string> dialog_options) {
  dialog_options_.clear();
  for (int i = 0; i < dialog_options.size(); i++) {
    dialog_options_.push_back(dialog_options[i]);
  }
}

void Dialog::Draw(int win_x, int win_y) {
  if (!enabled)
    return;

  int pos_y = 580;

  draw_2d_->DrawRectangle(win_x-1, kWindowHeight - pos_y, win_x + 602, 102, vec3(1, 0.69, 0.23));
  draw_2d_->DrawRectangle(win_x, kWindowHeight - pos_y+1, win_x + 600, 100, vec3(0.3));

  for (int i = 0; i < dialog_options_.size(); i++) {
    pos_y += 16;

    if (cursor_ == i) {
      draw_2d_->DrawRectangle(win_x, kWindowHeight - pos_y, win_x + 600, 18, vec3(1, 0.69, 0.23));
      draw_2d_->DrawText(dialog_options_[i], win_x-1, kWindowHeight - pos_y, vec3(0.3));
    } else {
      draw_2d_->DrawText(dialog_options_[i], win_x-1, kWindowHeight - pos_y, vec3(1));
    }
  }

  // if (cursor_ == 0) {
  //   draw_2d_->DrawRectangle(win_x + 18, kWindowHeight - 600, 54, 54, vec3(0, 0, 1));
  // } else {
  //   draw_2d_->DrawRectangle(win_x + 73, kWindowHeight - 600, 54, 54, vec3(0, 0, 1));
  // }

  // draw_2d_->DrawRectangle(win_x + 20, kWindowHeight - 602, 50, 50, vec3(0, 1, 0));
  // draw_2d_->DrawRectangle(win_x + 75, kWindowHeight - 602, 50, 50, vec3(1, 0, 0));
}
