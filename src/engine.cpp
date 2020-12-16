#include "engine.hpp"

// TODO: move this elsewhere.
void Engine::RunCommand(string command) {
  shared_ptr<Player> player = asset_catalog_->GetPlayer();
  shared_ptr<Configs> configs = asset_catalog_->GetConfigs();

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

        TerrainPoint p = asset_catalog_->GetTerrainPoint(top_left.x + x, top_left.y + y);
        p.height += h;
        asset_catalog_->SetTerrainPoint(top_left.x + x, top_left.y + y, p);
      }
    }
    renderer_->terrain()->Invalidate();
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
    if (asset_catalog_->GetAssetGroupByName(asset_name)) {
      configs->new_building = asset_catalog_->CreateGameObjFromAsset(
        asset_name, vec3(0));
      configs->place_object = true;
      configs->new_building->torque = vec3(0, 0.02f, 0);
      asset_catalog_->AddNewObject(configs->new_building);
    }
  } else if (result[0] == "save") {
    asset_catalog_->SaveHeightMap("assets/height_map2_compressed.dat");
  } else if (result[0] == "save-objs") {
    asset_catalog_->SaveNewObjects();
  }
}

bool Engine::ProcessGameInput() {
  asset_catalog_->UpdateFrameStart();
  asset_catalog_->UpdateMissiles();

  --throttle_counter;
  GLFWwindow* window = renderer_->window();

  shared_ptr<Configs> configs = asset_catalog_->GetConfigs();
  switch (asset_catalog_->GetGameState()) {
    case STATE_GAME: { 
      shared_ptr<Player> player = asset_catalog_->GetPlayer();

      // TODO: all of this should go into a class player.
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          text_editor_->Enable();
      
          stringstream ss;
          ss << "Player pos: " << player->position << endl;

          shared_ptr<Sector> s = asset_catalog_->GetSector(player->position + vec3(0, 0.75, 0));
          ss << "Sector: " << s->name << endl;

          ss << "Life: " << player->life << endl;

          if (configs->edit_terrain != "none") {
            ss << "Brush size: " << configs->brush_size << endl;
            ss << "Mode: " << configs->edit_terrain << endl;
            ss << "Selected tile: " << configs->selected_tile << endl;
            ss << "Raise factor: " << configs->raise_factor << endl;
          }

          text_editor_->SetContent(ss.str());
          asset_catalog_->SetGameState(STATE_EDITOR);
        }
        throttle_counter = 20;
      } else if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          inventory_->Enable();
          asset_catalog_->SetGameState(STATE_INVENTORY);
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

      Camera c = player_input_->ProcessInput(window);
      item_->ProcessItems();
      craft_->ProcessCrafting();
      npc_->ProcessNpcs();

      renderer_->SetCamera(c);

      ai_->RunSpiderAI();
      physics_->Run();
      asset_catalog_->UpdateParticles();

      collision_resolver_->Collide();

      configs->taking_hit -= 1.0f;
      if (configs->taking_hit < 0.0) {
        configs->player_speed = configs->target_player_speed;
      } else {
        configs->player_speed = configs->target_player_speed / 6.0f;
      }

      asset_catalog_->RemoveDead();
      configs->sun_position = vec3(rotate(mat4(1.0f), 0.001f, vec3(0.0, 0, 1.0)) 
        * vec4(configs->sun_position, 1.0f));

      return false;
    }
    case STATE_EDITOR: {
      if (!text_editor_->enabled) {
        asset_catalog_->SetGameState(STATE_GAME);
      }
      return true;
    }
    case STATE_INVENTORY: {
      if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          inventory_->Disable();
        }
        throttle_counter = 20;
      }
      if (!inventory_->enabled) {
        asset_catalog_->SetGameState(STATE_GAME);
      }
      return true;
    }
    case STATE_CRAFT: {
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          craft_->Disable();
        }
        throttle_counter = 20;
      }
      if (!craft_->enabled) {
        asset_catalog_->SetGameState(STATE_GAME);
      }
      return true;
    }
    case STATE_DIALOG: {
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        if (throttle_counter < 0) {
          dialog_->Disable();
        }
        throttle_counter = 20;
      }
      if (!dialog_->enabled) {
        asset_catalog_->SetGameState(STATE_GAME);
      }
      return true;
    }
    case STATE_BUILD: {
      Camera c = player_input_->ProcessInput(window);
      renderer_->SetCamera(c);

      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        asset_catalog_->SetGameState(STATE_GAME);
        ObjPtr player = asset_catalog_->GetPlayer();
        shared_ptr<Configs> configs = asset_catalog_->GetConfigs();
        player->position = configs->old_position;
        throttle_counter = 20;
        asset_catalog_->RemoveObject(configs->new_building);
      }
      return true;
    }
    default:
      return true;
  }
}

void Engine::AfterFrame() {
  switch (asset_catalog_->GetGameState()) {
    case STATE_GAME: {
      renderer_->DrawHand();
      break;
    }
    case STATE_EDITOR: {
      text_editor_->Draw();
      break;
    }
    case STATE_INVENTORY: {
      inventory_->Draw();
      break;
    }
    case STATE_CRAFT: {
      craft_->Draw();
      break;
    }
    case STATE_DIALOG: {
      dialog_->Draw();
      break;
    }
    default:
      break; 
  }
}

Engine::Engine(
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
) : project_4d_(project_4d), renderer_(renderer), text_editor_(text_editor),
    inventory_(inventory), craft_(craft), asset_catalog_(asset_catalog),
    collision_resolver_(collision_resolver), ai_(ai), physics_(physics),
    player_input_(player_input), item_(item), dialog_(dialog), npc_(npc) {
}

void Engine::Run() {
  // text_editor_->set_run_command_fn(RunCommand);
  // renderer_->Draw(ProcessGameInput, AfterFrame);
}
