#ifndef __ENGINE_HPP__
#define __ENGINE_HPP__

#include "renderer.hpp"
#include "text_editor.hpp"
#include "collision_resolver.hpp"
#include "ai.hpp"
#include "physics.hpp"
#include "player.hpp"
#include "inventory.hpp"
#include "scripts.hpp"

#include <thread>
#include <mutex>
#include <chrono>

using namespace std;
using namespace glm;

class Engine {
  shared_ptr<Project4D> project_4d_ = nullptr;
  shared_ptr<Renderer> renderer_ = nullptr;
  shared_ptr<TextEditor> text_editor_ = nullptr;
  shared_ptr<Inventory> inventory_ = nullptr;
  shared_ptr<Resources> resources_ = nullptr;
  shared_ptr<CollisionResolver> collision_resolver_ = nullptr;
  shared_ptr<AI> ai_ = nullptr;
  shared_ptr<Physics> physics_ = nullptr;
  shared_ptr<PlayerInput> player_input_ = nullptr;
  ObjPtr line_obj_ = nullptr;

  float delta_time_ = 0.0f;
  int window_width_ = WINDOW_WIDTH;
  int window_height_ = WINDOW_HEIGHT;
  GLFWwindow* window_;
  int throttle_counter_ = 0;

  bool terminate_ = false;
  thread collision_thread_;
  thread ai_thread_;
  thread periodic_events_thread_;

  void RunCommand(string command);
  bool ProcessGameInput();
  void BeforeFrameDebug();
  void BeforeFrame();
  void AfterFrame();
  void UpdateAnimationFrames();
  void RunPeriodicEventsAsync();

  // Debug.
  unordered_map<string, ObjPtr> door_obbs_;
  ObjPtr my_box_;
  void RunBeforeFrameDebugFunctions();

 public:
  Engine(
    shared_ptr<Project4D> project_4d,
    shared_ptr<Renderer> renderer,
    shared_ptr<TextEditor> text_editor,
    shared_ptr<Inventory> inventory,
    shared_ptr<Resources> asset_catalog,
    shared_ptr<CollisionResolver> collision_resolver,
    shared_ptr<AI> ai,
    shared_ptr<Physics> physics,
    shared_ptr<PlayerInput> player_input,
    GLFWwindow* window,
    int window_width,
    int window_height
  );

  void Run();
};

#endif
