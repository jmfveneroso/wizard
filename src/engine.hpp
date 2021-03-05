#ifndef __ENGINE_HPP__
#define __ENGINE_HPP__

#include "renderer.hpp"
#include "text_editor.hpp"
#include "collision_resolver.hpp"
#include "ai.hpp"
#include "physics.hpp"
#include "player.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "craft.hpp"
#include "dialog.hpp"
#include "npc.hpp"
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
  shared_ptr<Craft> craft_ = nullptr;
  shared_ptr<Resources> resources_ = nullptr;
  shared_ptr<CollisionResolver> collision_resolver_ = nullptr;
  shared_ptr<AI> ai_ = nullptr;
  shared_ptr<Physics> physics_ = nullptr;
  shared_ptr<PlayerInput> player_input_ = nullptr;
  shared_ptr<Item> item_ = nullptr;
  shared_ptr<Dialog> dialog_ = nullptr;
  shared_ptr<Npc> npc_ = nullptr;
  shared_ptr<ScriptManager> script_manager_ = nullptr;

  float delta_time_ = 0.0f;
  int window_width_ = WINDOW_WIDTH;
  int window_height_ = WINDOW_HEIGHT;
  GLFWwindow* window_;
  int throttle_counter_ = 0;

  bool terminate_ = false;
  thread collision_thread_;

  void RunCommand(string command);
  bool ProcessGameInput();
  void BeforeFrameDebug();
  void BeforeFrame();
  void AfterFrame();
  void UpdateAnimationFrames();
  void ProcessCollisionsAsync();

  // Debug.
  unordered_map<string, ObjPtr> door_obbs_;
  void RunBeforeFrameDebugFunctions();

 public:
  Engine(
    shared_ptr<Project4D> project_4d,
    shared_ptr<Renderer> renderer,
    shared_ptr<TextEditor> text_editor,
    shared_ptr<Inventory> inventory,
    shared_ptr<Craft> craft,
    shared_ptr<Resources> asset_catalog,
    shared_ptr<CollisionResolver> collision_resolver,
    shared_ptr<AI> ai,
    shared_ptr<Physics> physics,
    shared_ptr<PlayerInput> player_input,
    shared_ptr<Item> item,
    shared_ptr<Dialog> dialog,
    shared_ptr<Npc> npc,
    shared_ptr<ScriptManager> script_manager,
    GLFWwindow* window,
    int window_width,
    int window_height
  );

  void Run();
};

#endif
