#ifndef _GAME_SCREEN_HPP_
#define _GAME_SCREEN_HPP_

#include "2d.hpp"
#include "resources.hpp"

using namespace std;
using namespace glm;

class GameScreen {
  shared_ptr<Draw2D> draw_2d_;
  shared_ptr<Resources> resources_;

  Camera camera_;
  int win_x_ = 0;
  int win_y_ = 0;
  int debounce_ = 0;
  int mouse_x_ = 0;
  int mouse_y_ = 0;
  bool lft_click_ = false;
  bool rgt_click_ = false;
  int throttle_ = 0;
  int cursor_pos_ = 0;

  float closing = 0.0f;
  GLFWwindow* window_;

  void UpdateMouse(GLFWwindow* window);
  bool IsMouseInRectangle(int left, int right, int bottom, int top);
  bool IsMouseInCircle(const ivec2& pos, float circle);
  void UpdateAnimations();
  void DrawCogs();

 public:
  GameScreen(shared_ptr<Resources> asset_catalog, shared_ptr<Draw2D> draw_2d);

  void ProcessInput();
  void Draw(const Camera& camera, int win_x = 200, int win_y = 100, GLFWwindow* window = nullptr);
};

#endif

