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

bool Dialog::ProcessDefaultInput(int key, int scancode, int action, int mods) {
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
      if (cursor_ == 0) {
        resources_->SetGameState(STATE_BUILD);

        ObjPtr player = resources_->GetPlayer();
        shared_ptr<Configs> configs = resources_->GetConfigs();
        configs->old_position = player->position;
        player->position = vec3(11361, 331, 7888);

        configs->new_building = resources_->CreateGameObjFromAsset(
          "windmill", vec3(0));
      }
      Disable();
      return true;
    }
    default:
      return false;
  }
}

Dialog::Dialog(shared_ptr<Resources> asset_catalog, 
  shared_ptr<Draw2D> draw_2d)
  : resources_(asset_catalog), draw_2d_(draw_2d) {
}

void Dialog::SetMainText(string main_text) {
  main_text_ = main_text;
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

  int pos_y = 400;

  // draw_2d_->DrawRectangle(win_x-1, kWindowHeight - pos_y, win_x + 602, 102, vec3(1, 0.69, 0.23));
  // draw_2d_->DrawRectangle(win_x, kWindowHeight - pos_y+1, win_x + 600, 100, vec3(0.3));

  draw_2d_->DrawImage("dialog", 220, -pos_y, 800, 800, 1.0);

  pos_y += 100;
  draw_2d_->DrawText(main_text_, 300, kWindowHeight - pos_y, vec3(1), 1.0, false, "avenir_light_oblique");
  pos_y += 40;

  for (int i = 0; i < dialog_options_.size(); i++) {
    pos_y += 20;
    if (cursor_ == i) {
      draw_2d_->DrawRectangle(300, kWindowHeight - (pos_y - 16), 300 + 200, 18, vec3(1, 0.69, 0.23));
    }
    draw_2d_->DrawText(dialog_options_[i], 300-1, kWindowHeight - pos_y, vec3(1), 1.0, false, "avenir_light_oblique");
  }
}
