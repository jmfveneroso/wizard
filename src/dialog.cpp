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

        // configs->new_building = CreateGameObjFromAsset(resources_.get(),
        //   "windmill", vec3(0));
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

void Dialog::DrawDialog(int win_x, int win_y) {
  int pos_y = 400;

  draw_2d_->DrawImage("dialog", 220, -pos_y, 800, 800, 1.0);

  pos_y += 100;
  draw_2d_->DrawText(main_text_, 300, kWindowHeight - pos_y, vec4(1), 1.0, false, "avenir_light_oblique");
  pos_y += 40;

  for (int i = 0; i < dialog_options_.size(); i++) {
    pos_y += 20;
    if (cursor_ == i) {
      draw_2d_->DrawRectangle(300, kWindowHeight - (pos_y - 16), 
        300 + 200, 18, vec3(1, 0.69, 0.23));
    }
    draw_2d_->DrawText(dialog_options_[i], 300-1, kWindowHeight - pos_y, 
      vec4(1), 1.0, false, "avenir_light_oblique");
  }
}

void Dialog::DrawBook(int win_x, int win_y) {
  draw_2d_->DrawImage("book_interface", win_x, win_y, 800, 800, 1.0);

  if (spell_id_ > 0) {
    const SpellData& spell_data = resources_->GetSpellData()[spell_id_];
    draw_2d_->DrawImage(spell_data.image_name, win_x + 100, win_y + 100, 
      spell_data.image_size[0], spell_data.image_size[1], 1.0);
  }
}

void Dialog::Draw(int win_x, int win_y) {
  if (!enabled)
    return;

  switch (mode) {
    case DIALOG_DIALOG:
      DrawDialog(win_x, win_y);
      break;
    case DIALOG_BOOK:
      DrawBook(win_x, win_y);
      break;
  }
}

void Dialog::Disable() { 
  enabled = false; 
  ObjPtr player = resources_->GetPlayer();
  shared_ptr<Player> player_p = static_pointer_cast<Player>(player);
  player_p->talking_to = "";
}
