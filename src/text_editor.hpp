#ifndef _TEXT_EDITOR_HPP_
#define _TEXT_EDITOR_HPP_

#include <algorithm>
#include <vector>
#include <iostream>
#include <memory>
#include <fstream>
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <math.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp> 
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "2d.hpp"

using namespace std;
using namespace glm;

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
  vector<string> content_;
  string command;
  bool enabled;
  string write_buffer;
  int cursor_row_;
  int cursor_col_;
  string filename;
  int start_line;

  shared_ptr<Draw2D> draw_2d_;

 public:
  bool update_object = false;
  int create_object = -1;

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
};

#endif
