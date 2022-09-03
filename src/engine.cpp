#include "engine.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

Engine::Engine(
  shared_ptr<Project4D> project_4d,
  shared_ptr<Renderer> renderer,
  shared_ptr<TextEditor> text_editor,
  shared_ptr<Inventory> inventory,
  shared_ptr<GameScreen> game_screen,
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
    game_screen_(game_screen), 
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
  Dungeon& dungeon = resources_->GetDungeon();

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
  } else if (result[0] == "pathfinding-map") {
    dungeon.PrintPathfindingMap(player->position);
  } else if (result[0] == "drawdungeon") {
    configs->draw_dungeon = true;
  } else if (result[0] == "nodrawdungeon") {
    configs->draw_dungeon = false;
  } else if (result[0] == "pathfinding") {
    int x = boost::lexical_cast<int>(result[1]);
    int y = boost::lexical_cast<int>(result[2]);
    player->actions.push(make_shared<LongMoveAction>(
      dungeon.GetTilePosition(ivec2(x, y))));
    cout << "Moving to X:" << x << " - Y:" << y << endl;
  } else if (result[0] == "dungeon-tile") {
    ivec2 tile = dungeon.GetDungeonTile(player->position);
    cout << "Tile X: " << tile.x << " - Y:" << tile.y << endl;
  } else if (result[0] == "clear-actions") {
    player->ClearActions();
  } else if (result[0] == "self-harm") {
    player->life -= 10.0f;
  } else if (result[0] == "nolevitate") {
    configs->levitate = false;
  } else if (result[0] == "time") {
    configs->stop_time = false;
  } else if (result[0] == "debug") {
    before_frame_debug_ = true;
  } else if (result[0] == "nodebug") {
    before_frame_debug_ = false;
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
  } else if (result[0] == "restart") {
    resources_->RestartGame();
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
  } else if (result[0] == "reveal") {
    Dungeon& dungeon = resources_->GetDungeon();
    dungeon.Reveal();
  } else if (result[0] == "create") {
    string asset_name = result[1];
    if (resources_->GetAssetGroupByName(asset_name)) {
      configs->place_axis = 1;

      Camera c = player_input_->GetCamera();
      vec3 position = c.position + c.direction * 10.0f;

      configs->new_building = CreateGameObjFromAsset(resources_.get(), 
        asset_name, position);

      configs->new_building->being_placed = true;
      configs->new_building->life = 100;
      configs->place_object = true;
      resources_->AddNewObject(configs->new_building);
    }
  } else if (result[0] == "learn") {
    stringstream ss;
    for (int i = 1; i < result.size(); i++) {
      ss << result[i];
      if (i == result.size() - 1) break;
      ss << " ";
    }

    string spell_name = ss.str();
    boost::algorithm::trim(spell_name);
    cout << "#" << spell_name << "#" <<endl;
    
    int item_id = resources_->GetSpellItemIdFromName(spell_name);
    if (item_id != -1) {
      resources_->LearnSpell(item_id);
    }
  } else if (result[0] == "learn-id") {
    int item_id = boost::lexical_cast<int>(result[1]);
    resources_->LearnSpell(item_id);
  } else if (result[0] == "learn-all") {
    for (int i = 0; i < 5; i++) {
      resources_->LearnSpell(i);
    }
  } else if (result[0] == "create-2d-particle") {
    string particle_name = result[1];
    configs->place_axis = 1;

    Camera c = player_input_->GetCamera();
    vec3 position = c.position + c.direction * 10.0f;

    configs->new_building = resources_->CreateOneParticle(position, 1000000.0f, 
      particle_name, 5);
    configs->new_building->speed = vec3(0);

    cout << "Particle: " << position << endl;
    configs->new_building->being_placed = true;
    configs->place_object = true;
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
      resources_->ChangeDungeonLevel(dungeon_level);
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
  } else if (result[0] == "arena") {
    try {
      const string& filename = "dungeons/dungeon1.txt";

      Dungeon& dungeon = resources_->GetDungeon();
      if (dungeon.LoadDungeonFromFile(filename)) {
        resources_->ChangeDungeonLevel(0);
        resources_->DeleteAllObjects();
        resources_->CreateDungeon(false);
        vec3 pos = dungeon.GetTilePosition(ivec2(6, 6));
        configs->wave_reset_timer = glfwGetTime() + 5.0f;

        resources_->GetPlayer()->ChangePosition(pos);
        resources_->GetConfigs()->render_scene = "arena";
        resources_->SaveGame();
        resources_->CalculateCollisionData();
        resources_->GenerateOptimizedOctree();
        configs->update_renderer = true;
        configs->wave_monsters.clear();
        configs->current_wave = -1;

        for (int i = 0; i < 11; i++) {
          resources_->LearnSpell(i);
        }
      }
    } catch(boost::bad_lexical_cast const& e) {
    }
  } else if (result[0] == "nowave") {
    configs->current_wave = 999;
  } else if (result[0] == "wave") {
    if (result.size() == 3) {
      try {
        char code = boost::lexical_cast<char>(result[1]);
        int quantity = boost::lexical_cast<int>(result[2]);
        resources_->CreateMonsters(code, quantity);
      } catch(boost::bad_lexical_cast const& e) {
      }
    }
  } else if (result[0] == "set-wave") {
    if (result.size() == 2) {
      try {
        int wave_num = boost::lexical_cast<int>(result[1]);
        configs->current_wave = wave_num;
      } catch(boost::bad_lexical_cast const& e) {
      }
    }
  } else if (result[0] == "town") {
    resources_->DeleteAllObjects();
    resources_->CreateTown();
    resources_->LoadCollisionData("resources/objects/collision_data.xml");
    resources_->CalculateCollisionData();
    resources_->GenerateOptimizedOctree();
    resources_->GetConfigs()->render_scene = "town";
    resources_->GetPlayer()->ChangePosition(vec3(11628, 141, 7246));
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
  } else if (result[0] == "detect-monsters") {
    resources_->CastDetectMonsters();
  } else if (result[0] == "no-detect-monsters") {
    configs->detect_monsters = false;
  } else if (result[0] == "set-flag") {
    if (result.size() == 3) {
      resources_->SetGameFlag(result[1], result[2]);
    }
  } else if (result[0] == "weave") {
    // resources_->CreateWeave();
    // configs->gameplay_style = GAMEPLAY_WEAVE;
    configs->gameplay_style = GAMEPLAY_RANDOM;
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
      Dungeon& dungeon = resources_->GetDungeon();

      // TODO: all of this should go into a class player.
      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          text_editor_->Enable();
    
          ivec2 tile = dungeon.GetDungeonTile(player->position);

          stringstream ss;
          ss << "Player pos: " << player->position << endl;
          ss << "Player tile: " << tile.x  << ", " << tile.y << endl;
          ss << "Dungeon level: " << configs->dungeon_level << endl;
          ss << "Time of day: " << configs->time_of_day << endl;

          Camera c = player_input_->GetCamera();
          ObjPtr obj = resources_->CollideRayAgainstObjects(c.position, c.direction);
          if (obj) {
            ss << "Pointing to " << obj->name << "(" << obj->GetDisplayName() << ")" << endl;
          }

          if (configs->place_object) {
            ss << "Placing : " << configs->new_building->name << endl;
          }
           ss << "==================" << endl;

          for (ObjPtr obj : resources_->GetMovingObjects()) {
            if (!obj->IsCreature()) continue;
            ss << "Spider (" << obj->name << "):" << obj->position << " - " << AiStateToStr(obj->ai_state) << endl;

            shared_ptr<Action> next_action = nullptr;
            if (!obj->actions.empty()) {
              next_action = obj->actions.front();
            }
             
            if (next_action) {        
              ss << "next_action: " << ActionTypeToStr(next_action->type) << endl;
              if (next_action->type == ACTION_LONG_MOVE) {
                shared_ptr<LongMoveAction> move_action =  
                  static_pointer_cast<LongMoveAction>(next_action);
                ss << "long move to: " << move_action->destination << endl;
              }
              // if (next_action->type == ACTION_MOVE_TO_PLAYER) {
              //   shared_ptr<MoveToPlayerAction> move_action =  
              //     static_pointer_cast<MoveToPlayerAction>(next_action);
              //   ss << "move to player: " << move_action->destination << endl;
              // }
            }
          }
          
          ss << "Time of day: " << configs->time_of_day << endl;

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
      } else if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
        if (throttle_counter_ < 0) {
          inventory_->Enable(window, INVENTORY_SPELLBOOK);
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
    case STATE_START_SCREEN: {
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
    case STATE_START_SCREEN: {
      Camera c = player_input_->GetCamera();
      game_screen_->Draw(c, 0, 0, window_);
      break; 
    }
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
  // for (auto& [name, obj] : objs) {
  //   // if (obj->type != GAME_OBJ_DOOR) continue;
  //   if (obj->GetCollisionType() != COL_OBB) continue;

  //   string obb_name = obj->name + "-obb";
  //   OBB obb = obj->GetTransformedOBB();

  //   vec3 w = obb.axis[0] * obb.half_widths[0];
  //   vec3 h = obb.axis[1] * obb.half_widths[1];
  //   vec3 d = obb.axis[2] * obb.half_widths[2];

  //   const vector<vec3> offsets {
  //     // Back face.
  //     vec3(-1, 1, -1), vec3(1, 1, -1), vec3(-1, -1, -1), vec3(1, -1, -1), 
  //     // Front face.
  //     vec3(-1, 1,  1), vec3(1, 1,  1), vec3(-1, -1,  1), vec3(1, -1, 1 )
  //   };

  //   vector<vec3> v;
  //   const vec3& c = obb.center;
  //   for (const auto& o : offsets) {
  //     v.push_back(c + w * o.x + h * o.y + d * o.z);
  //   }

  //   vector<vec3> vertices;
  //   vector<vec2> uvs;
  //   vector<unsigned int> indices(36);
  //   Mesh mesh = CreateCube(v, vertices, uvs, indices);
  //   if (door_obbs_.find(obb_name) == door_obbs_.end()) {
  //     shared_ptr<GameObject> obb_obj = 
  //       CreateGameObjFromPolygons(resources_.get(), mesh.polygons, obb_name,
  //         obj->position);
  //     door_obbs_[obb_name] = obb_obj;
  //   } else {
  //     ObjPtr obb_obj = door_obbs_[obb_name];
  //     MeshPtr mesh = resources_->GetMesh(obb_obj);
  //     UpdateMesh(*mesh, vertices, uvs, indices);
  //   }
  // }

  for (auto& [name, obj] : objs) {
    if (!obj->IsCreature()) continue;

    for (auto& [id, bone] : obj->bones) {
      string obb_name = obj->name + "-bone-" + boost::lexical_cast<string>(id);
    
      BoundingSphere bs = obj->GetBoneBoundingSphere(id);
      if (door_obbs_.find(obb_name) == door_obbs_.end()) {
        ObjPtr bone_obj = CreateSphere(resources_.get(), bs.radius, bs.center);
        bone_obj->never_cull = true;
        door_obbs_[obb_name] = bone_obj;
        resources_->UpdateObjectPosition(bone_obj);
      } else {
        ObjPtr bone_obj = door_obbs_[obb_name];
        bone_obj->position = bs.center;
        resources_->UpdateObjectPosition(bone_obj);
      }
    }
  }

  // shared_ptr<Configs> configs = resources_->GetConfigs();
  // Camera c = player_input_->GetCamera();
  // for (auto& [name, obj] : objs) {
  //   if (obj->type != GAME_OBJ_PARTICLE) continue;
  //   if (!obj->Is3dParticle()) continue;
  //   if (!configs->levitate) continue;

  //   vector<vec3> vertices;
  //   vector<vec2> uvs;
  //   vector<unsigned int> indices(12);
  //   Mesh m;
  //   // Mesh m = CreateLine(vec3(0), c.direction * 40.0f, vertices, uvs, indices);

  //   // vec3 w = vec3(1, 0, 0);
  //   // vec3 h = vec3(0, 1, 0);
  //   // vec3 d = vec3(0, 0, 1);

  //   // const vector<vec3> offsets {
  //   //   // Back face.
  //   //   vec3(-1, 1, -1), vec3(1, 1, -1), vec3(-1, -1, -1), vec3(1, -1, -1), 
  //   //   // Front face.
  //   //   vec3(-1, 1,  1), vec3(1, 1,  1), vec3(-1, -1,  1), vec3(1, -1, 1 )
  //   // };

  //   // static float bla = 0.01f;
  //   // bla += 0.01;
  //   // mat4 rotation_matrix = rotate(
  //   //   mat4(1.0),
  //   //   bla,
  //   //   vec3(0.0f, 1.0f, 0.0f)
  //   // );

  //   // vector<vec3> v;
  //   // const vec3& c = vec3(0, 0, 0);
  //   // for (const auto& o : offsets) {
  //   //   vec3 a = c + w * o.x + h * o.y + d * o.z;
  //   //  a = vec3(rotation_matrix * vec4(a, 1.0));
  //   //   v.push_back(a);
  //   // }

  //   // vector<vec3> vertices;
  //   // vector<vec2> uvs;
  //   // vector<unsigned int> indices(36);
  //   // Mesh m = CreateCube(v, vertices, uvs, indices);

  //   shared_ptr<Particle> p = static_pointer_cast<Particle>(obj);

  //   // MeshPtr mesh = resources_->GetMeshByName("rotating_quad");
  //   if (p->mesh_name.empty()) {
  //     cout << "never should" << endl;
  //     // MeshPtr mesh = resources_->GetMeshByName("rotating_quad");

  //     p->mesh_name = resources_->GetRandomName();
  //     resources_->AddMesh(p->mesh_name, m);
  //     
  //     // my_box_ = CreateGameObjFromPolygons(resources_.get(), m.polygons, "my-box",
  //     //     obj->position + vec3(0, 10, 0));
  //   } else {
  //     // MeshPtr mesh = resources_->GetMesh(my_box_);
  //     MeshPtr mesh = resources_->GetMesh(obj);
  //     UpdateMesh(*mesh, vertices, uvs, indices);
  //   }

  //   // MeshPtr mesh = resources_->GetMeshByName("rotating_quad");
  //   // UpdateMesh(*mesh, vertices, uvs, indices);

  //   // MeshPtr mesh = resources_->GetMeshByName("rotating_quad");
  //   // MeshPtr mesh = resources_->GetMesh(obj);
  //   // configs->levitate = false;
  // }
}

void Engine::BeforeFrame() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  {
    if (before_frame_debug_) {
      BeforeFrameDebug();
    }

    switch (resources_->GetGameState()) {
      case STATE_GAME: {
        physics_->Run();
        collision_resolver_->Collide();
        ai_->Run();
        resources_->RunPeriodicEvents();
        break;
      }
      case STATE_MAP: {
        renderer_->DrawMap();
        break;
      }
      default: {
        break;
      }
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

  // Start threads.
  // periodic_events_thread_ = thread(&Engine::RunPeriodicEventsAsync, this);

  glfwSetCursorPos(window_, 0, 0);

  shared_ptr<Configs> configs = resources_->GetConfigs();
  resources_->LoadGame("config.xml", false);

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


    resources_->Lock();
    renderer_->Draw();
    resources_->Unlock();

    BeforeFrame();
    Camera c = player_input_->GetCamera();
    renderer_->SetCamera(c);
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
