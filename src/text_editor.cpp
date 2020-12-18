#include "text_editor.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

const int kWindowWidth = 1280;
const int kWindowHeight = 800;
const int kLineHeight = 18;

// Static function to process GLFW char input.
void TextEditor::PressCharCallback(string buffer) {
  if (!enabled) return;
  if (already_processed_key) return;
  if (buffer.size() == 0) return;
  if (mode == 0) return;

  switch (mode) {
    case 1:
      if (cursor_col_ == 0) {
        content_[cursor_row_] = buffer + content_[cursor_row_];
      } else 
        content_[cursor_row_] = content_[cursor_row_].substr(0, cursor_col_) + buffer + content_[cursor_row_].substr(cursor_col_);

      cursor_col_++;
      break;
    case 2:
      command += buffer;
      break;
  }
}

bool TextEditor::ProcessDefaultInput(int key, int scancode, int action, 
  int mods) {
  if (action == GLFW_REPEAT) {
    switch (key) {
      case GLFW_KEY_H: 
        if (cursor_col_ >= content_[cursor_row_].size()) cursor_col_ = content_[cursor_row_].size() - 1;
        if (cursor_col_ > 0) cursor_col_--;
        if (cursor_col_ < 0) cursor_col_ = 0;
        return true;
      case GLFW_KEY_J: 
        if (cursor_row_ < content_.size() - 1) cursor_row_++;
        if (cursor_row_ >= start_line + 30) start_line++; 
        return true;
      case GLFW_KEY_K: 
        if (cursor_row_ > 0) cursor_row_--;
        if (cursor_row_ < start_line) start_line--; 
        return true;
      case GLFW_KEY_L: 
        if (content_.size() > 0 && cursor_col_ < content_[cursor_row_].size() - 1) cursor_col_++;
        return true;
      case GLFW_KEY_X:
        if (cursor_col_ >= 0 && content_[cursor_row_].size()) {
          content_[cursor_row_] = content_[cursor_row_].substr(0, cursor_col_) + content_[cursor_row_].substr(cursor_col_+1);
        }
        return true;
      default:
        return false;
    }
  }

  if (action != GLFW_PRESS) {
    return false;
  }

  switch (key) {
    case GLFW_KEY_G: {
      if (mods & GLFW_MOD_SHIFT) {
        start_line = std::max(0, (int) content_.size() - 30);
        cursor_row_ = content_.size() - 1;
        cursor_col_ = content_[content_.size()-1].size() - 1;
        return true;
      }

      if (on_g) {
        start_line = 0;
        cursor_row_ = 0;
        cursor_col_ = 0;
        on_g = false;
      } else {
        on_g = true;
      }
      return true;
    }
    case GLFW_KEY_H: 
      if (mode == 1) return true;
      if (cursor_col_ >= content_[cursor_row_].size()) cursor_col_ = content_[cursor_row_].size() - 1;
      if (cursor_col_ > 0) cursor_col_--;
      if (cursor_col_ < 0) cursor_col_ = 0;
      repeat_wait = glfwGetTime() + 0.5;
      return true;
    case GLFW_KEY_J: 
      if (mode == 1) return true;
      if (cursor_row_ < content_.size() - 1) cursor_row_++;
      if (cursor_row_ >= start_line + 30) start_line++; 
      repeat_wait = glfwGetTime() + 0.5;
      return true;
    case GLFW_KEY_K: 
      if (mode == 1) return true;
      if (cursor_row_ > 0) cursor_row_--;
      if (cursor_row_ < start_line) start_line--; 
      repeat_wait = glfwGetTime() + 0.5;
      return true;
    case GLFW_KEY_L: 
      if (mode == 1) return true;
      if (content_.size() > 0 && cursor_col_ < content_[cursor_row_].size() - 1) cursor_col_++;
      repeat_wait = glfwGetTime() + 0.5;
      return true;
    case GLFW_KEY_I: {
      if (!editable) return true;
      if (mods & GLFW_MOD_SHIFT) {
        cursor_col_ = 0;
        if (content_.size() > 0) {
          while (cursor_col_ < content_[cursor_row_].size() && content_[cursor_row_][cursor_col_] == ' ') 
            cursor_col_++;
        }
 
        ignore = true;
        mode = 1;
      } else {
        if (content_.size() > 0 && cursor_col_ >= content_[cursor_row_].size()) cursor_col_ = content_[cursor_row_].size() - 1;
        if (cursor_col_ < 0) cursor_col_ = 0;
        ignore = true;
        mode = 1;
      }
      return true;
    }
    case GLFW_KEY_X:
      if (!editable) return true;
      if (cursor_col_ >= 0 && content_[cursor_row_].size()) {
        content_[cursor_row_] = content_[cursor_row_].substr(0, cursor_col_) + content_[cursor_row_].substr(cursor_col_+1);
      }
      return true;
    case GLFW_KEY_S:
      if (!editable) return true;
      if (cursor_col_ >= 0 && content_[cursor_row_].size()) {
        content_[cursor_row_] = content_[cursor_row_].substr(0, cursor_col_) + content_[cursor_row_].substr(cursor_col_+1);
      }
      if (content_.size() > 0 && cursor_col_ >= content_[cursor_row_].size()) cursor_col_ = content_[cursor_row_].size() - 1;
      if (cursor_col_ < 0) cursor_col_ = 0;
      ignore = true;
      mode = 1;
      return true;
    case GLFW_KEY_A:
      if (!editable) return true;
      if (mods & GLFW_MOD_SHIFT) {
        if (content_.size() > 0) cursor_col_ = content_[cursor_row_].size();
        ignore = true;
        mode = 1;
      } else {
        cursor_col_++;
        if (content_.size() > 0 && cursor_col_ >= content_[cursor_row_].size()) cursor_col_ = content_[cursor_row_].size() - 1;
        if (cursor_col_ < 0) cursor_col_ = 0;
        ignore = true;
        mode = 1;
      }
      return true;
    case GLFW_KEY_O:
      if (!editable) return true;
      if (mods & GLFW_MOD_SHIFT) {
        vector<string> after { "" };
        content_.insert(content_.begin() + cursor_row_, after.begin(), after.end());
        cursor_col_ = 0;
        ignore = true;
        mode = 1;
      } else {
        vector<string> after { "" };
        content_.insert(content_.begin() + cursor_row_ + 1, after.begin(), after.end());
        cursor_row_++;
        if (cursor_row_ >= start_line + 30) start_line++;
        cursor_col_ = 0;
        ignore = true;
        mode = 1;
      }
      return true;
    case GLFW_KEY_U: {
      if (mods & GLFW_MOD_CONTROL) {
        start_line = std::max(start_line - 10, 0);
        cursor_row_ = std::max(cursor_row_ - 10, 0);
      }
      return true;
    }
    case GLFW_KEY_D: {
      if (mods & GLFW_MOD_CONTROL) {
        if (start_line >= (int) content_.size() - 30) {
          cursor_row_ = content_.size() - 1;
          return true;
        }
        start_line = std::min(start_line + 10, (int) content_.size() - 1);
        cursor_row_ = std::min(cursor_row_ + 10, (int) content_.size() - 1);
        return true;
      }

      if (!editable) return true;
      if (on_delete) {
        content_.erase(content_.begin() + cursor_row_);
        if (content_.size() == 0)
          content_.push_back("");
        if (cursor_row_ >= content_.size()) 
          cursor_row_--;
        on_delete = false;
        cursor_col_ = 0;
      } else {
        on_delete = true;
      }
      return true;
    }
    case GLFW_KEY_ENTER:
      enabled = false; 
      return true;
    case GLFW_KEY_ESCAPE: 
      on_delete = false;
      on_g = false;
      return true;
    case GLFW_KEY_SEMICOLON:
    case GLFW_KEY_SLASH: // For Portuguese keyboards.
      if (mods & GLFW_MOD_SHIFT) {
        mode = 2;
        command = ":";
      }
      return true;
    default:
      break;
  }
  return false;
}

bool TextEditor::ProcessTextInput(int key, int scancode, int action, 
  int mods) {
  if (action == GLFW_REPEAT) {
    switch (key) {
      case GLFW_KEY_BACKSPACE: {
        if (cursor_col_ == 0 && cursor_row_ > 0) {
          content_.erase(content_.begin() + cursor_row_);
          cursor_row_--;
          if (cursor_row_ < start_line) start_line--;
          cursor_col_ = content_[cursor_row_].size();
        } else if (cursor_row_ > 0) {
          content_[cursor_row_] = content_[cursor_row_].substr(0, cursor_col_-1) + content_[cursor_row_].substr(cursor_col_);
          cursor_col_--;
        }
        return true;
      }
      case GLFW_KEY_ENTER:
        string before = content_[cursor_row_].substr(0, cursor_col_);
        vector<string> after { content_[cursor_row_].substr(cursor_col_) };
        content_[cursor_row_] = before;
        content_.insert(content_.begin() + cursor_row_ + 1, after.begin(), after.end());
        cursor_row_++;
        if (cursor_row_ >= start_line + 30) start_line++;
        cursor_col_ = 0;
        return true;
    }
  }

  if (action != GLFW_PRESS) {
    return false;
  }

  switch (key) {
    case GLFW_KEY_ESCAPE: 
      mode = 0;
      on_delete = false;
      return true;
    case GLFW_KEY_BACKSPACE: {
      if (cursor_col_ == 0 && cursor_row_ > 0) {
        content_.erase(content_.begin() + cursor_row_);
        cursor_row_--;
        if (cursor_row_ < start_line) start_line--;
        cursor_col_ = content_[cursor_row_].size();
      } else if (cursor_col_ > 0) {
        content_[cursor_row_] = content_[cursor_row_].substr(0, cursor_col_-1) + content_[cursor_row_].substr(cursor_col_);
        cursor_col_--;
      }
      return true;
    }
    case GLFW_KEY_ENTER: {
      string before = content_[cursor_row_].substr(0, cursor_col_);
      vector<string> after { content_[cursor_row_].substr(cursor_col_) };
      content_[cursor_row_] = before;
      content_.insert(content_.begin() + cursor_row_ + 1, after.begin(), after.end());
      cursor_row_++;
      if (cursor_row_ >= start_line + 30) start_line++;
      cursor_col_ = 0;
      return true;
    }
    default:
      break;
  }
  return false;
}

bool TextEditor::ProcessCommandInput(int key, int scancode, int action, 
  int mods) {
  if (action != GLFW_PRESS) {
    return false;
  }

  switch (key) {
    case GLFW_KEY_BACKSPACE: {
      if (command.size() == 1) {
        mode = 0;
      } else {
        command = command.substr(0, command.size()-1);
      }
      return true;
    }
    case GLFW_KEY_ENTER: {
      mode = 0;
      string cmd = command.substr(1);
      if (cmd == "w") {
        WriteFile();
      } else if (cmd == "q") {
        enabled = false; 
      } else if (cmd == "wq") {
        WriteFile();
        enabled = false; 
      } else {
        run_command_fn_(cmd.c_str());
      }
      return true;
    }
    case GLFW_KEY_ESCAPE: {
      mode = 0;
      return true;
    }
    default:
      break;
  }
  return false;
}

void TextEditor::PressKeyCallback(int key, int scancode, int action, int mods) {
  if (!enabled) {
    return;
  }

  already_processed_key = false;
  switch (mode) {
    case 0:
     already_processed_key = ProcessDefaultInput(key, scancode, action, mods);
     break;
    case 1:
     already_processed_key = ProcessTextInput(key, scancode, action, mods);
     break;
    case 2:
     already_processed_key = ProcessCommandInput(key, scancode, action, mods);
     break;
    default:
     throw runtime_error("Invalid mode");
  }
}

void TextEditor::OpenFile(string new_filename) {
  filename = new_filename;
  if (new_filename == "/dev/create") {
    SetContent("Create Scroll\nCreate 2D Plot\nCreate 3D Plot\nCreate Book\nCreate Art Object");
    editable = false; 
    mode_ = CREATE_OBJECT;
    return;
  }

  mode_ = TXT_FILE;
  editable = true; 

  fstream f(filename, ios::in);
  if (!f.is_open()) {
    fstream f(filename, ios::app);
    SetContent("");
    return;
  }

  string content((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
  SetContent(content);
}

void TextEditor::WriteFile() {
  ofstream f(filename, std::ofstream::out);
  if (!f.is_open()) return;

  for (int i = 0; i < content_.size(); i++) {
    f << content_[i] << endl; 
  }
  f.close();
}

void TextEditor::SetContent(string text) {
  boost::split(content_, text, boost::is_any_of("\n"));

  if (content_.size() == 0)
    content_.push_back("");
}

void TextEditor::Draw(int win_x, int win_y) {
  if (!enabled)
    return;

  draw_2d_->DrawRectangle(win_x-1, kWindowHeight - 99, win_x + 602, 602, vec3(1, 0.69, 0.23));
  draw_2d_->DrawRectangle(win_x, kWindowHeight - 100, win_x + 600, 600, vec3(0.3));

  double current_time = glfwGetTime();
  vector<string> lines = content_;

  int digits = 0;
  for (int aux = lines.size(); aux; aux /= 10) { digits++; }
  digits = std::max(digits, 3);

  int base_x = win_x + digits * 9 + 8;
  int base_y = kWindowHeight - 100 - kLineHeight;
  int height = 0;
  if (mode_ == CREATE_OBJECT) {
    draw_2d_->DrawRectangle(win_x + 2, base_y - kLineHeight * (cursor_row_ - 1) - 3, win_x + 596, kLineHeight, vec3(1, 1, 1));
  }

  for (int y = start_line; y < start_line + 30; ++y) {
    if (y >= lines.size()) break;
    stringstream ss;
    ss << y + 1;

    int digits_ = 0;
    for (int aux = y + 1; aux; aux /= 10) { digits_++; }
    int offset_x = (digits - digits_) * 9;
    draw_2d_->DrawText(ss.str(), win_x + 2 + offset_x, base_y - height, vec3(1, 0.69, 0.23));

    if (lines[y].size() == 0 && cursor_row_ == y && mode != 2) {
      draw_2d_->DrawChar((char) 150, base_x + 2, base_y - height);
    }

    for (int x = 0; x < 80; ++x) {
      if (x >= lines[y].size()) break;

      bool draw_cursor = false;
      if (cursor_col_ == x && cursor_row_ == y) draw_cursor = true;
      if (cursor_row_ == y && cursor_col_ >= lines[y].size() && 
          x == lines[y].size() - 1 && mode == 0) draw_cursor = true;
      if (mode == 2) draw_cursor = false;

      vec3 color = vec3(1);
      if (mode_ == CREATE_OBJECT && cursor_row_ == y) {
        color = vec3(0.3);
      }

      if (draw_cursor) {
        draw_2d_->DrawChar((char) 150, base_x + 2 + x * 9, base_y - height);
        draw_2d_->DrawChar(lines[y][x], base_x + 2 + x * 9, base_y - height, vec3(0, 0, 0));
      } else {
        draw_2d_->DrawChar(lines[y][x], base_x + 2 + x * 9, base_y - height, color);
      }
    }

    if (cursor_col_ == lines[y].size() && cursor_row_ == y && mode == 1) {
      draw_2d_->DrawChar((char) 150, base_x + 2 + lines[y].size() * 9, base_y - height);
    }
    height += kLineHeight;
  }

  for (int y = lines.size(); y < 30; ++y) {
    draw_2d_->DrawText("~", win_x + 2, base_y - height, vec3(1, 0.69, 0.23));
    height += kLineHeight;
  }

  draw_2d_->DrawRectangle(win_x + 2, base_y - kLineHeight * 29 - 3, win_x + 596, kLineHeight, vec3(1, 0.69, 0.23));
  draw_2d_->DrawText(filename, win_x + 2, base_y - kLineHeight * 30, vec3(0.3));

  if (mode == 1) {
    draw_2d_->DrawText("-- INSERT --", win_x + 2, base_y - kLineHeight * 32, vec3(1, 0.69, 0.23));
  }

  // Cursor.
  if (mode == 2) {
    draw_2d_->DrawText(command, win_x + 2, base_y - kLineHeight * 31, vec3(1, 0.69, 0.23));
    draw_2d_->DrawChar((char) 150, win_x + 2 + command.size() * 9, base_y - kLineHeight * 31, vec3(1, 0.69, 0.23));
  }
}
