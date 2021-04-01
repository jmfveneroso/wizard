#ifndef __DIALOG_HPP__
#define __DIALOG_HPP__

#include "resources.hpp"
#include "2d.hpp"

enum DialogMode {
  DIALOG_DIALOG,
  DIALOG_BOOK
};

// TODO: create functions to set the dialog options and a callback to determine
// the result.
class Dialog {
  DialogMode mode = DIALOG_DIALOG;

  shared_ptr<Resources> resources_;
  shared_ptr<Draw2D> draw_2d_;
  int cursor_ = 0;
  bool already_processed_key;
  int spell_id_ = 0;

  bool ProcessDefaultInput(int key, int scancode, int action, int mods);

  string main_text_;
  vector<string> dialog_options_;

  void DrawDialog(int win_x, int win_y);
  void DrawBook(int win_x, int win_y);

 public:
  bool enabled = false;

  Dialog(shared_ptr<Resources> asset_catalog, shared_ptr<Draw2D> draw_2d);
  void Draw(int win_x = 200, int win_y = 100);

  void Enable(DialogMode new_mode = DIALOG_DIALOG) { 
    mode = new_mode;
    enabled = true; 
  }
  void Disable();
  bool Close() { return !enabled; }

  void PressKeyCallback(int, int, int, int);
  void ProcessCrafting();

  void SetSpell(int spell_id) { spell_id_ = spell_id; }
  void SetMainText(string main_text);
  void SetDialogOptions(vector<string> dialog_options);
};

#endif
