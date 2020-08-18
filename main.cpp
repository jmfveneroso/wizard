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

// Portal culling:
// http://di.ubi.pt/~agomes/tjv/teoricas/07-culling.pdf

#define PLAYER_SPEED 0.03f
// #define PLAYER_HEIGHT 1.5
#define PLAYER_HEIGHT 0.75
#define GRAVITY 0.016
#define JUMP_FORCE 0.3f

using namespace std;
using namespace glm;

struct Player {
  glm::vec3 position = glm::vec3(2000, 31, 2000);
  glm::vec3 next_position = glm::vec3(0, 0, 0);
  glm::vec3 speed = glm::vec3(0, 0, 0);
  // float h_angle = -230.991;
  float h_angle = 0;
  float v_angle = 0;
  bool can_jump = true;
};

Player player_;
Renderer renderer;

void UpdateForces() {
  Player& p = player_;
  glm::vec3 prev_pos = p.position;

  p.speed += glm::vec3(0, -GRAVITY, 0);

  // Friction.
  p.speed.x *= 0.9;
  p.speed.y *= 0.99;
  p.speed.z *= 0.9;

  vec3 old_player_pos = player_.position;
  p.position += p.speed;

  renderer.Collide(&player_.position, old_player_pos, &player_.speed, &player_.can_jump);

  // Test collision with terrain.
  float height = renderer.terrain()->GetHeight(player_.position.x, player_.position.z) + PLAYER_HEIGHT;
  if (p.position.y - PLAYER_HEIGHT < height) {
    glm::vec3 pos = p.position;
    pos.y = height + PLAYER_HEIGHT;
    p.position = pos;
    glm::vec3 speed = p.speed;
    if (speed.y < 0) speed.y = 0.0f;
    p.speed = speed;
    p.can_jump = true;
  }
}

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
    player_.speed += front * PLAYER_SPEED;

  // Move backward.
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    player_.speed -= front * PLAYER_SPEED;

  // Strafe right.
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    player_.speed += right * PLAYER_SPEED;

  // Strafe left.
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    player_.speed -= right * PLAYER_SPEED;

  // Move up.
  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
    if (player_.can_jump) {
      player_.can_jump = false;
      player_.speed.y += JUMP_FORCE;
    }
  }

  // Move down.
  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    player_.position -= vec3(0, 1, 0) * PLAYER_SPEED;

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
  UpdateForces();

  renderer.SetCamera(Camera(player_.position + vec3(0, 0.75, 0), direction, up));
}

int main() {
  renderer.Init("shaders");
  // renderer.CreateCube(vec3(1.0, 1.0, 1.0), vec3(2010, 275, 2010));
  // renderer.CreateCube(vec3(100, 20, 100), vec3(2040, 20, 2040));
  renderer.LoadFbx("fish10.fbx", vec3(2042, 180, 2010));

  // renderer.LoadStaticFbx("tower_floor.fbx", vec3(2000, 172, 2000));

  // renderer.LoadStaticFbx("four_walls.fbx", vec3(2020, 170, 2000));
  // renderer.LoadStaticFbx("first_floor_walls.fbx", vec3(2020, 170, 2000));
  // renderer.LoadStaticFbx("tower_floor.fbx", vec3(2042, 172.8, 2010));
  // renderer.LoadStaticFbx("tower_walls.fbx", vec3(2042, 172.7, 2010));
  // renderer.LoadStaticFbx("tower_ramp.fbx",  vec3(2042, 172.8, 2010));
  // renderer.LoadStaticFbx("single_triangle.fbx", vec3(2020, 175, 2000));
  // renderer.LoadStaticFbx("single_wall.fbx", vec3(2020, 170, 2000));

  renderer.LoadSector("tower_first_floor_sector.fbx", 1, 
    vec3(2042, 172.8, 2010));

  renderer.LoadPortal("tower_first_floor_portal.fbx", 0, 
    vec3(2042, 172.8, 2010));

  renderer.LoadOccluder("tower_first_floor_occluder.fbx", 0,
    vec3(2042, 172.8, 2010));

  renderer.LoadStaticFbx("tower_outer_wall.fbx", vec3(2042, 172.8, 2010), 0, 0);

  renderer.LoadStaticFbx("tower_inner_wall.fbx", vec3(2042, 172.8, 2010), 1);

  renderer.LoadStaticFbx("wooden_box.fbx", vec3(2052, 190, 2010), 1);

  renderer.LoadStaticFbx("wooden_box.fbx", vec3(2047, 190, 2010), 1);

  renderer.LoadStaticFbx("wooden_box.fbx", vec3(2040, 190, 2010), 1);

  renderer.LoadStaticFbx("stone_pillar.fbx", vec3(2052, 180, 2010), 1);

  renderer.LoadStaticFbx("stone_pillar.fbx", vec3(2052, 180, 2050), 0);

  renderer.CreateCube(vec3(10, 10, 10), vec3(2000, 200, 2000));

  renderer.Run(ProcessGameInput);
  return 0;
}
