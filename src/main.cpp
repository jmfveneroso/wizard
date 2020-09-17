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
#include "text_editor.hpp"
#include "collision_resolver.hpp"
#include "ai.hpp"

// Portal culling:
// http://di.ubi.pt/~agomes/tjv/teoricas/07-culling.pdf

#define PLAYER_SPEED 0.03f
// #define PLAYER_SPEED 0.6f
// #define PLAYER_SPEED 0.3f
// #define PLAYER_HEIGHT 1.5
#define PLAYER_HEIGHT 0.75
#define GRAVITY 0.016
// #define JUMP_FORCE 30.0f
#define JUMP_FORCE 0.3f
#define PLAYER_START_POSITION vec3(10000, 220, 10000)

using namespace std;
using namespace glm;

bool text_mode = false;
int throttle_counter = 0;

shared_ptr<Renderer> renderer = nullptr;
shared_ptr<TextEditor> text_editor = nullptr;
shared_ptr<AssetCatalog> asset_catalog = nullptr;
shared_ptr<CollisionResolver> collision_resolver = nullptr;
shared_ptr<AI> ai = nullptr;

void PressCharCallback(GLFWwindow* window, unsigned int char_code) {
  text_editor->PressCharCallback(string(1, (char) char_code));
}

void PressKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  text_editor->PressKeyCallback(key, scancode, action, mods);
}

void UpdateForces() {
  shared_ptr<Player> p = asset_catalog->GetPlayer();
  glm::vec3 prev_pos = p->position;

  p->speed += glm::vec3(0, -GRAVITY, 0);

  // Friction.
  p->speed.x *= 0.9;
  p->speed.y *= 0.99;
  p->speed.z *= 0.9;

  vec3 old_player_pos = p->position;
  p->position += p->speed;

  collision_resolver->Collide(&p->position, old_player_pos, &p->speed, &p->can_jump);
}

void UpdateParticleForces() {
}

void RunCommand(string command) {
  shared_ptr<Player> player = asset_catalog->GetPlayer();

  vector<string> result; 
  boost::split(result, command, boost::is_any_of(" ")); 
  if (result.empty()) {
    cout << "result empty" << endl;
    return;
  }

  if (result[0] == "move") {
    float x = boost::lexical_cast<float>(result[1]);
    float y = boost::lexical_cast<float>(result[2]);
    float z = boost::lexical_cast<float>(result[3]);
    player->position = vec3(x, y, z);
  } else if (result[0] == "raise") {
    ivec2 top_left = ivec2(player->position.x, player->position.z) - 40;
    for (int x = 0; x < 80; x++) {
      for (int y = 0; y < 80; y++) {
        float x_ = x / 10.0f - 4.0f;
        float y_ = y / 10.0f - 4.0f;
        float h = (50.0f / (2.0f * 3.14f)) * exp(-0.5 * (x_*x_ + y_*y_));

        TerrainPoint p = asset_catalog->GetTerrainPoint(top_left.x + x, top_left.y + y);
        p.height += h;
        asset_catalog->SetTerrainPoint(top_left.x + x, top_left.y + y, p);
      }
    }
    renderer->terrain()->Invalidate();
    cout << "Raised terrain" << endl; 
  } else if (result[0] == "save") {
    asset_catalog->SaveHeightMap("assets/height_map2.dat");
  }
}

bool ProcessGameInput() {
  static double last_time = glfwGetTime();
  double current_time = glfwGetTime();

  --throttle_counter;
  GLFWwindow* window = renderer->window();

  shared_ptr<Player> player = asset_catalog->GetPlayer();
  if (text_mode) { 
    if (!text_editor->enabled) {
      text_mode = false;
    }
    return true;
  } else {
    vec3 direction(
      cos(player->v_angle) * sin(player->h_angle), 
      sin(player->v_angle),
      cos(player->v_angle) * cos(player->h_angle)
    );
    
    vec3 right = glm::vec3(
      sin(player->h_angle - 3.14f/2.0f), 
      0,
      cos(player->h_angle - 3.14f/2.0f)
    );

    vec3 front = glm::vec3(
      cos(player->v_angle) * sin(player->h_angle), 
      0,
      cos(player->v_angle) * cos(player->h_angle)
    );
    
    glm::vec3 up = glm::cross(right, direction);
    
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
      if (throttle_counter < 0) {
        text_editor->Enable();
  
        stringstream ss;
        ss << "Player pos: " << player->position << endl;

        shared_ptr<Sector> s = asset_catalog->GetSector(player->position + vec3(0, 0.75, 0));
        ss << "Sector: " << s->name << endl;

        ss << "Life: " << asset_catalog->GetGameData()->life << endl;
        text_editor->SetContent(ss.str());
        text_mode = true;
      }
      throttle_counter = 20;
    }
    
    // Move forward.
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      player->speed += front * PLAYER_SPEED;

    // Move backward.
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      player->speed -= front * PLAYER_SPEED;

    // Strafe right.
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      player->speed += right * PLAYER_SPEED;

    // Strafe left.
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      player->speed -= right * PLAYER_SPEED;

    // Move up.
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
      if (player->can_jump) {
        player->can_jump = false;
        player->speed.y += JUMP_FORCE;
      }
    }

    // Move down.
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
      player->position -= vec3(0, 1, 0) * PLAYER_SPEED;

    double x_pos, y_pos;
    glfwGetCursorPos(window, &x_pos, &y_pos);
    glfwSetCursorPos(window, 0, 0);

    // Change orientation.
    float mouse_sensitivity = 0.003f;
    player->h_angle += mouse_sensitivity * float(-x_pos);
    player->v_angle += mouse_sensitivity * float(-y_pos);
    if (player->v_angle < -1.57f) player->v_angle = -1.57f;
    if (player->v_angle >  1.57f) player->v_angle = +1.57f;
    last_time = current_time;

    Camera c = Camera(player->position + vec3(0, 0.75, 0), direction, up);
    c.rotation.x = player->v_angle;
    c.rotation.y = player->h_angle;

    renderer->SetCamera(c);

    static int debounce = 0;
    --debounce;
    static int animation_frame = 0;
    --animation_frame;
    if (animation_frame == 0 && glfwGetKey(window, GLFW_KEY_C) != GLFW_PRESS) {
      shared_ptr<GameObject> obj = asset_catalog->GetObjectByName("hand-001");
      obj->active_animation = "Armature|idle";
      obj->frame = 0;
    } else if (animation_frame <= 0 && glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
      shared_ptr<GameObject> obj = asset_catalog->GetObjectByName("hand-001");
      obj->active_animation = "Armature|shoot";
      obj->frame = 0;
      animation_frame  = 60;
      collision_resolver->ChargeMagicMissile(c);
    } else if (animation_frame == 20) {
      collision_resolver->CastMagicMissile(c);
    }

    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
      if (debounce < 0) {
        shared_ptr<GameObject> obj = asset_catalog->GetObjectByName("hand-001");
        obj->active_animation = "Armature|shoot";
        obj->frame = 0;
        animation_frame = 60;
      }
      debounce = 20;
    }

    UpdateForces();
    UpdateParticleForces();
    asset_catalog->UpdateParticles();

    ai->RunSpiderAI(c);
    ai->UpdateSpiderForces();
    collision_resolver->CollideSpider();

    collision_resolver->UpdateMagicMissile(c);

    return false;
  }
}

void AfterFrame() {
  if (text_mode) {
    text_editor->Draw();
  }
}

int main() {
  renderer = make_shared<Renderer>();
  asset_catalog = make_shared<AssetCatalog>("assets");
  shared_ptr<Draw2D> draw_2d = make_shared<Draw2D>(
    asset_catalog->GetShader("text"), 
    asset_catalog->GetShader("polygon"));
  renderer->set_asset_catalog(asset_catalog);
  renderer->set_draw_2d(draw_2d);
  renderer->Init();

  collision_resolver = make_shared<CollisionResolver>(asset_catalog);
  collision_resolver->InitMagicMissile();

  ai = make_shared<AI>(asset_catalog);
  ai->InitSpider();

  text_editor = make_shared<TextEditor>(draw_2d);

  glfwSetCharCallback(renderer->window(), PressCharCallback);
  glfwSetKeyCallback(renderer->window(), PressKeyCallback);
  text_editor->set_run_command_fn(RunCommand);

  renderer->Run(ProcessGameInput, AfterFrame);
  return 0;
}
