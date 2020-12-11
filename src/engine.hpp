#include <fbxsdk.h>
#include <stdio.h>
#include <iostream>
#include <exception>
#include <memory>
#include <random>
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
#include "physics.hpp"
#include "player.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "craft.hpp"
#include "dialog.hpp"
#include "npc.hpp"

class Engine {
  shared_ptr<Project4D> project_4d = nullptr;
  shared_ptr<Renderer> renderer = nullptr;
  shared_ptr<TextEditor> text_editor = nullptr;
  shared_ptr<Inventory> inventory = nullptr;
  shared_ptr<Craft> craft = nullptr;
  shared_ptr<AssetCatalog> asset_catalog = nullptr;
  shared_ptr<CollisionResolver> collision_resolver = nullptr;
  shared_ptr<AI> ai = nullptr;
  shared_ptr<Physics> physics = nullptr;
  shared_ptr<PlayerInput> player_input = nullptr;
  shared_ptr<Item> item = nullptr;
  shared_ptr<Dialog> dialog = nullptr;
  shared_ptr<Npc> npc = nullptr;

 public:
  Engine(
    shared_ptr<Project4D> project_4d,
    shared_ptr<Renderer> renderer,
    shared_ptr<TextEditor> text_editor,
    shared_ptr<Inventory> inventory,
    shared_ptr<Craft> craft,
    shared_ptr<AssetCatalog> asset_catalog,
    shared_ptr<CollisionResolver> collision_resolver,
    shared_ptr<AI> ai,
    shared_ptr<Physics> physics,
    shared_ptr<PlayerInput> player_input,
    shared_ptr<Item> item,
    shared_ptr<Dialog> dialog,
    shared_ptr<Npc> npc
  );
};
