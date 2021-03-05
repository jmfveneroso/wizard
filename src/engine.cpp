#include "engine.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

Engine::Engine(
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
) : project_4d_(project_4d), renderer_(renderer), text_editor_(text_editor),
    inventory_(inventory), craft_(craft), resources_(asset_catalog),
    collision_resolver_(collision_resolver), ai_(ai), physics_(physics),
    player_input_(player_input), item_(item), dialog_(dialog), npc_(npc),
    script_manager_(script_manager), window_(window), 
    window_width_(window_width), window_height_(window_height) {
}

// TODO: move this elsewhere.
void Engine::RunCommand(string command) {
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();

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
  } else if (result[0] == "time") {
    configs->time_of_day = boost::lexical_cast<float>(result[1]);
  } else if (result[0] == "levitate") {
    configs->levitate = true;
  } else if (result[0] == "nolevitate") {
    configs->levitate = false;
  } else if (result[0] == "time") {
    configs->stop_time = false;
  } else if (result[0] == "notime") {
    configs->stop_time = true;
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
  } else if (result[0] == "s") {
    configs->target_player_speed = 0.2;
    configs->levitate = true;
  } else if (result[0] == "nos") {
    configs->target_player_speed = 0.04;
    configs->levitate = false;
  } else if (result[0] == "create") {
    string asset_name = result[1];
    if (resources_->GetAssetGroupByName(asset_name)) {
      cout << "Creating asset: " << asset_name << endl;
      configs->place_axis = 1;

      Camera c = player_input_->GetCamera();
      vec3 position = c.position + c.direction * 10.0f;
      configs->new_building = resources_->CreateGameObjFromAsset(
        asset_name, position);
      configs->new_building->being_placed = true;
      configs->place_object = true;
      resources_->AddNewObject(configs->new_building);
    }
  } else if (result[0] == "create-region") {
    Camera c = player_input_->GetCamera();
    vec3 position = c.position + c.direction * 10.0f;

    shared_ptr<Region> region = resources_->CreateRegion(position, vec3(10));

    configs->new_building = region;
    configs->place_object = true;
    configs->place_axis = 1;
    resources_->AddNewObject(configs->new_building);
  } else if (result[0] == "create-waypoint") {
    Camera c = player_input_->GetCamera();
    vec3 position = c.position + c.direction * 10.0f;
    configs->new_building = resources_->CreateWaypoint(position);
    configs->place_axis = 1;
    configs->place_object = true;
    resources_->AddNewObject(configs->new_building);
  } else if (result[0] == "save") {
    resources_->GetHeightMap().Save();
  } else if (result[0] == "save-objs") {
    resources_->SaveNewObjects();
  }
}

void Engine::ProcessCollisionsAsync() {
  double min_time_elapsed = 1.0f / 60.0f;

  double last_update = 0;
  while (!terminate_) {
    double cur_time = glfwGetTime();

    double time_elapsed = cur_time - last_update;
    if (time_elapsed < min_time_elapsed) {
      int ms = (min_time_elapsed - time_elapsed) * 1000;
      this_thread::sleep_for(chrono::milliseconds(ms));
    }
    last_update = cur_time;

    resources_->LockOctree();
    physics_->Run();
    collision_resolver_->Collide();
    resources_->UnlockOctree();
  }
}

bool Engine::ProcessGameInput() {
  resources_->UpdateFrameStart();
  resources_->UpdateMissiles();

  --throttle_counter_;
  GLFWwindow* window = renderer_->window();

  shared_ptr<Configs> configs = resources_->GetConfigs();
  switch (resources_->GetGameState()) {
    case STATE_GAME: { 
      shared_ptr<Player> player = resources_->GetPlayer();

      // TODO: all of this should go into a class player.
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          text_editor_->Enable();
      
          stringstream ss;
          ss << "Player pos: " << player->position << endl;

          shared_ptr<Sector> s = resources_->GetSector(player->position + vec3(0, 0.75, 0));
          ss << "Sector: " << s->name << endl;

          ss << "Life: " << player->life << endl;

          ss << "Time of day: " << configs->time_of_day << endl;

          if (configs->place_object) {
            ss << "Name: " << configs->new_building->name << endl;
          }

          text_editor_->SetContent(ss.str());
          resources_->SetGameState(STATE_EDITOR);
        }
        throttle_counter_ = 20;
      } else if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          glfwSetCursorPos(window, 640 - 32, 400 + 32);
          inventory_->Enable();
          resources_->SetGameState(STATE_INVENTORY);
        }
        throttle_counter_ = 20;
        return false;
      } else if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->brush_size-=10;
          if (configs->brush_size < 0) {
            configs->brush_size = 0;
          }
        }
        throttle_counter_ = 4;
      } else if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->brush_size += 10;
          if (configs->brush_size > 1000) {
            configs->brush_size = 1000;
          }
        }
        throttle_counter_ = 4;
      } else if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->selected_spell = 0;
        }
        throttle_counter_ = 5;
      } else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->selected_spell = 1;
        }
        throttle_counter_ = 5;
      } else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->selected_spell = 2;
        }
        throttle_counter_ = 5;
      } else if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->selected_spell = 3;
        }
        throttle_counter_ = 5;
      } else if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->selected_spell = 4;
        }
        throttle_counter_ = 5;
      } else if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->selected_spell = 5;
        }
        throttle_counter_ = 5;
      } else if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->selected_spell = 6;
        }
        throttle_counter_ = 5;
      } else if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->selected_spell = 7;
        }
        throttle_counter_ = 5;
      }
      player_input_->ProcessInput(window_);
      return false;
    }
    case STATE_EDITOR: {
      if (!text_editor_->enabled) {
        resources_->SetGameState(STATE_GAME);
      }
      return true;
    }
    case STATE_INVENTORY: {
      if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          inventory_->Disable();
          glfwSetCursorPos(window, 0, 0);
        }
        throttle_counter_ = 20;
      }
      if (!inventory_->enabled) {
        resources_->SetGameState(STATE_GAME);
      }
      return true;
    }
    case STATE_CRAFT: {
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          craft_->Disable();
        }
        throttle_counter_ = 20;
      }
      if (!craft_->enabled) {
        resources_->SetGameState(STATE_GAME);
      }
      return true;
    }
    case STATE_DIALOG: {
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          dialog_->Disable();
        }
        throttle_counter_ = 20;
      }
      if (!dialog_->enabled) {
        resources_->SetGameState(STATE_GAME);
      }
      return true;
    }
    case STATE_BUILD: {
      Camera c = player_input_->ProcessInput(window);
      renderer_->SetCamera(c);

      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        resources_->SetGameState(STATE_GAME);
        ObjPtr player = resources_->GetPlayer();
        shared_ptr<Configs> configs = resources_->GetConfigs();
        player->position = configs->old_position;
        throttle_counter_ = 20;
        resources_->RemoveObject(configs->new_building);
      }
      return true;
    }
    default:
      return true;
  }
}

void Engine::AfterFrame() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  {
    // TODO: periodic events: update frame, cooldown.
    resources_->UpdateCooldowns();
    resources_->RemoveDead();

    if (!configs->stop_time) {
      // 1 minute per second.
      configs->time_of_day += (1.0f / (60.0f * 60.0f));
      if (configs->time_of_day > 24.0f) {
        configs->time_of_day -= 24.0f;
      }

      float radians = configs->time_of_day * ((2.0f * 3.141592f) / 24.0f);
      configs->sun_position = 
        vec3(rotate(mat4(1.0f), radians, vec3(0.0, 0, 1.0)) *
        vec4(0.0f, -1.0f, 0.0f, 1.0f));
    }
  }

  switch (resources_->GetGameState()) {
    case STATE_GAME: {
      renderer_->DrawScreenEffects();
      break;
    }
    case STATE_EDITOR: {
      text_editor_->Draw();
      break;
    }
    case STATE_INVENTORY: {
      Camera c = player_input_->GetCamera();
      inventory_->Draw(c, 200, 100, window_);
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

void Engine::UpdateAnimationFrames() {
  float d = resources_->GetDeltaTime() / 0.016666f;

  unordered_map<string, shared_ptr<GameObject>>& objs = 
    resources_->GetObjects();
  for (auto& [name, obj] : objs) {
    if (obj->type == GAME_OBJ_DOOR) {
      shared_ptr<Door> door = static_pointer_cast<Door>(obj);
      switch (door->state) {
        case 0: // closed.
          door->frame = 0;
          resources_->ChangeObjectAnimation(door, "Armature|open");
          break;
        case 1: // opening.
          resources_->ChangeObjectAnimation(door, "Armature|open");
          door->frame += 1.0f * d;
          if (door->frame >= 59) {
            door->state = 2;
            door->frame = 0;
            resources_->ChangeObjectAnimation(door, "Armature|close");
          }
          break;
        case 2: // open.
          door->frame = 0;
          resources_->ChangeObjectAnimation(door, "Armature|close");
          break;
        case 3: // closing.
          resources_->ChangeObjectAnimation(door, "Armature|close");
          door->frame += 1.0f * d;
          if (door->frame >= 59) {
            door->state = 0;
            door->frame = 0;
            resources_->ChangeObjectAnimation(door, "Armature|open");
          }
          break;
      }
      continue; 
    }

    if (obj->type == GAME_OBJ_ACTIONABLE) {
      shared_ptr<Actionable> actionable = static_pointer_cast<Actionable>(obj);
      shared_ptr<Mesh> mesh = resources_->GetMesh(actionable);

      switch (actionable->state) {
        case 0: // idle.
          resources_->ChangeObjectAnimation(actionable, "Armature|idle");
          actionable->frame += 1.0f * d;
          break;
        case 1: { // start.
          resources_->ChangeObjectAnimation(actionable, "Armature|start");
          actionable->frame += 1.0f * d;
          int num_frames = GetNumFramesInAnimation(*mesh, "Armature|start");
          if (actionable->frame >= num_frames - 1) {
            actionable->state = 2;
            actionable->frame = 0;
            resources_->ChangeObjectAnimation(actionable, "Armature|on");
          }
          break;
        }
        case 2: // on.
          resources_->ChangeObjectAnimation(actionable, "Armature|on");
          actionable->frame += 1.0f * d;
          break;
        case 3: { // shutdown.
          resources_->ChangeObjectAnimation(actionable, "Armature|shutdown");
          actionable->frame++;
          int num_frames = GetNumFramesInAnimation(*mesh, "Armature|shutdown");
          if (actionable->frame >= num_frames - 1) {
            actionable->state = 0;
            actionable->frame = 0;
            resources_->ChangeObjectAnimation(actionable, "Armature|idle");
          }
          break;
        }
      }

      int num_frames = GetNumFramesInAnimation(*mesh, actionable->active_animation);
      if (actionable->frame >= num_frames) {
        actionable->frame = 0;
      }
      continue; 
    }

    if (obj->type != GAME_OBJ_DEFAULT) {
      continue;
    }

    shared_ptr<GameAsset> asset = obj->GetAsset();
    const string mesh_name = asset->lod_meshes[0];
    shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
    if (!mesh) {
      throw runtime_error(string("Mesh ") + mesh_name + " does not exist.");
    }

    const Animation& animation = mesh->animations[obj->active_animation];
    obj->frame += 1.0f * d;
    if (obj->frame >= animation.keyframes.size()) {
      obj->frame = 0;
    }
  }
}

void Engine::BeforeFrameDebug() {
  unordered_map<string, shared_ptr<GameObject>>& objs = 
    resources_->GetObjects();
  for (auto& [name, obj] : objs) {
    if (obj->type != GAME_OBJ_DOOR) continue;

    string obb_name = obj->name + "-obb";
    OBB obb = obj->GetTransformedOBB();

    vec3 w = obb.axis[0] * obb.half_widths[0];
    vec3 h = obb.axis[1] * obb.half_widths[1];
    vec3 d = obb.axis[2] * obb.half_widths[2];

    const vector<vec3> offsets {
      // Back face.
      vec3(-1, 1, -1), vec3(1, 1, -1), vec3(-1, -1, -1), vec3(1, -1, -1), 
      // Front face.
      vec3(-1, 1,  1), vec3(1, 1,  1), vec3(-1, -1,  1), vec3(1, -1, 1 )
    };

    vector<vec3> v;
    const vec3& c = obb.center;
    for (const auto& o : offsets) {
      v.push_back(c + w * o.x + h * o.y + d * o.z);
    }

    vector<vec3> vertices;
    vector<vec2> uvs;
    vector<unsigned int> indices(36);
    Mesh mesh = CreateCube(v, vertices, uvs, indices);

    if (door_obbs_.find(obb_name) == door_obbs_.end()) {
      shared_ptr<GameObject> obb_obj = 
        resources_->CreateGameObjFromPolygons(mesh.polygons, obb_name,
          obj->position);
      door_obbs_[obb_name] = obb_obj;
    } else {
      ObjPtr obb_obj = door_obbs_[obb_name];
      MeshPtr mesh = resources_->GetMesh(obb_obj);
      if (mesh == nullptr) {
        cout << "xxx---xxx" << endl;
        continue;
      }

      UpdateMesh(*mesh, vertices, uvs, indices);
    }
  }
}

void Engine::BeforeFrame() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  {
    // BeforeFrameDebug();
    UpdateAnimationFrames();
    ProcessGameInput();
    item_->ProcessItems();
    craft_->ProcessCrafting();
    npc_->ProcessNpcs();
    ai_->RunSpiderAI();
    script_manager_->ProcessScripts();
    resources_->UpdateParticles();
    physics_->Run();
    collision_resolver_->Collide();

    configs->taking_hit -= 1.0f;
    if (configs->taking_hit < 0.0) {
      configs->player_speed = configs->target_player_speed;
    } else {
      configs->player_speed = configs->target_player_speed / 6.0f;
    }
  }
}

void Engine::Run() {
  text_editor_->set_run_command_fn(std::bind(&Engine::RunCommand, this, 
    std::placeholders::_1));

  GLint major_version, minor_version;
  glGetIntegerv(GL_MAJOR_VERSION, &major_version); 
  glGetIntegerv(GL_MINOR_VERSION, &minor_version);
  cout << "Open GL version is " << major_version << "." << minor_version 
    << endl;

  vector<float> hypercube_rotation { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

  // Start threads.
  // collision_thread_ = thread(&Engine::ProcessCollisionsAsync, this);

  int frames = 0;
  double next_print_time = glfwGetTime();
  double last_time = glfwGetTime();
  do {
    frames++;

    double current_time = glfwGetTime();
    delta_time_ = current_time - last_time;
    if (current_time >= next_print_time) { 
      cout << 1000.0 / double(frames) << " ms / frame" << endl;
      next_print_time = current_time + 1.0;
      frames = 0;
    }
    last_time = current_time;
    resources_->SetDeltaTime(delta_time_);

    BeforeFrame();

    resources_->LockOctree();
    Camera c = player_input_->GetCamera();
    renderer_->SetCamera(c);
    renderer_->Draw();
    resources_->UnlockOctree();

    AfterFrame();

    // project_4d_->CreateHypercube(vec3(11623, 177, 7550), hypercube_rotation);
    // hypercube_rotation[2] += 0.02;

    glfwSwapBuffers(window_);
    glfwPollEvents();
  } while (glfwWindowShouldClose(window_) == 0);

  // Cleanup VBO and shader.
  resources_->Cleanup();
  glfwTerminate();

  terminate_ = true;

  // Join threads.
  // collision_thread_.join();
}
