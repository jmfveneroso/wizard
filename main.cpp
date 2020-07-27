#include <fbxsdk.h>
#include <stdio.h>
#include <iostream>
#include <exception>
#include <memory>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp> 
#include <exception>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <unordered_map>
#include "renderer.hpp"

#define PLAYER_SPEED 0.5f
#define PLAYER_HEIGHT 1.5

using namespace std;
using namespace glm;

struct Player {
  glm::vec3 position = glm::vec3(2000, 31, 2000);
  glm::vec3 next_position = glm::vec3(0, 0, 0);
  glm::vec3 speed = glm::vec3(0, 0, 0);
  float h_angle = -230.991;
  float v_angle = 0;
};

Player player_;
Renderer renderer;

void ProcessGameInput(){
  static double last_time = glfwGetTime();
  double current_time = glfwGetTime();

  vec3 direction(
    cos(player_.v_angle) * sin(player_.h_angle), 
    sin(player_.v_angle),
    cos(player_.v_angle) * cos(player_.h_angle)
  );
  
  vec3 right = glm::vec3(
    sin(player_.h_angle - 3.14f/2.0f), 
    0,
    cos(player_.h_angle - 3.14f/2.0f)
  );

  vec3 front = glm::vec3(
    cos(player_.v_angle) * sin(player_.h_angle), 
    0,
    cos(player_.v_angle) * cos(player_.h_angle)
  );
  
  glm::vec3 up = glm::cross(right, direction);
  
  GLFWwindow* window = renderer.window();

  // Move forward.
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    player_.position += front * PLAYER_SPEED;

  // Move backward.
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    player_.position -= front * PLAYER_SPEED;

  // Strafe right.
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    player_.position += right * PLAYER_SPEED;

  // Strafe left.
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    player_.position -= right * PLAYER_SPEED;

  // Move up.
  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    player_.position += up * PLAYER_SPEED;

  // Move down.
  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    player_.position -= up * PLAYER_SPEED;

  double x_pos, y_pos;
  glfwGetCursorPos(window, &x_pos, &y_pos);
  glfwSetCursorPos(window, 0, 0);

  // Change orientation.
  float mouse_sensitivity = 0.003f;
  Player& p = player_;
  p.h_angle += mouse_sensitivity * float(-x_pos);
  p.v_angle += mouse_sensitivity * float(-y_pos);
  if (p.v_angle < -1.57f) p.v_angle = -1.57f;
  if (p.v_angle >  1.57f) p.v_angle = +1.57f;
  last_time = current_time;

  renderer.SetCamera(Camera(player_.position, direction, up));
}

int main() {
  renderer.Init("shaders");
  renderer.CreateCube(vec3(1.0, 1.0, 1.0));
  renderer.LoadFbx("fish10.fbx");

  renderer.Run(ProcessGameInput);
  return 0;
}
