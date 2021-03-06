#include "player.hpp"

PlayerInput::PlayerInput(shared_ptr<Resources> asset_catalog, 
    shared_ptr<Project4D> project_4d, 
    shared_ptr<Inventory> inventory,
    shared_ptr<Terrain> terrain)
  : resources_(asset_catalog), project_4d_(project_4d), 
    inventory_(inventory), terrain_(terrain) {
}

void PlayerInput::InteractWithItem(GLFWwindow* window, const Camera& c, 
  bool interact) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  vector<shared_ptr<GameObject>> items = resources_->GetItems();

  vec3 p = c.position;
  vec3 d = normalize(c.direction);

  configs->interacting_item = nullptr;

  float t;
  vec3 q;
  ObjPtr item = resources_->IntersectRayObjects(p, d, 20.0f, INTERSECT_ITEMS, t, q);
  bool hit = false;
  if (item) {
    if (interact) {
      if (item->type == GAME_OBJ_DOOR) {
        shared_ptr<Door> door = static_pointer_cast<Door>(item);
        if (door->state == DOOR_CLOSED) {
          door->state = DOOR_OPENING;
          if (door->dungeon_piece_type == 'd' || door->dungeon_piece_type == 'D') {
            resources_->GetDungeon().SetDoorOpen(door->dungeon_tile);
          }
          for (shared_ptr<Event> e : door->on_open_events) {
            shared_ptr<DoorEvent> door_event = static_pointer_cast<DoorEvent>(e);
            resources_->AddEvent(e);
          }
        } else if (door->state == DOOR_OPEN) {
          door->state = DOOR_CLOSING;
          if (door->dungeon_piece_type == 'd' || door->dungeon_piece_type == 'D') {
            resources_->GetDungeon().SetDoorClosed(door->dungeon_tile);
          }
        }
      } else if (item->type == GAME_OBJ_ACTIONABLE) {
         resources_->TurnOnActionable(item->name);

         if (item->trapped) {
           resources_->CastFireExplosion(nullptr, item->position, vec3(0));
         }

         Dungeon& dungeon = resources_->GetDungeon();
         ivec2 adj_tile = dungeon.GetRandomAdjTile(item->position);
         vec3 drop_pos = dungeon.GetTilePosition(adj_tile);

         // TODO: xml chest treasure.
         int r = Random(0, 100); 
         const int item_id = 10;
         const int quantity = Random(0, 100);
         ObjPtr obj = CreateGameObjFromAsset(resources_.get(), 
           "gold", drop_pos + vec3(0, 5.0f, 0));
         obj->CalculateCollisionData();
         obj->quantity = quantity;

      } else {
        shared_ptr<GameAsset> asset = item->GetAsset();
        if (item->name == "book1-001") {
          configs->learned_spells[1] = 1;
          inventory_->Enable(window, INVENTORY_SPELLBOOK);
          resources_->SetGameState(STATE_INVENTORY);
          resources_->AddMessage("You learned to cast magic missile.");
        } else if (item->name == "book2-001") {
          configs->learned_spells[2] = 1;
          inventory_->Enable(window, INVENTORY_SPELLBOOK);
          resources_->SetGameState(STATE_INVENTORY);
          resources_->AddMessage("You learned to cast minor open lock.");
        } else if (item->name == "altar-001") {
          inventory_->Enable(window, INVENTORY_CRAFT);
          resources_->SetGameState(STATE_INVENTORY);
        } else if (item->name == "alessia") {
          resources_->TalkTo(item->name);
        } else if (item->name == "finganforn") {
          resources_->TalkTo(item->name);
        } else if (item->name == "bene") {
          resources_->TalkTo(item->name);
        } else if (item->GetAsset()->name == "dungeon_platform_up") {
          configs->dungeon_level++;
          resources_->DeleteAllObjects();
          resources_->CreateDungeon();
          Dungeon& dungeon = resources_->GetDungeon();
          vec3 pos = dungeon.GetPlatform();
          resources_->GetPlayer()->ChangePosition(pos);
          resources_->GetConfigs()->render_scene = "dungeon";
          resources_->SaveGame();
        } else if (item->GetAsset()->name == "dungeon_platform_down") {
          if (configs->dungeon_level == 0) {
            resources_->DeleteAllObjects();
            resources_->CreateTown();
            resources_->LoadCollisionData("resources/objects/collision_data.xml");
            resources_->CalculateCollisionData();
            resources_->GenerateOptimizedOctree();
            resources_->GetConfigs()->render_scene = "town";
            resources_->GetPlayer()->ChangePosition(vec3(11787, 300, 7742));
            resources_->GetScriptManager()->LoadScripts();
            resources_->SaveGame();
          } else {
            configs->dungeon_level--;
            resources_->DeleteAllObjects();
            resources_->CreateDungeon();
            Dungeon& dungeon = resources_->GetDungeon();
            vec3 pos = dungeon.GetPlatform();
            resources_->GetPlayer()->ChangePosition(pos);
            resources_->GetConfigs()->render_scene = "dungeon";
            resources_->SaveGame();
          }
        } else {
          int item_id = asset->item_id;

          int quantity = item->quantity;
          if (resources_->InsertItemInInventory(item_id, quantity)) {
            resources_->RemoveObject(item);
            resources_->AddMessage(string("You picked a " + 
              item->GetDisplayName()));
          }
        }
      }
    }
    configs->interacting_item = item;
    hit = true;
  }
}

void PlayerInput::Extract(const Camera& c) {
  vector<shared_ptr<GameObject>> extractables = resources_->GetExtractables();
  for (auto obj : extractables) {
    vec3 p = c.position;
    vec3 d = c.direction;
 
    float t;
    vec3 q;
    if (IntersectRaySphere(p, d, obj->GetTransformedBoundingSphere(), t, q)) {
      resources_->MakeGlow(obj);
      obj->status = STATUS_BEING_EXTRACTED;
    }
  }
}

void PlayerInput::EditTerrain(GLFWwindow* window, const Camera& c) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  if (configs->edit_terrain == "none") {
    return;
  }

  int state1 = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
  int state2 = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
  int left_or_right = (state1 == GLFW_PRESS) ? 1 : 0;
  left_or_right = (state2 == GLFW_PRESS) ? -1 : left_or_right;
  if (left_or_right != 0) {
    vec3 start = c.position;
    vec3 end = c.position + c.direction * 500.0f;
    ivec2 tile;
    if (resources_->CollideRayAgainstTerrain(start, end, tile)) {
      int size = configs->brush_size;

      float mean_height = 0;
      for (int x = -size; x <= size; x++) {
        for (int y = -size; y <= size; y++) {
          TerrainPoint p = resources_->GetHeightMap().GetTerrainPoint(tile.x + x, tile.y + y);
          mean_height += p.height;
        }
      }
      mean_height /= 4*size*size;

      for (int x = -size; x <= size; x++) {
        for (int y = -size; y <= size; y++) {
          float distance = sqrt(x*x + y*y);
          if (distance > size) continue;
          float factor = ((size - distance) * (size - distance)) * 0.04f;

          TerrainPoint p = resources_->GetHeightMap().GetTerrainPoint(tile.x + x, tile.y + y);
          if (configs->edit_terrain == "tile") {
            p.tile = configs->selected_tile;

            p.blending = vec3(0, 0, 0);
            if (configs->selected_tile > 0) {
              p.blending[configs->selected_tile-1] = 1;
            }
          } else if (configs->edit_terrain == "height") {
            if (left_or_right == 1) {
              p.height += 0.03 * factor * configs->raise_factor;
            } else {
              p.height -= 0.03 * factor * configs->raise_factor;
            }
          } else if (configs->edit_terrain == "flatten") {
            p.height += 0.001f * (mean_height - p.height) * factor * configs->raise_factor;
          }
          resources_->GetHeightMap().SetTerrainPoint(tile.x+x, tile.y+y, p);
          terrain_->InvalidatePoint(tile + ivec2(x, y));
        }
      }
    }
  }
}

void PlayerInput::EditObject(GLFWwindow* window, const Camera& c) {
  vec3 p = c.position;
  vec3 d = normalize(c.direction);

  float t;
  vec3 q;
  ObjPtr obj = resources_->IntersectRayObjects(p, d, 50.0f, INTERSECT_EDIT, t, q);
  // ObjPtr obj = resources_->CollideRayAgainstObjects(c.position, c.direction);
  if (!obj) {
    cout << "No object" << endl;
    return;
  }

  cout << "There is object" << obj->name << endl;
  shared_ptr<Configs> configs = resources_->GetConfigs();
  configs->new_building = obj;
  configs->place_object = true;
  configs->place_axis = 0;
}

void PlayerInput::Build(GLFWwindow* window, const Camera& c) {
  if (resources_->GetGameState() != STATE_BUILD) {
    return;
  }

  shared_ptr<Configs> configs = resources_->GetConfigs();

  vec3 start = c.position;
  vec3 end = c.position + c.direction * 500.0f;
  ivec2 tile;
  if (resources_->CollideRayAgainstTerrain(start, end, tile)) {
    TerrainPoint p = resources_->GetHeightMap().GetTerrainPoint(tile.x, tile.y);
    configs->new_building->position = vec3(tile.x, p.height, tile.y);

    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    if (state == GLFW_PRESS) {
      resources_->SetGameState(STATE_GAME);
      ObjPtr player = resources_->GetPlayer();
      shared_ptr<Configs> configs = resources_->GetConfigs();
      player->position = configs->old_position;
    }
  }
}

void PlayerInput::PlaceObject(GLFWwindow* window, const Camera& c) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  if (!configs->place_object) {
    return;
  }

  ObjPtr obj = configs->new_building;

  vec3 start = c.position;
  vec3 end = c.position + c.direction * 500.0f;
  if (configs->place_axis == -1) {
    ivec2 tile;
    if (resources_->CollideRayAgainstTerrain(start, end, tile)) {
      TerrainPoint p = resources_->GetHeightMap().GetTerrainPoint(tile.x, tile.y);
      obj->position = vec3(tile.x, p.height, tile.y);
      resources_->UpdateObjectPosition(obj);
    }
  } else {
    vec3 plane_point = configs->new_building->position;
    vec3 plane_normal = vec3(0, 0, 0);
    if (configs->place_axis == 0) {
      plane_normal = vec3(0, 0, 1);
    } else if (configs->place_axis == 1) {
      plane_normal = vec3(0, 1, 0);
    } else if (configs->place_axis == 2) {
      plane_normal = vec3(1, 0, 0);
    }

    float d = dot(plane_point, plane_normal);
    Plane p = Plane(plane_normal, d);

    float t;
    vec3 q;
    if (IntersectSegmentPlane(start, end, p, t, q)) {
      if (configs->scale_object) {
        float scaling_factor = 0.0f;
        if (configs->place_axis == 0) {
          scaling_factor = abs(q.y - configs->scale_pivot.y);
          configs->scale_dimensions.y = scaling_factor;
        } else if (configs->place_axis == 1) {
          scaling_factor = abs(q.x - configs->scale_pivot.x);
          configs->scale_dimensions.x = scaling_factor;
        } else if (configs->place_axis == 2) {
          scaling_factor = abs(q.z - configs->scale_pivot.z);
          configs->scale_dimensions.z = scaling_factor;
        }
       
        vector<vec3> vertices;
        vector<vec2> uvs;
        vector<unsigned int> indices;
        vector<Polygon> polygons;
        CreateCube(vertices, uvs, indices, polygons, configs->scale_dimensions);

        const string mesh_name = obj->GetAsset()->lod_meshes[0];
        shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
        if (!mesh) {
          throw runtime_error(string("Mesh ") + mesh_name + " does not exist.");
        }

        UpdateMesh(*mesh, vertices, uvs, indices);
        mesh->polygons = polygons;
        obj->GetAsset()->aabb = GetAABBFromPolygons(polygons);
        resources_->UpdateObjectPosition(obj);
      } else {
        configs->new_building->position = q;
        resources_->UpdateObjectPosition(obj);
      }
    }
  }

  int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
  if (state == GLFW_PRESS) {
    configs->place_object = false;
    configs->scale_object = false;
    resources_->RemoveObject(configs->new_building);  
    return;
  }

  state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
  if (state == GLFW_PRESS) {
    resources_->SetGameState(STATE_GAME);
    configs->new_building->CalculateCollisionData();
    resources_->GenerateOptimizedOctree();

    configs->place_object = false;
    configs->scale_object = false;
    configs->scale_pivot = configs->new_building->position;
    configs->new_building = nullptr;
  }
}

Camera PlayerInput::GetCamera() {
  shared_ptr<Player> player = resources_->GetPlayer();
  
  vec3 direction(
    cos(player->rotation.x) * sin(player->rotation.y), 
    sin(player->rotation.x),
    cos(player->rotation.x) * cos(player->rotation.y)
  );

  vec3 right = glm::vec3(
    sin(player->rotation.y - 3.14f/2.0f), 
    0,
    cos(player->rotation.y - 3.14f/2.0f)
  );

  vec3 front = glm::vec3(
    cos(player->rotation.x) * sin(player->rotation.y), 
    0,
    cos(player->rotation.x) * cos(player->rotation.y)
  );

  vec3 up = glm::cross(right, direction);
  Camera c = Camera(player->position, direction, up);

  c.rotation.x = player->rotation.x;
  c.rotation.y = player->rotation.y;
  c.right = right;
  return c;
}

void PlayerInput::CastSpellOrUseItem() {
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");

  int item_id = configs->spellbar[configs->selected_spell];
  shared_ptr<ArcaneSpellData> arcane_spell =  
    resources_->WhichArcaneSpell(item_id);

  if (arcane_spell) {
    current_spell_ = arcane_spell;
    if (arcane_spell->spell_id == 0) {
      // TODO: check which spell here.
      // if (configs->spellbar[configs->selected_spell] == 1) { 
      // Magic missile.
      obj->active_animation = "Armature|shoot";
      player->player_action = PLAYER_CASTING;
      obj->frame = 0;
      animation_frame_ = 60;
      resources_->CreateChargeMagicMissileEffect();
      player->selected_spell = 0;
      configs->spellbar_quantities[configs->selected_spell]--;
      if (configs->spellbar_quantities[configs->selected_spell] == 0) {
        configs->spellbar[configs->selected_spell] = 0;
      }
      debounce_ = 20;
    } else if (arcane_spell->spell_id == 1) {
      resources_->CastBurningHands(camera_);
      if (Random(0, 5) == 0) {
        configs->spellbar_quantities[configs->selected_spell]--;
        if (configs->spellbar_quantities[configs->selected_spell] == 0) {
          configs->spellbar[configs->selected_spell] = 0;
        }
      }
      debounce_ = -1;
    } else if (arcane_spell->spell_id == 2) {
      resources_->CastLightningRay(player, camera_.position, camera_.direction);
      if (Random(0, 5) == 0) {
        configs->spellbar_quantities[configs->selected_spell]--;
        if (configs->spellbar_quantities[configs->selected_spell] == 0) {
          configs->spellbar[configs->selected_spell] = 0;
        }
      }
      debounce_ = -1;
    }
    return;
  }

  current_spell_ = nullptr;
  if (item_id == 11) {
    resources_->CastHeal(player);
    configs->spellbar_quantities[configs->selected_spell]--;
    if (configs->spellbar_quantities[configs->selected_spell] == 0) {
      configs->spellbar[configs->selected_spell] = 0;
    }
    debounce_ = 20;
  } else if (item_id == 12) {
    resources_->CastDarkvision();
    configs->spellbar_quantities[configs->selected_spell]--;
    if (configs->spellbar_quantities[configs->selected_spell] == 0) {
      configs->spellbar[configs->selected_spell] = 0;
    }
    debounce_ = 20;
  } else if (item_id == 4) {
    vec3 p = camera_.position;
    vec3 d = normalize(camera_.direction);

    float t;
    vec3 q;
    ObjPtr item = resources_->IntersectRayObjects(p, d, 10.0f, INTERSECT_ITEMS, t, q);
    if (item && item->type == GAME_OBJ_DOOR) {
      shared_ptr<Door> door = static_pointer_cast<Door>(item);
      if (door->state == 4) {
        door->state = DOOR_CLOSED;
        obj->active_animation = "Armature|shoot";
        player->player_action = PLAYER_CASTING;
        obj->frame = 0;
        animation_frame_ = 20;
        resources_->CreateChargeMagicMissileEffect();
        player->selected_spell = 4;
        configs->spellbar_quantities[configs->selected_spell]--;
        if (configs->spellbar_quantities[configs->selected_spell] == 0) {
          configs->spellbar[configs->selected_spell] = 0;
        }
      }
    }
    debounce_ = 20;
  } else if (item_id == 3) {
    configs->spellbar_quantities[configs->selected_spell]--;
    if (configs->spellbar_quantities[configs->selected_spell] == 0) {
      configs->spellbar[configs->selected_spell] = 0;
    }
    player->life += Random(30, 51);
    debounce_ = 20;
  } else {
    player->selected_spell = -1;
    player->player_action = PLAYER_CASTING;
    resources_->CreateChargeMagicMissileEffect();
    animation_frame_ = 60;
    debounce_ = 20;
    obj->frame = 0;
  }
}

void PlayerInput::ProcessPlayerCasting() {
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  obj->active_animation = "Armature|shoot";
  if (animation_frame_ == 0 && glfwGetKey(window_, GLFW_KEY_C) != GLFW_PRESS) {
    obj->active_animation = "Armature|idle.001";
    obj->frame = 0;
    player->player_action = PLAYER_IDLE;
    current_spell_ = nullptr;
  } else if (animation_frame_ <= 0 && glfwGetKey(window_, GLFW_KEY_C) == GLFW_PRESS) {
    if (!current_spell_) {
      obj->active_animation = "Armature|shoot";
      obj->frame = 0;
      animation_frame_ = 60;
      resources_->CreateChargeMagicMissileEffect();
    } else if (current_spell_->spell_id == 0) {
      obj->active_animation = "Armature|shoot";
      obj->frame = 0;
      animation_frame_ = 60;
      resources_->CreateChargeMagicMissileEffect();

      configs->spellbar_quantities[configs->selected_spell]--;
      if (configs->spellbar_quantities[configs->selected_spell] == 0) {
        configs->spellbar[configs->selected_spell] = 0;
      }
    } else {
      obj->active_animation = "Armature|idle.001";
      obj->frame = 0;
      player->player_action = PLAYER_IDLE;
    }
  } else if (animation_frame_ == 20) {
    if (!current_spell_) {
      vec3 p = camera_.position;
      vec3 d = normalize(camera_.direction);

      float t;
      vec3 q;
      ObjPtr obj = resources_->IntersectRayObjects(p, d, 20.0f, INTERSECT_EDIT, t, q);
      if (obj && (obj->IsCreature() || obj->IsDestructible())) {
        float dmg = ProcessDiceFormula(configs->base_damage);
        obj->DealDamage(player, dmg);
      }
    } else if (current_spell_->spell_id == 0) {
      resources_->CastMagicMissile(camera_);
    }
  }
}

Camera PlayerInput::ProcessInput(GLFWwindow* window) {
  static double last_time = glfwGetTime();
  double current_time = glfwGetTime();

  window_ = window;
  --debounce_;
  --animation_frame_;

  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();

  vec3 right = glm::vec3(
    sin(player->rotation.y - 3.14f/2.0f), 
    0,
    cos(player->rotation.y - 3.14f/2.0f)
  );

  vec3 front = glm::vec3(
    cos(player->rotation.x) * sin(player->rotation.y), 
    0,
    cos(player->rotation.x) * cos(player->rotation.y)
  );

  Camera c = GetCamera();
  camera_ = c;
  if (!player->actions.empty()) {
    return c;
  }

  if (player->status == STATUS_DEAD) {
    return c;
  }

  if (resources_->GetGameState() == STATE_EDITOR) return c;

  float player_speed = resources_->GetConfigs()->player_speed; 
  float jump_force = resources_->GetConfigs()->jump_force; 

  float d = resources_->GetDeltaTime() / 0.016666f;
  if (resources_->GetGameState() == STATE_BUILD) {
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      player->position += front * 10.0f * d;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      player->position -= front * 10.0f * d;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      player->position += right * 10.0f * d;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      player->position -= right * 10.0f * d;

    if (player->position.x < 11200)
      player->position.x = 11200;
    if (player->position.x > 11800)
      player->position.x = 11800;
    if (player->position.z < 7400)
      player->position.z = 7400;
    if (player->position.z > 7900)
      player->position.z = 7900;

  } else {
    if (player->paralysis_cooldown == 0) {
      // Move forward.
      if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        player->speed += front * player_speed * d;

      // Move backward.
      if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        player->speed -= front * player_speed * d;

      // Strafe right.
      if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        player->speed += right * player_speed * d;

      // Strafe left.
      if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        player->speed -= right * player_speed * d;

      // Move up.
      if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (player->can_jump) {
          player->can_jump = false;
          player->speed.y += jump_force;
        }
      }

      // Move down.
      if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        player->speed.y -= jump_force;
      }
    }
  }

  double x_pos = 0, y_pos = 0;
  int focused = glfwGetWindowAttrib(window, GLFW_FOCUSED);
  if (focused) {
    glfwGetCursorPos(window, &x_pos, &y_pos);
    glfwSetCursorPos(window, 0, 0);
  }

  // Change orientation.
  float mouse_sensitivity = 0.003f;
  player->rotation.y += mouse_sensitivity * float(-x_pos);
  player->rotation.x += mouse_sensitivity * float(-y_pos);
  if (player->rotation.x < -1.57f) player->rotation.x = -1.57f;
  if (player->rotation.x >  1.57f) player->rotation.x = +1.57f;
  last_time = current_time;

  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  switch (player->player_action) {
    case PLAYER_IDLE: {
      obj->active_animation = "Armature|idle.001";
      if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        if (debounce_ < 0) {
          CastSpellOrUseItem();
        } else {
          debounce_ = 20;
        }
      } else if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (debounce_ < 0) {
          resources_->CastStringAttack(player, c.position, c.direction);
          debounce_ = 0;
        }
      } else if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
        if (debounce_ < 0) {
          resources_->CastFireExplosion(player, c.position, c.direction);
          debounce_ = 20;
        }
      } else if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS) {
        if (debounce_ < 0) {
          resources_->CastFireball(c);
          debounce_ = 20;
        }
      }
      break;
    }
    case PLAYER_CASTING: {
      ProcessPlayerCasting();
      break;
    }
    default: break;
  }

  throttle_counter_--;
  bool interacted_with_item = false;
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      InteractWithItem(window, c, true);
      interacted_with_item = true;
    }
    throttle_counter_ = 20;
  } else if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      EditObject(window, c);
    }
    throttle_counter_ = 20;
  } else if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      configs->place_axis = 0;
    }
    throttle_counter_ = 20;
  } else if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      configs->place_axis = 1;
    }
    throttle_counter_ = 20;
  } else if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      configs->place_axis = 2;
    }
    throttle_counter_ = 20;
  } else if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      configs->place_axis = -1;
    }
    throttle_counter_ = 20;
  } else if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
    if (configs->new_building) {
      if (throttle_counter_ < 0) {
        if (configs->scale_object) {
          configs->scale_object = false;
        } else {
          configs->scale_object = true;
          configs->scale_pivot = configs->new_building->position;
        }
      }
      throttle_counter_ = 20;
    }
  } else if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) {
    if (configs->place_object) {
      configs->new_building->rotation_matrix *= rotate(mat4(1.0), -0.005f, vec3(0, 1, 0));
    }
  } else if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) {
    if (configs->place_object) {
      configs->new_building->rotation_matrix *= rotate(mat4(1.0), 0.005f, vec3(0, 1, 0));
    }
  } else if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
    if (configs->place_object) {
      configs->new_building->rotation_matrix *= rotate(mat4(1.0), -0.005f, vec3(1, 0, 0));
    }
  } else if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) {
    if (configs->place_object) {
      configs->new_building->rotation_matrix *= rotate(mat4(1.0), 0.005f, vec3(1, 0, 0));
    }
  }

  if (!interacted_with_item) {
    InteractWithItem(window, c, false);
  }

  EditTerrain(window, c);
  Build(window, c);
  PlaceObject(window, c);

  return c;
}
