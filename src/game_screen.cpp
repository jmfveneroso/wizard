#include <boost/algorithm/string.hpp>
#include "game_screen.hpp"

const int kWindowWidth = 1440;
const int kWindowHeight = 900;

GameScreen::GameScreen(shared_ptr<Resources> asset_catalog, 
  shared_ptr<Draw2D> draw_2d) : resources_(asset_catalog), 
  draw_2d_(draw_2d) {
}

void GameScreen::UpdateMouse(GLFWwindow* window) {
  double x_pos, y_pos;
  glfwGetCursorPos(window, &x_pos, &y_pos);
  mouse_x_ = x_pos;
  mouse_y_ = y_pos;

  lft_click_ = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  rgt_click_ = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
  if (mouse_x_ < 0) mouse_x_ = 0; 
  if (mouse_x_ > kWindowWidth - 1) mouse_x_ = kWindowWidth - 1; 
  if (mouse_y_ < 0) mouse_y_ = 0; 
  if (mouse_y_ > kWindowHeight) mouse_y_ = kWindowHeight; 
  glfwSetCursorPos(window, mouse_x_, mouse_y_); 
}

bool GameScreen::IsMouseInRectangle(int left, int right, int bottom, int top) {
  return (mouse_x_ > left && mouse_x_ < right && 
          mouse_y_ < bottom && mouse_y_ > top);
}

bool GameScreen::IsMouseInCircle(const ivec2& pos, float circle) {
  return length2(vec2(mouse_x_, mouse_y_) - vec2(pos)) < circle * circle;
}

void GameScreen::ProcessInput() {
  UpdateMouse(window_);

  if (glfwGetKey(window_, GLFW_KEY_SPACE) == GLFW_PRESS ||
    (IsMouseInRectangle(200, 300, 300, 200) && lft_click_)) {
    resources_->RestartGame();
    resources_->SetGameState(STATE_GAME);
  }
}

void GameScreen::Draw(const Camera& camera, int win_x, int win_y, 
  GLFWwindow* window) {
  camera_ = camera;
  win_x_ = win_x;
  win_y_ = win_y;
  window_ = window;

  ProcessInput();

  shared_ptr<Configs> configs = resources_->GetConfigs();
  ivec2 pos = vec2(win_x_, win_y_);

  draw_2d_->DrawImage("game_screen", pos.x, pos.y, 1440, 900, 1.0);

  draw_2d_->DrawRectangle(200, kWindowHeight - 200, 100, 100, vec3(1));

  draw_2d_->DrawImage("cursor", mouse_x_, mouse_y_, 64, 64, 1.0);
}
