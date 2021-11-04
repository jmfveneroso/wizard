#include "engine.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

Engine::Engine(
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
) : project_4d_(project_4d), 
    renderer_(renderer), 
    text_editor_(text_editor),
    inventory_(inventory), 
    resources_(asset_catalog),
    collision_resolver_(collision_resolver), 
    ai_(ai), 
    physics_(physics),
    player_input_(player_input), 
    window_(window), window_width_(window_width), 
    window_height_(window_height) {
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
  } else if (result[0] == "speed") {
    if (result.size() == 2) {
      try {
        float speed = boost::lexical_cast<float>(result[1]);
        configs->max_player_speed = speed;
      } catch(boost::bad_lexical_cast const& e) {
      }
    }
  } else if (result[0] == "raise-factor") {
    float raise_factor = boost::lexical_cast<float>(result[1]);
    configs->raise_factor = raise_factor;
  } else if (result[0] == "time") {
    configs->time_of_day = boost::lexical_cast<float>(result[1]);
  } else if (result[0] == "levitate") {
    configs->levitate = true;
  } else if (result[0] == "self-harm") {
    player->life -= 10.0f;
  } else if (result[0] == "nolevitate") {
    configs->levitate = false;
  } else if (result[0] == "time") {
    configs->stop_time = false;
  } else if (result[0] == "notime") {
    configs->stop_time = true;
  } else if (result[0] == "disable-attacks") {
    configs->disable_attacks = true;
  } else if (result[0] == "disable-collision") {
    configs->disable_collision = true;
  } else if (result[0] == "enable-collision") {
    configs->disable_collision = false;
  } else if (result[0] == "disable-ai") {
    configs->disable_ai = true;
  } else if (result[0] == "enable-ai") {
    configs->disable_ai = false;
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
    configs->max_player_speed = 0.2;
    configs->levitate = true;
  } else if (result[0] == "nos") {
    configs->max_player_speed = 0.04;
    configs->levitate = false;
  } else if (result[0] == "overlay") {
    configs->overlay = "wizard-farm-concept";
  } else if (result[0] == "nooverlay") {
    configs->overlay = "";
  } else if (result[0] == "level-up") {
    resources_->GiveExperience(10);
  } else if (result[0] == "level-down") {
    configs->level--;
    configs->experience = 0;
  } else if (result[0] == "arcane-level-up") {
    configs->arcane_level++;
  } else if (result[0] == "arcane-level-down") {
    configs->arcane_level--;
  } else if (result[0] == "invincible") {
    configs->invincible = true;
  } else if (result[0] == "full-life" || result[0] == "fl") {
    player->life = player->max_life;
    player->mana = player->max_mana;
    player->stamina = player->max_stamina;

    player->status = STATUS_NONE;
  } else if (result[0] == "count-octree") {
    resources_->CountOctreeNodes();
  } else if (result[0] == "create") {
    string asset_name = result[1];
    if (resources_->GetAssetGroupByName(asset_name)) {
      configs->place_axis = 1;

      Camera c = player_input_->GetCamera();
      vec3 position = c.position + c.direction * 10.0f;

      configs->new_building = CreateGameObjFromAsset(resources_.get(), 
        asset_name, position);

      configs->new_building->being_placed = true;
      configs->place_object = true;
      resources_->AddNewObject(configs->new_building);

      if (asset_name == "scepter") {
        configs->new_building->torque = vec3(0.2, 0.2, 0);
      }
    }
  } else if (result[0] == "create-particle") {
    string asset_name = result[1];
    if (resources_->GetAssetGroupByName(asset_name)) {
      configs->place_axis = 1;

      Camera c = player_input_->GetCamera();
      vec3 position = c.position + c.direction * 10.0f;

      configs->new_building = resources_->Create3dParticleEffect(asset_name, position);
      configs->new_building->being_placed = true;
      configs->place_object = true;
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
  } else if (result[0] == "gold") {
    int gold_quantity = boost::lexical_cast<int>(result[1]);
    configs->gold = gold_quantity;
  } else if (result[0] == "save") {
    resources_->GetHeightMap().Save();
  } else if (result[0] == "save-objs") {
    resources_->SaveObjects();
  } else if (result[0] == "save-collision") {
    resources_->SaveCollisionData();
  } else if (result[0] == "dungeon") {
    try {
      int dungeon_level = boost::lexical_cast<int>(result[1]);
      configs->dungeon_level = dungeon_level;
      resources_->DeleteAllObjects();
      resources_->CreateDungeon();
      Dungeon& dungeon = resources_->GetDungeon();
      vec3 pos = dungeon.GetUpstairs();
      resources_->GetPlayer()->ChangePosition(pos);
      resources_->GetConfigs()->render_scene = "dungeon";
      resources_->SaveGame();
      resources_->CalculateCollisionData();
      resources_->GenerateOptimizedOctree();
    } catch(boost::bad_lexical_cast const& e) {
    }
  } else if (result[0] == "town") {
    resources_->DeleteAllObjects();
    resources_->CreateTown();
    resources_->LoadCollisionData("resources/objects/collision_data.xml");
    resources_->CalculateCollisionData();
    resources_->GenerateOptimizedOctree();
    resources_->GetConfigs()->render_scene = "town";
    resources_->GetPlayer()->ChangePosition(vec3(11787, 300, 7742));
    resources_->GetScriptManager()->LoadScripts();
    resources_->SaveGame();
  } else if (result[0] == "delete-all") {
    cout << "im here" << endl;
    resources_->DeleteAllObjects();
  } else if (result[0] == "save-game") {
    resources_->SaveGame();
  } else if (result[0] == "load-game") {
    resources_->LoadGame("config.xml", false);
  } else if (result[0] == "new-game") {
    resources_->LoadGame("start_config.xml", false);
    resources_->SaveGame();
  } else if (result[0] == "create-cylinder") {
    Camera c = player_input_->GetCamera();
    vector<vec3> vertices;
    vector<vec2> uvs;
    vector<unsigned int> indices;
    vector<Polygon> polygons;

    CreateCylinder(vec3(0), vec3(100.0f, 100.0f, 100.0f), 10.0f, vertices, uvs, indices,
      polygons);

    Mesh mesh = CreateMesh(0, vertices, uvs, indices);
    mesh.polygons = polygons;

    vec3 position = c.position + c.direction * 10.0f;
    ObjPtr obj = CreateGameObjFromPolygons(resources_.get(), 
      mesh.polygons, resources_->GetRandomName(), position);

    configs->new_building = obj;
    configs->place_object = true;
    configs->place_axis = 1;
    resources_->AddNewObject(configs->new_building);
  } else if (result[0] == "add-item") {
    try {
      int item_id = boost::lexical_cast<int>(result[1]);
      const ItemData& item_data = resources_->GetItemData()[item_id];
      resources_->InsertItemInInventory(item_id, item_data.max_stash);
    } catch(boost::bad_lexical_cast const& e) {
    }
  }
}

void Engine::RunPeriodicEventsAsync() {
  // const double min_time_elapsed = 1.0f / 60.0f;

  // double last_update = 0;
  // while (!terminate_) {
  //   double cur_time = glfwGetTime();

  //   double time_elapsed = cur_time - last_update;
  //   if (time_elapsed < min_time_elapsed) {
  //     int ms = (min_time_elapsed - time_elapsed) * 1000;
  //     this_thread::sleep_for(chrono::milliseconds(ms));
  //   }
  //   last_update = glfwGetTime();

  //   resources_->RunPeriodicEvents();
  // }
}

void Engine::EnableMap() {
  shared_ptr<GameObject> obj = resources_->GetObjectByName("map-001");
  obj->frame = 0;
  obj->active_animation = "Armature|unroll";
}

bool Engine::ProcessGameInput() {
  --throttle_counter_;
  GLFWwindow* window = renderer_->window();

  shared_ptr<Configs> configs = resources_->GetConfigs();
  if (configs->show_spellbook) {
    configs->show_spellbook = false;
    inventory_->Enable(window, INVENTORY_SPELLBOOK);
    resources_->SetGameState(STATE_INVENTORY);
  } else if (configs->show_store) {
    configs->show_store = false;
    inventory_->Enable(window, INVENTORY_STORE);
    resources_->SetGameState(STATE_INVENTORY);
  }

  switch (resources_->GetGameState()) {
    case STATE_GAME: { 
      shared_ptr<Player> player = resources_->GetPlayer();

      // TODO: all of this should go into a class player.
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          text_editor_->Enable();
    
          stringstream ss;

          Dungeon& dungeon = resources_->GetDungeon(); 
          ivec2 player_pos = dungeon.GetDungeonTile(player->position);

          char** dungeon_map = dungeon.GetDungeon();
          char** monsters_and_objs = dungeon.GetMonstersAndObjs();
          for (int y = 0; y < kDungeonSize; y++) {
            for (int x = 0; x < kDungeonSize; x++) {
              if (x == player_pos.x && y == player_pos.y) {
                ss << "@ ";
                text_editor_->SetCursorPos(x, y);
                continue;
              }
              char code = dungeon_map[x][y];
              if (code == ' ') { 
                code = monsters_and_objs[x][y];
                if (code == ' ' && dungeon.IsWebFloor(ivec2(x, y))) {
                  code = '#';
                }
              }
              ss << code;
            }
            ss << endl;
          }

          ss << "=============================" << endl;
          ss << "Player pos: " << player->position << endl;
          ss << "Dungeon level: " << configs->dungeon_level << endl;
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
          inventory_->Enable(window);
          resources_->SetGameState(STATE_INVENTORY);
        }
        throttle_counter_ = 10;
        return false;
      } else if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          EnableMap();
          inventory_->Enable(window, INVENTORY_MAP);
          resources_->SetGameState(STATE_MAP);
        }
        throttle_counter_ = 10;
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
      } else if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->selected_tile = 0;
        }
        throttle_counter_ = 5;
      } else if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->selected_spell = 0;
          configs->selected_tile = 1;
        }
        throttle_counter_ = 5;
      } else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->selected_spell = 1;
          configs->selected_tile = 2;
        }
        throttle_counter_ = 5;
      } else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          configs->selected_spell = 2;
          configs->selected_tile = 3;
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

      shared_ptr<CurrentDialog> current_dialog = resources_->GetCurrentDialog();
      if (current_dialog->enabled) {
        glfwSetCursorPos(window_, 0, 0);
        inventory_->Enable(window, INVENTORY_DIALOG);
        resources_->SetGameState(STATE_INVENTORY);
      }
      return false;
    }
    case STATE_EDITOR: {
      if (!text_editor_->enabled) {
        resources_->SetGameState(STATE_GAME);
      }
      return true;
    }
    case STATE_MAP: {
      if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          resources_->SetGameState(STATE_GAME);
          inventory_->Disable();
          glfwSetCursorPos(window_, 0, 0);
        }
        throttle_counter_ = 20;
      }
      return true;
    }
    case STATE_INVENTORY: {
      if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          inventory_->Disable();
        }
        throttle_counter_ = 20;
      }
      if (!inventory_->enabled) {
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
  ProcessGameInput();
  switch (resources_->GetGameState()) {
    case STATE_GAME: {
      renderer_->DrawScreenEffects();
      break;
    }
    case STATE_EDITOR: {
      text_editor_->Draw();
      break;
    }
    case STATE_MAP:
    case STATE_INVENTORY: {
      Camera c = player_input_->GetCamera();
      inventory_->Draw(c, 0, 0, window_);
      break;
    }
    default:
      break; 
  }
}

void Engine::BeforeFrameDebug() {
  unordered_map<string, shared_ptr<GameObject>>& objs = 
    resources_->GetObjects();
  for (auto& [name, obj] : objs) {
    // if (obj->type != GAME_OBJ_DOOR) continue;
    if (obj->GetCollisionType() != COL_OBB) continue;

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
        CreateGameObjFromPolygons(resources_.get(), mesh.polygons, obb_name,
          obj->position);
      door_obbs_[obb_name] = obb_obj;
    } else {
      ObjPtr obb_obj = door_obbs_[obb_name];
      MeshPtr mesh = resources_->GetMesh(obb_obj);
      UpdateMesh(*mesh, vertices, uvs, indices);
    }
  }


  // for (auto& [name, obj] : objs) {
  //   if (!obj->IsCreature()) continue;

  //   cout << "Obj: " << obj->GetName() << endl;

  //   for (auto& [id, bone] : obj->bones) {
  //     string obb_name = obj->name + "-bone-" + boost::lexical_cast<string>(id);
  //     cout << "obb: " << obb_name << endl;
  //   
  //     BoundingSphere bs = obj->GetBoneBoundingSphere(id);
  //     if (door_obbs_.find(obb_name) == door_obbs_.end()) {
  //     cout << "bs: " << bs << endl;
  //       ObjPtr bone_obj = CreateSphere(resources_.get(), bs.radius, bs.center);
  //       bone_obj->never_cull = true;
  //       door_obbs_[obb_name] = bone_obj;
  //       resources_->UpdateObjectPosition(bone_obj);
  //     } else {
  //       ObjPtr bone_obj = door_obbs_[obb_name];
  //       bone_obj->position = bs.center;
  //       resources_->UpdateObjectPosition(bone_obj);
  //     }
  //   }
  // }

  shared_ptr<Configs> configs = resources_->GetConfigs();
  Camera c = player_input_->GetCamera();
  for (auto& [name, obj] : objs) {
    if (obj->type != GAME_OBJ_PARTICLE) continue;
    if (!obj->Is3dParticle()) continue;
    if (!configs->levitate) continue;

    vector<vec3> vertices;
    vector<vec2> uvs;
    vector<unsigned int> indices(12);
    Mesh m;
    // Mesh m = CreateLine(vec3(0), c.direction * 40.0f, vertices, uvs, indices);

    // vec3 w = vec3(1, 0, 0);
    // vec3 h = vec3(0, 1, 0);
    // vec3 d = vec3(0, 0, 1);

    // const vector<vec3> offsets {
    //   // Back face.
    //   vec3(-1, 1, -1), vec3(1, 1, -1), vec3(-1, -1, -1), vec3(1, -1, -1), 
    //   // Front face.
    //   vec3(-1, 1,  1), vec3(1, 1,  1), vec3(-1, -1,  1), vec3(1, -1, 1 )
    // };

    // static float bla = 0.01f;
    // bla += 0.01;
    // mat4 rotation_matrix = rotate(
    //   mat4(1.0),
    //   bla,
    //   vec3(0.0f, 1.0f, 0.0f)
    // );

    // vector<vec3> v;
    // const vec3& c = vec3(0, 0, 0);
    // for (const auto& o : offsets) {
    //   vec3 a = c + w * o.x + h * o.y + d * o.z;
    //  a = vec3(rotation_matrix * vec4(a, 1.0));
    //   v.push_back(a);
    // }

    // vector<vec3> vertices;
    // vector<vec2> uvs;
    // vector<unsigned int> indices(36);
    // Mesh m = CreateCube(v, vertices, uvs, indices);

    shared_ptr<Particle> p = static_pointer_cast<Particle>(obj);

    // MeshPtr mesh = resources_->GetMeshByName("rotating_quad");
    if (p->mesh_name.empty()) {
      cout << "never should" << endl;
      // MeshPtr mesh = resources_->GetMeshByName("rotating_quad");

      p->mesh_name = resources_->GetRandomName();
      resources_->AddMesh(p->mesh_name, m);
      
      // my_box_ = CreateGameObjFromPolygons(resources_.get(), m.polygons, "my-box",
      //     obj->position + vec3(0, 10, 0));
    } else {
      // MeshPtr mesh = resources_->GetMesh(my_box_);
      MeshPtr mesh = resources_->GetMesh(obj);
      UpdateMesh(*mesh, vertices, uvs, indices);
    }

    // MeshPtr mesh = resources_->GetMeshByName("rotating_quad");
    // UpdateMesh(*mesh, vertices, uvs, indices);

    // MeshPtr mesh = resources_->GetMeshByName("rotating_quad");
    // MeshPtr mesh = resources_->GetMesh(obj);
    // configs->levitate = false;
  }
}

void Engine::BeforeFrame() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  {
    // BeforeFrameDebug();
    physics_->Run();
    collision_resolver_->Collide();
    ai_->Run();
    resources_->RunPeriodicEvents();
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

  // Start threads.
  // periodic_events_thread_ = thread(&Engine::RunPeriodicEventsAsync, this);

  glfwSetCursorPos(window_, 0, 0);

  shared_ptr<Configs> configs = resources_->GetConfigs();

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

    resources_->Lock();
    Camera c = player_input_->GetCamera();
    renderer_->SetCamera(c);

    renderer_->Draw();
    resources_->Unlock();

    AfterFrame();

    glfwSwapBuffers(window_);
    glfwPollEvents();
  } while (glfwWindowShouldClose(window_) == 0);

  // Cleanup VBO and shader.
  resources_->Cleanup();
  glfwTerminate();

  terminate_ = true;

  // Join threads.
  // periodic_events_thread_.join();
}
