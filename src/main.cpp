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

// Portal culling:
// http://di.ubi.pt/~agomes/tjv/teoricas/07-culling.pdf

using namespace std;
using namespace glm;

int throttle_counter = 0;

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

void PressCharCallback(GLFWwindow* window, unsigned int char_code) {
  text_editor->PressCharCallback(string(1, (char) char_code));
}

void PressKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  text_editor->PressKeyCallback(key, scancode, action, mods);
  craft->PressKeyCallback(key, scancode, action, mods);
  dialog->PressKeyCallback(key, scancode, action, mods);
}

// TODO: move this elsewhere.
void RunCommand(string command) {
  shared_ptr<Player> player = asset_catalog->GetPlayer();
  shared_ptr<Configs> configs = asset_catalog->GetConfigs();

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
  } else if (result[0] == "spells") {
    player->num_spells += 100;
    player->num_spells_2 += 100;
  } else if (result[0] == "speed") {
    if (result.size() == 2) {
      float speed = boost::lexical_cast<float>(result[1]);
      configs->target_player_speed = speed;
    }
  } else if (result[0] == "raise-factor") {
    float raise_factor = boost::lexical_cast<float>(result[1]);
    configs->raise_factor = raise_factor;
  } else if (result[0] == "sun") {
    float x = boost::lexical_cast<float>(result[1]);
    float y = boost::lexical_cast<float>(result[2]);
    float z = boost::lexical_cast<float>(result[3]);
    configs->sun_position = vec3(x, y, z);
  } else if (result[0] == "levitate") {
    configs->levitate = true;
  } else if (result[0] == "nolevitate") {
    configs->levitate = false;
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
  } else if (result[0] == "disable-attacks") {
    configs->disable_attacks = true;
  } else if (result[0] == "allow-attacks") {
    configs->disable_attacks = false;
  } else if (result[0] == "edit-tile") {
    configs->edit_terrain = "tile";
  } else if (result[0] == "edit-height") {
    configs->edit_terrain = "height";
  } else if (result[0] == "edit-flatten") {
    configs->edit_terrain = "flatten";
  } else if (result[0] == "noedit") {
    configs->edit_terrain = "none";
  } else if (result[0] == "create") {
    string asset_name = result[1];
    if (asset_catalog->GetAssetGroupByName(asset_name)) {
      configs->new_building = asset_catalog->CreateGameObjFromAsset(
        asset_name, vec3(0));
      configs->place_object = true;
      configs->new_building->torque = vec3(0, 0.02f, 0);
      asset_catalog->AddNewObject(configs->new_building);
    }
  } else if (result[0] == "save") {
    asset_catalog->SaveHeightMap("assets/height_map2_compressed.dat");
  } else if (result[0] == "save-objs") {
    asset_catalog->SaveNewObjects();
  }
}

bool ProcessGameInput() {
  asset_catalog->UpdateFrameStart();
  asset_catalog->UpdateMissiles();

  --throttle_counter;
  GLFWwindow* window = renderer->window();

  shared_ptr<Configs> configs = asset_catalog->GetConfigs();
  switch (asset_catalog->GetGameState()) {
    case STATE_GAME: { 
      shared_ptr<Player> player = asset_catalog->GetPlayer();

      // TODO: all of this should go into a class player.
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          text_editor->Enable();
      
          stringstream ss;
          ss << "Player pos: " << player->position << endl;

          shared_ptr<Sector> s = asset_catalog->GetSector(player->position + vec3(0, 0.75, 0));
          ss << "Sector: " << s->name << endl;

          ss << "Life: " << player->life << endl;

          if (configs->edit_terrain != "none") {
            ss << "Brush size: " << configs->brush_size << endl;
            ss << "Mode: " << configs->edit_terrain << endl;
            ss << "Selected tile: " << configs->selected_tile << endl;
            ss << "Raise factor: " << configs->raise_factor << endl;
          }

          text_editor->SetContent(ss.str());
          asset_catalog->SetGameState(STATE_EDITOR);
        }
        throttle_counter = 20;
      } else if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          inventory->Enable();
          asset_catalog->SetGameState(STATE_INVENTORY);
        }
        throttle_counter = 20;
      } else if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          configs->brush_size--;
          if (configs->brush_size < 0) {
            configs->brush_size = 0;
          }
        }
        throttle_counter = 4;
      } else if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          configs->brush_size++;
          if (configs->brush_size > 1000) {
            configs->brush_size = 1000;
          }
        }
        throttle_counter = 4;
      } else if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          configs->selected_tile = 0;
        }
        throttle_counter = 20;
      } else if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          configs->selected_tile = 1;
        }
        throttle_counter = 20;
      } else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          configs->selected_tile = 2;
        }
        throttle_counter = 20;
      } else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          configs->selected_tile = 3;
        }
        throttle_counter = 20;
      }

      Camera c = player_input->ProcessInput(window);
      item->ProcessItems();
      craft->ProcessCrafting();
      npc->ProcessNpcs();

      renderer->SetCamera(c);

      ai->RunSpiderAI();
      physics->Run();
      asset_catalog->UpdateParticles();

      collision_resolver->Collide();

      configs->taking_hit -= 1.0f;
      if (configs->taking_hit < 0.0) {
        configs->player_speed = configs->target_player_speed;
      } else {
        configs->player_speed = configs->target_player_speed / 6.0f;
      }

      asset_catalog->RemoveDead();
      configs->sun_position = vec3(rotate(mat4(1.0f), 0.001f, vec3(0.0, 0, 1.0)) 
        * vec4(configs->sun_position, 1.0f));

      return false;
    }
    case STATE_EDITOR: {
      if (!text_editor->enabled) {
        asset_catalog->SetGameState(STATE_GAME);
      }
      return true;
    }
    case STATE_INVENTORY: {
      if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          inventory->Disable();
        }
        throttle_counter = 20;
      }
      if (!inventory->enabled) {
        asset_catalog->SetGameState(STATE_GAME);
      }
      return true;
    }
    case STATE_CRAFT: {
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          craft->Disable();
        }
        throttle_counter = 20;
      }
      if (!craft->enabled) {
        asset_catalog->SetGameState(STATE_GAME);
      }
      return true;
    }
    case STATE_DIALOG: {
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          dialog->Disable();
        }
        throttle_counter = 20;
      }
      if (!dialog->enabled) {
        asset_catalog->SetGameState(STATE_GAME);
      }
      return true;
    }
    case STATE_BUILD: {
      Camera c = player_input->ProcessInput(window);
      renderer->SetCamera(c);

      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        asset_catalog->SetGameState(STATE_GAME);
        ObjPtr player = asset_catalog->GetPlayer();
        shared_ptr<Configs> configs = asset_catalog->GetConfigs();
        player->position = configs->old_position;
        throttle_counter = 20;
        asset_catalog->RemoveObject(configs->new_building);
      }
      return true;
    }
    default:
      return true;
  }
}

void AfterFrame() {
  switch (asset_catalog->GetGameState()) {
    case STATE_GAME: {
      renderer->DrawHand();
      break;
    }
    case STATE_EDITOR: {
      text_editor->Draw();
      break;
    }
    case STATE_INVENTORY: {
      inventory->Draw();
      break;
    }
    case STATE_CRAFT: {
      craft->Draw();
      break;
    }
    case STATE_DIALOG: {
      dialog->Draw();
      break;
    }
    default:
      break; 
  }
}

int main() {
  renderer = make_shared<Renderer>();
  asset_catalog = make_shared<AssetCatalog>("assets");
  shared_ptr<Draw2D> draw_2d = make_shared<Draw2D>(asset_catalog);
  project_4d  = make_shared<Project4D>(asset_catalog);

  renderer->set_project_4d(project_4d);
  renderer->set_asset_catalog(asset_catalog);
  renderer->set_draw_2d(draw_2d);
  renderer->Init();

  physics = make_shared<Physics>(asset_catalog);

  collision_resolver = make_shared<CollisionResolver>(asset_catalog);

  ai = make_shared<AI>(asset_catalog);
  ai->InitSpider();

  text_editor = make_shared<TextEditor>(draw_2d);
  inventory = make_shared<Inventory>(asset_catalog, draw_2d);
  craft = make_shared<Craft>(asset_catalog, draw_2d, project_4d);
  dialog = make_shared<Dialog>(asset_catalog, draw_2d);
  npc = make_shared<Npc>(asset_catalog, ai);

  glfwSetCharCallback(renderer->window(), PressCharCallback);
  glfwSetKeyCallback(renderer->window(), PressKeyCallback);
  text_editor->set_run_command_fn(RunCommand);

  player_input = make_shared<PlayerInput>(asset_catalog, project_4d, craft, 
    renderer->terrain(), dialog);
  item = make_shared<Item>(asset_catalog);

  renderer->Draw(ProcessGameInput, AfterFrame);
  return 0;
}
