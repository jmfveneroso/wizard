#ifndef _TEXT_EDITOR_HPP_
#define _TEXT_EDITOR_HPP_

#include "2d.hpp"

enum TextEditorMode {
  TXT_FILE,
  CREATE_OBJECT
};

class TextEditor {
  TextEditorMode mode_ = TXT_FILE;
  bool on_g;
  bool on_delete;
  double repeat_wait;
  bool ignore;
  bool editable = true;
  int mode;
  bool already_processed_key;
  vector<string> content_;
  string command;
  string write_buffer;
  int cursor_row_;
  int cursor_col_;
  string filename;
  int start_line;

  shared_ptr<Draw2D> draw_2d_;
  function<void(string)> run_command_fn_;

  bool ProcessDefaultInput(int key, int scancode, int action, int mods);
  bool ProcessTextInput(int key, int scancode, int action, int mods);
  bool ProcessCommandInput(int key, int scancode, int action, int mods);

 public:
  bool enabled;

  TextEditor(shared_ptr<Draw2D> draw_2d) 
    : draw_2d_(draw_2d) {
    command = "";
    on_g = false;
    on_delete = false;
    repeat_wait = 0.0f;
    ignore = false;
    enabled = false;
    write_buffer = "";
    cursor_row_ = 0;
    cursor_col_ = 0;
    content_ = {};
    mode = 0;
    start_line = 0;
    filename = "";
  }

  void PressCharCallback(string);
  void PressKeyCallback(int, int, int, int);

  void Draw(int win_x = 200, int win_y = 100);
  void SetContent(string);
  void OpenFile(string);
  void WriteFile();
  void Enable() { enabled = true; cursor_row_ = 0; cursor_col_ = 0; }
  void Disable() { enabled = false; cursor_row_ = 0; cursor_col_ = 0; }
  bool Close() { return !enabled; }
  void SetCursorPos(int x, int y);

  void set_run_command_fn(function<void(string)> run_command_fn) {
    run_command_fn_ = run_command_fn;
  }
};

#endif
