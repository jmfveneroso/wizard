#ifndef __DIALOG_HPP__
#define __DIALOG_HPP__

#include "resources.hpp"
#include "2d.hpp"

class Dialog {
  shared_ptr<Resources> resources_;
  shared_ptr<Draw2D> draw_2d_;
  int cursor_ = 0;
  bool already_processed_key;

  bool ProcessDefaultInput(int key, int scancode, int action, int mods);

  vector<string> dialog_options_;

 public:
  bool enabled;

  Dialog(shared_ptr<Resources> asset_catalog, shared_ptr<Draw2D> draw_2d);
  void Draw(int win_x = 200, int win_y = 100);

  void Enable() { enabled = true; }
  void Disable() { enabled = false; }
  bool Close() { return !enabled; }

  void PressKeyCallback(int, int, int, int);
  void ProcessCrafting();

  void SetDialogOptions(vector<string> dialog_options);
};

#endif