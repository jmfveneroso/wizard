#include "player.hpp"

PlayerInput::PlayerInput(shared_ptr<Resources> asset_catalog, 
    shared_ptr<Project4D> project_4d, 
    shared_ptr<Inventory> inventory,
    shared_ptr<Terrain> terrain)
  : resources_(asset_catalog), project_4d_(project_4d), 
    inventory_(inventory), terrain_(terrain) {
}

bool PlayerInput::InteractWithItem(GLFWwindow* window, const Camera& c, 
  bool interact) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  vector<shared_ptr<GameObject>> items = resources_->GetItems();
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  Dungeon& dungeon = resources_->GetDungeon();

  vec3 p = c.position;
  vec3 d = normalize(c.direction);

  configs->interacting_item = nullptr;

  float t;
  vec3 q;
  ObjPtr item = resources_->IntersectRayObjects(p, d, configs->reach, INTERSECT_ITEMS, t, q);
  bool hit = false;
  if (item) {
    if (interact) {
      if (item->type == GAME_OBJ_DOOR) {
        if (item->name == "dungeon-entrance") {
          resources_->ChangeDungeonLevel(0);
          resources_->DeleteAllObjects();
          resources_->CreateDungeon();
          Dungeon& dungeon = resources_->GetDungeon();
          vec3 pos = dungeon.GetUpstairs();
          resources_->GetPlayer()->ChangePosition(pos);
          resources_->GetConfigs()->render_scene = "dungeon";
          resources_->SaveGame();
        } else {
          shared_ptr<Door> door = static_pointer_cast<Door>(item);
          if (door->state == DOOR_CLOSED) {
            door->state = DOOR_OPENING;
            if (door->dungeon_tile.x != -1) {
              resources_->GetDungeon().SetDoorOpen(door->dungeon_tile);
              dungeon.CalculateAllPathsAsync(door->dungeon_tile);
            }
            for (shared_ptr<Event> e : door->on_open_events) {
              shared_ptr<DoorEvent> door_event = static_pointer_cast<DoorEvent>(e);
              resources_->AddEvent(e);
            }
          } else if (door->state == DOOR_OPEN) {
            door->state = DOOR_CLOSING;
            if (door->dungeon_tile.x != -1) {
              resources_->GetDungeon().SetDoorClosed(door->dungeon_tile);
              dungeon.CalculateAllPathsAsync(door->dungeon_tile);
            }
          }
        }
      } else if (item->type == GAME_OBJ_ACTIONABLE) {
         shared_ptr<Actionable> actionable = 
           static_pointer_cast<Actionable>(item);

         if (actionable->GetAsset()->name == "large_chest_wood") {
           if (actionable->state == 0) {
             resources_->TurnOnActionable(item->name);

             Dungeon& dungeon = resources_->GetDungeon();
             int item_id = dungeon.GetRandomChestLoot(configs->dungeon_level);
             if (item_id != -1) {
               resources_->CreateChestDrops(item, item_id);
             }
           }
         }
      } else {
        shared_ptr<GameAsset> asset = item->GetAsset();
        if (item->GetAsset()->name == "merchant_body") {
          resources_->TalkTo(item->name);
        } else if (item->GetAsset()->name == "spell_door_block") {
          Dungeon& dungeon = resources_->GetDungeon();
          ivec2 tile = dungeon.GetDungeonTile(item->position);
          dungeon.UnsetFlag(tile, DLRG_SPELL_WALL);
          dungeon.CalculateAllPathsAsync(tile);
          resources_->RemoveObject(item);
          resources_->CalculateCollisionData();
          resources_->GenerateOptimizedOctree();
        } else if (item->name == "town-portal") {
          resources_->EnterTownPortal();
        } else if (item->GetAsset()->name == "stairs_down_hull") {
          resources_->ChangeDungeonLevel(configs->dungeon_level+1);
          if (configs->dungeon_level == 7) {
            resources_->GetConfigs()->render_scene = "safe-zone";
            resources_->DeleteAllObjects();
            resources_->CreateSafeZone();
            resources_->LoadCollisionData("resources/objects/collision_data.xml");
            resources_->CalculateCollisionData();
            resources_->GenerateOptimizedOctree();
            resources_->GetPlayer()->ChangePosition(vec3(11600, 510, 7600));
            resources_->SaveGame();
          } else {
            resources_->DeleteAllObjects();
            resources_->CreateDungeon();
            Dungeon& dungeon = resources_->GetDungeon();
            vec3 pos = dungeon.GetUpstairs();
            resources_->GetPlayer()->ChangePosition(pos);
            resources_->GetConfigs()->render_scene = "dungeon";
            resources_->SaveGame();
          }
        } else if (item->GetAsset()->name == "stairs_up_hull") {
          if (configs->render_scene == "safe-zone") {
            resources_->ChangeDungeonLevel(7);
          }

          if (configs->dungeon_level == 0) {
            resources_->DeleteAllObjects();
            resources_->CreateTown();
            resources_->LoadCollisionData("resources/objects/collision_data.xml");
            resources_->CalculateCollisionData();
            resources_->GenerateOptimizedOctree();
            resources_->GetConfigs()->render_scene = "town";
            resources_->GetPlayer()->ChangePosition(vec3(11628, 141, 7246));
            resources_->SaveGame();
          } else {
            resources_->ChangeDungeonLevel(configs->dungeon_level-1);
            resources_->DeleteAllObjects();
            resources_->CreateDungeon();
            Dungeon& dungeon = resources_->GetDungeon();
            vec3 pos = dungeon.GetDownstairs();
            resources_->GetPlayer()->ChangePosition(pos);
            resources_->GetConfigs()->render_scene = "dungeon";
            resources_->SaveGame();
          }
        } else {
          int item_id = item->GetItemId();
          shared_ptr<ArcaneSpellData> arcane_spell =  
            resources_->WhichArcaneSpell(item_id);

          if (arcane_spell) { 
            resources_->LearnSpell(arcane_spell->spell_id);
            resources_->RemoveObject(item);
          } else if (item_id == 20) { 
            resources_->GetPlayer()->mana += 1;
            if (resources_->GetPlayer()->mana > 
              resources_->GetPlayer()->max_mana) {
              resources_->GetPlayer()->mana = resources_->GetPlayer()->max_mana;
            }
            resources_->RemoveObject(item);
          } else if (item_id == 40) { 
            resources_->GetPlayer()->life += 1;
            if (resources_->GetPlayer()->life > 
              resources_->GetPlayer()->max_life) {
              resources_->GetPlayer()->life = resources_->GetPlayer()->max_life;
            }
            resources_->RemoveObject(item);
          } else if (item_id == 27) { // +1 life.
            if (resources_->GetPlayer()->max_life - 5 > configs->dungeon_level) {
              resources_->GetPlayer()->life += 1;
              if (resources_->GetPlayer()->life > 
                resources_->GetPlayer()->max_life ) {
                resources_->GetPlayer()->life = resources_->GetPlayer()->max_life;
              }
              resources_->RemoveObject(item);
            } else {
              resources_->GetPlayer()->max_life++;
              resources_->GetConfigs()->max_life++;
              resources_->GetPlayer()->life++;
              resources_->RemoveObject(item);
            }
          } else if (item_id == 28) { // +1 mana.
            if (resources_->GetPlayer()->max_mana - 10 > configs->dungeon_level) {
              resources_->GetPlayer()->mana += 1;
              if (resources_->GetPlayer()->mana > 
                resources_->GetPlayer()->max_mana) {
                resources_->GetPlayer()->mana = resources_->GetPlayer()->max_mana;
              }
              resources_->RemoveObject(item);
            } else {
              resources_->GetPlayer()->max_mana++;
              resources_->GetConfigs()->max_mana++;
              resources_->GetPlayer()->mana++;
              resources_->RemoveObject(item);
            }
          } else {
            int quantity = item->quantity;
            if (resources_->InsertItemInInventory(item_id, quantity)) {
              resources_->RemoveObject(item);
              resources_->AddMessage(string("You picked a " + 
                item->GetDisplayName()));
            } else {
              resources_->AddMessage("Inventory is full.");
            }
          }
        }
      }
    }

    if (item->type == GAME_OBJ_ACTIONABLE) {
      shared_ptr<Actionable> actionable = static_pointer_cast<Actionable>(item);
      if (actionable->state == 0) {
        configs->interacting_item = item;
        hit = true;
      }
    } else {
      configs->interacting_item = item;
      hit = true;
    }
  }
  return hit;
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
            // p.height += 0.01f * clamp(mean_height - p.height, mean_height, 1000.0f);
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
  // cout << obj->position << endl;
  // obj->position = obj->position + vec3(1.0, 0, 0);
  // resources_->UpdateObjectPosition(obj);
  // cout << obj->position << endl;
  // cout << "==================" << endl;
  // return;

  vec3 start = c.position;
  vec3 end = c.position + c.direction * 500.0f;
  if (configs->place_axis == -1) {
    ivec2 tile;
    if (resources_->CollideRayAgainstTerrain(start, end, tile)) {
      TerrainPoint p = resources_->GetHeightMap().GetTerrainPoint(tile.x, tile.y);
      obj->position = vec3(tile.x, p.height, tile.y);
      obj->target_position = obj->position;
      obj->prev_position = obj->position;
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
        obj->target_position = obj->position;
        obj->prev_position = obj->position;
        resources_->UpdateObjectPosition(obj);
      } else {
        obj->position = q;
        obj->target_position = obj->position;
        obj->prev_position = obj->position;
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
    if (configs->new_building->type == GAME_OBJ_DEFAULT) {
      configs->new_building->CalculateCollisionData();
      resources_->GenerateOptimizedOctree();
    }

    configs->place_object = false;
    configs->scale_object = false;
    configs->scale_pivot = configs->new_building->position;
    configs->new_building = nullptr;
  }
}

Camera PlayerInput::GetCamera() {
  return resources_->GetCamera();
}

bool PlayerInput::DecreaseCharges(int item_id) {
  if (!resources_->InventoryHasItem(25)) {
    return false;
  }

  shared_ptr<Configs> configs = resources_->GetConfigs();
  int (&item_matrix)[10][5] = configs->item_matrix;
  int (&item_quantities)[10][5] = configs->item_quantities;

  ivec2 item_pos = resources_->GetInventoryItemPosition(item_id);

  if (item_quantities[item_pos.x][item_pos.y] <= 0) {
    item_matrix[item_pos.x][item_pos.y] = 0;
    return false;
  }

  item_quantities[item_pos.x][item_pos.y]--;
  if (item_quantities[item_pos.x][item_pos.y] <= 0) {
    item_matrix[item_pos.x][item_pos.y] = 0;
  }
  return true;
}

bool PlayerInput::UseItem(int position) {
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  shared_ptr<GameObject> scepter = resources_->GetObjectByName("scepter-001");

  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();

  if (configs->render_scene == "town") {
    return false;
  }

  int item_id = configs->active_items[position];
  if (item_id == 0) return false;

  switch (item_id) {
    case 3: {
      player->RestoreHealth(5);
      configs->active_items[position] = 0;
      debounce_ = 20;
      return true;
    } 
    case 4: {
      player->RestoreMana(10);
      configs->active_items[position] = 0;
      debounce_ = 20;
      return true;
    } 
    case 5: 
    case 6: 
    case 25: {
      obj->active_animation = "Armature|shoot_scepter";
      scepter->active_animation = "Armature|shoot_scepter";
      current_spell_ = resources_->GetArcaneSpellData()[0];

      player->scepter = item_id;
      player->charges = 10;
      player->scepter_timeout = glfwGetTime() + 30;
      StartDrawing();
      configs->active_items[position] = 0;
      obj->frame = 0;
      scepter->frame = 0;
      return true;
    }
    case 41: {
      configs->town_portal_active = true;
      configs->town_portal_dungeon_level = configs->dungeon_level;
      resources_->DeleteAllObjects();
      resources_->CreateTown();
      resources_->LoadCollisionData("resources/objects/collision_data.xml");
      resources_->CalculateCollisionData();
      resources_->GenerateOptimizedOctree();
      resources_->GetConfigs()->render_scene = "town";
      resources_->GetPlayer()->ChangePosition(vec3(11628, 141, 7246));
      resources_->SaveGame();
      configs->active_items[position] = 0;
      return true;
    }
    case 44: {
      Dungeon& dungeon = resources_->GetDungeon();
      dungeon.Reveal();
      configs->active_items[position] = 0;
      return true;
    }
    case 45: {
      player->armor = 3;
      configs->active_items[position] = 0;
      return true;
    }
    case 46: { // Scroll of Door.
      obj->active_animation = "Armature|shoot";
      player->player_action = PLAYER_CASTING;
      obj->frame = 0;
      scepter->frame = 0;
      animation_frame_ = 60;
      resources_->CreateChargeMagicMissileEffect("particle-sparkle-green");
      player->selected_spell = 3;
      current_spell_ = resources_->GetArcaneSpellData()[3];
      debounce_ = 0;
      configs->active_items[position] = 0;
      return true;
    }
    case 47: { // Scroll of Trap.
      obj->active_animation = "Armature|shoot";
      player->player_action = PLAYER_CASTING;
      obj->frame = 0;
      scepter->frame = 0;
      animation_frame_ = 60;
      resources_->CreateChargeMagicMissileEffect("particle-sparkle-green");
      player->selected_spell = 5;
      current_spell_ = resources_->GetArcaneSpellData()[5];
      debounce_ = 0;
      configs->active_items[position] = 0;
      return true;
    }
    case 48: { // Scroll of Decoy.
      obj->active_animation = "Armature|shoot";
      player->player_action = PLAYER_CASTING;
      obj->frame = 0;
      scepter->frame = 0;
      animation_frame_ = 60;
      resources_->CreateChargeMagicMissileEffect("particle-sparkle-green");
      player->selected_spell = 7;
      current_spell_ = resources_->GetArcaneSpellData()[7];
      debounce_ = 0;
      configs->active_items[position] = 0;
      return true;
    }
    case 49: { // Scroll of Detect.
      obj->active_animation = "Armature|shoot";
      player->player_action = PLAYER_CASTING;
      obj->frame = 0;
      scepter->frame = 0;
      animation_frame_ = 60;
      resources_->CreateChargeMagicMissileEffect("particle-sparkle-green");
      player->selected_spell = 4;
      current_spell_ = resources_->GetArcaneSpellData()[4];
      debounce_ = 0;
      configs->active_items[position] = 0;
      return true;
    }
    case 57: { // Teleport Rod.
      resources_->UseTeleportRod();
      configs->active_items[position] = 0;
      return true;
    }
    case 58: { // Concoction of Quick Casting.
      player->AddTemporaryStatus(make_shared<QuickCastingStatus>(30.0f, 1));
      configs->active_items[position] = 0;
      return true;
    }
    case 59: { // Concoction of Mana Regen.
      player->AddTemporaryStatus(make_shared<ManaRegenStatus>(30.0f, 1));
      configs->active_items[position] = 0;
      return true;
    }
    default: {
      break;
    }
  }
  return false;
}

bool PlayerInput::CastSpellOrUseItem() {
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  shared_ptr<GameObject> scepter = resources_->GetObjectByName("scepter-001");

  if (resources_->IsHoldingScepter()) {
    if (player->charges <= 0) {
      player->scepter = -1;
      player->charges = 0;
      player->scepter_timeout = 0;
      return false;
    }

    player->charges--;

    player->player_action = PLAYER_CASTING;
    obj->frame = 0;
    scepter->frame = 0;
    animation_frame_ = 60;
    resources_->CreateChargeMagicMissileEffect();
    debounce_ = 20;

    switch (player->scepter) {
      case 5: {
        player->selected_spell = 9;
        break;
      }
      case 6: {
        player->selected_spell = 1;
        break;
      }
      case 25: {
        player->selected_spell = 0;
        break;
      }
    }
    current_spell_ = resources_->GetArcaneSpellData()[player->selected_spell];
    return true;
  }

  if (configs->render_scene == "town") {
    return false;
  }
  
  shared_ptr<ArcaneSpellData> arcane_spell =  
    resources_->GetArcaneSpell(configs->selected_spell);
  if (arcane_spell) {
    current_spell_ = arcane_spell;

    Camera c = GetCamera();

    if (player->mana >= arcane_spell->mana_cost) {
      // player->mana -= arcane_spell->mana_cost;
    } else {
      return false;
    }

    int spell_id = arcane_spell->spell_id;

    switch (spell_id) {
      case 0: { // Spell shot.
        // if (player->stamina > 0.0f && player->mana >= 1.0f) {
          obj->active_animation = "Armature|shoot";

          player->player_action = PLAYER_CASTING;
          obj->frame = 0;
          scepter->frame = 0;
          animation_frame_ = 60;
          resources_->CreateChargeMagicMissileEffect();
          player->selected_spell = 0;
          debounce_ = 20;
        //   return true;
        // }
        return false;
      }
      case 1: { // Windslash.
        if (!player->CanUseAbility("windslash")) return false;
        // TODO: set spell cost in config.
        // if (player->stamina > 0.0f && player->mana >= 2.0f) {
          obj->active_animation = "Armature|shoot";
          player->player_action = PLAYER_CASTING;
          obj->frame = 0;
          scepter->frame = 0;
          animation_frame_ = 60;
          resources_->CreateChargeMagicMissileEffect();
          player->selected_spell = 1;
          --arcane_spell->quantity;
          debounce_ = 20;

          // if (--arcane_spell->quantity == 0) {
          //   if (!SelectSpell(arcane_spell->spell_id + 1)) {
          //     SelectSpell(0);
          //   }
          // }
          return true;
        return false;
      }
      case 2: { // Fireball.
        // if (player->stamina > 0.0f && player->mana >= arcane_spell->mana_cost) {
          obj->active_animation = "Armature|shoot";
          player->player_action = PLAYER_CASTING;
          obj->frame = 0;
          scepter->frame = 0;
          animation_frame_ = 60;
          resources_->CreateChargeMagicMissileEffect("particle-sparkle-fire");
          player->selected_spell = 6;
          debounce_ = 0;
          return true;
        // }
        return false;
      }
      case 3: { // Magma Ray.
        // if (player->stamina > 0.0f && player->mana >= arcane_spell->mana_cost) {
          player->mana += arcane_spell->mana_cost;
          obj->active_animation = "Armature|channel_ray";
          player->player_action = PLAYER_CHANNELING;
          channel_until_ = glfwGetTime() + 1.0f;
          obj->frame = 0;
          scepter->frame = 0;
          animation_frame_ = 60;
          resources_->CreateChargeMagicMissileEffect("particle-sparkle-fire");
          player->selected_spell = 8;
          debounce_ = 0;
          return true;
        // }
        return false;
      }
      case 4: { // Shotgun.
        // TODO: set spell cost in config.
        // if (player->stamina > 0.0f && player->mana >= arcane_spell->mana_cost) {
          obj->active_animation = "Armature|shoot";
          player->player_action = PLAYER_CASTING;
          obj->frame = 0;
          scepter->frame = 0;
          animation_frame_ = 60;
          resources_->CreateChargeMagicMissileEffect();
          player->selected_spell = 0;
          debounce_ = 20;
          return true;
        // }
        return false;
      }
      case 5: { // Flash.
        // TODO: set spell cost in config.
        // if (player->stamina > 0.0f && player->mana >= 1.0f) {
          obj->active_animation = "Armature|shoot";
          player->player_action = PLAYER_CASTING;
          obj->frame = 0;
          scepter->frame = 0;
          animation_frame_ = 60;
          resources_->CreateChargeMagicMissileEffect();
          player->selected_spell = 2;
          debounce_ = 20;
          return true;
        // }
        return false;
      }
      case 6: { // Spell wall.
        // TODO: set spell cost in config.
        // if (player->stamina > 0.0f && player->mana >= 0.1f) {
          obj->active_animation = "Armature|shoot";
          player->player_action = PLAYER_CASTING;
          obj->frame = 0;
          scepter->frame = 0;
          animation_frame_ = 60;
          resources_->CreateChargeMagicMissileEffect("particle-sparkle-fire");
          player->selected_spell = 3;
          debounce_ = 0;
          return true;
        // }
        return false;
      }
      case 7: { // Power shot.
        resources_->ChangeObjectAnimation(obj, "Armature|start_charging", true,
          TRANSITION_FINISH_ANIMATION);

        player->player_action = PLAYER_CHANNELING;
        channel_until_ = glfwGetTime() + 5.0f;
        obj->frame = 0;
        scepter->frame = 0;
        animation_frame_ = 180;
        player->selected_spell = 8;
        debounce_ = 0;
        return true;
      }
      case 8: { // Trap.
        // TODO: set spell cost in config.
        // if (player->stamina > 0.0f && player->mana >= arcane_spell->mana_cost) {
          obj->active_animation = "Armature|shoot";
          player->player_action = PLAYER_CASTING;
          obj->frame = 0;
          scepter->frame = 0;
          animation_frame_ = 60;
          resources_->CreateChargeMagicMissileEffect("particle-sparkle-fire");
          player->selected_spell = 5;
          debounce_ = 0;
          return true;
        // }
        return false;
      }
      case 9: { // Decoy.
        // if (player->stamina > 0.0f && player->mana >= arcane_spell->mana_cost) {
          obj->active_animation = "Armature|shoot";
          player->player_action = PLAYER_CASTING;
          obj->frame = 0;
          scepter->frame = 0;
          animation_frame_ = 60;
          resources_->CreateChargeMagicMissileEffect("particle-sparkle-fire");
          player->selected_spell = 7;
          debounce_ = 0;
          return true;
        // }
        return false;
      }
      case 10: { // Detect monsters.
        // if (player->stamina > 0.0f && arcane_spell->mana_cost) {
          obj->active_animation = "Armature|shoot";
          player->player_action = PLAYER_CASTING;
          obj->frame = 0;
          scepter->frame = 0;
          animation_frame_ = 60;
          resources_->CreateChargeMagicMissileEffect();
          player->selected_spell = 8;
          debounce_ = 20;
          return true;
        // }
        return false;
      }
    }
    return false;
  }

  return true;
}

bool PlayerInput::CastShield() {
  Camera c = GetCamera();

  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  shared_ptr<GameObject> scepter = resources_->GetObjectByName("scepter-001");

  if (!player->CanUseAbility("push")) return false;

  if (resources_->IsHoldingScepter()) {
    return false;
  }

  if (configs->render_scene == "town") {
    return false;
  }

  obj->active_animation = "Armature|open_palm";

  player->player_action = PLAYER_SHIELD;
  obj->frame = 0;
  scepter->frame = 0;
  animation_frame_ = 60;

  debounce_ = 20;
  return true;
}

void PlayerInput::ProcessPlayerDrawing() {
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  shared_ptr<GameObject> scepter = resources_->GetObjectByName("scepter-001");

  obj->active_animation = "Armature|draw_scepter";
  scepter->active_animation = "Armature|draw_scepter";

  shared_ptr<Mesh> mesh = resources_->GetMesh(obj);
  int num_frames = GetNumFramesInAnimation(*mesh, obj->active_animation);
  if (obj->frame >= num_frames-1) {
    obj->frame = 0;
    scepter->frame = 0;
    obj->repeat_animation = true;
    player->player_action = PLAYER_IDLE;
  }
}

void PlayerInput::ProcessPlayerFlipping() {
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  shared_ptr<GameObject> scepter = resources_->GetObjectByName("scepter-001");

  resources_->ChangeObjectAnimation(obj, "Armature|flip_page", true,
    TRANSITION_NONE);
  resources_->ChangeObjectAnimation(scepter, "Armature|flip_page", true,
    TRANSITION_NONE);

  shared_ptr<Mesh> mesh = resources_->GetMesh(obj);
  int num_frames = GetNumFramesInAnimation(*mesh, obj->active_animation);
  if (obj->frame >= num_frames-10) {
    obj->frame = 0;
    scepter->frame = 0;
    obj->active_animation = "Armature|idle.001";
    scepter->active_animation = "Armature|idle.001";
    player->player_action = PLAYER_IDLE;
  }
}

void PlayerInput::ProcessPlayerCasting() {
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  shared_ptr<GameObject> scepter = resources_->GetObjectByName("scepter-001");
  Camera c = GetCamera();

  if (resources_->IsHoldingScepter()) {
    obj->active_animation = "Armature|shoot_scepter";
    scepter->active_animation = "Armature|shoot_scepter";
  } else {
    obj->active_animation = "Armature|shoot";
    scepter->active_animation = "Armature|shoot";
  }

  if (animation_frame_ == 0 && !lft_click_) {
    if (resources_->IsHoldingScepter()) {
      obj->active_animation = "Armature|hold_scepter";
      scepter->active_animation = "Armature|hold_scepter";
    } else {
      obj->active_animation = "Armature|idle.001";
      scepter->active_animation = "Armature|idle.001";
    }
    obj->frame = 0;
    scepter->frame = 0;

    player->player_action = PLAYER_IDLE;
    current_spell_ = nullptr;
  } else if (animation_frame_ <= 0 && lft_click_) {
    if (current_spell_ && current_spell_->spell_id == 0) {
      if (CastSpellOrUseItem()) {
        return;
      }
    }

    if (resources_->IsHoldingScepter()) {
      obj->active_animation = "Armature|hold_scepter";
      scepter->active_animation = "Armature|hold_scepter";
    } else {
      obj->active_animation = "Armature|idle.001";
      scepter->active_animation = "Armature|idle.001";
    }
    obj->frame = 0;
    scepter->frame = 0;

    player->player_action = PLAYER_IDLE;
  } else if (animation_frame_ == 20) {
    ObjPtr waypoint_obj = resources_->GetObjectByName("waypoint-001");
    if (current_spell_) {
      int spell_id = current_spell_->spell_id;
      if (configs->gameplay_style == GAMEPLAY_RANDOM) {     
        if (spell_id == 2) spell_id = 5;
        if (spell_id == 3) spell_id = 9;
      }

      switch (spell_id) {
        case 0:
          resources_->CastSpellShot(camera_);
          break;
        case 1: {
          resources_->CastWindslash(camera_);
          player->cooldowns["windslash"] = glfwGetTime() + 10.0f;
          break;
        }
        case 2:
          resources_->CastFireball(player, c.direction);
          break;
        case 4:
          resources_->CastShotgun(camera_);
          break;
        case 5:
          resources_->CastFlashMissile(camera_);
          break;
        case 6: {
          spell_wall_pos_ = resources_->GetSpellWallRayCollision(player, c.position, 
            c.direction);
          waypoint_obj->draw = true;

          waypoint_obj->position = spell_wall_pos_;
          if (obj->position.y < 0) obj->position.y = 0;
          resources_->UpdateObjectPosition(obj);
          resources_->CastSpellWall(player, spell_wall_pos_);
          break; 
        }
        case 8: {
          trap_pos_ = resources_->GetTrapRayCollision(player, c.position, 
            c.direction);

          waypoint_obj->draw = true;

          waypoint_obj->position = trap_pos_;
          if (obj->position.y < 0) obj->position.y = 0;
          resources_->UpdateObjectPosition(obj);

          resources_->CastSpellTrap(player, trap_pos_);
          break;
        }
        case 9: {
          trap_pos_ = resources_->GetTrapRayCollision(player, c.position, 
            c.direction);

          waypoint_obj->draw = true;

          waypoint_obj->position = trap_pos_;
          if (obj->position.y < 0) obj->position.y = 0;
          resources_->UpdateObjectPosition(obj);

          resources_->CastDecoy(player, trap_pos_);
          break;
        }
        case 10:
          resources_->CastDetectMonsters();
          break;
        default:
          break;
      }
    }
  }
}

void PlayerInput::ProcessPlayerShield() {
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  shared_ptr<GameObject> scepter = resources_->GetObjectByName("scepter-001");
  Camera c = GetCamera();

  debounce_ = 20;

  obj->active_animation = "Armature|open_palm";
  scepter->active_animation = "Armature|open_palm";

  if (resources_->IsHoldingScepter() || animation_frame_ == 0 || 
    (!rgt_click_ && animation_frame_ > 40)) {
    if (resources_->IsHoldingScepter()) {
      obj->active_animation = "Armature|hold_scepter";
      scepter->active_animation = "Armature|hold_scepter";
    } else {
      obj->active_animation = "Armature|idle.001";
      scepter->active_animation = "Armature|idle.001";
    }
    obj->frame = 0;
    scepter->frame = 0;

    player->player_action = PLAYER_IDLE;
    current_spell_ = nullptr;
  } else if (animation_frame_ == 40) {
    shared_ptr<Mesh> mesh = resources_->GetMesh(obj);
    int bone_id = mesh->bones_to_ids["hand"];
    BoundingSphere bs = obj->GetBoneBoundingSphere(bone_id);
    shared_ptr<Particle> p = resources_->CreateOneParticle(
      player->position + c.direction * 5.0f, 60.0f, "shield_spell_effect", 5.0f);
    p->associated_obj = obj;
    p->offset = vec3(0);
    p->associated_bone = bone_id;
    // player->AddTemporaryStatus(make_shared<ShieldStatus>(0.5f, 1));
    resources_->CastPush(player->position);
    player->cooldowns["push"] = glfwGetTime() + 10.0f;
  } 
}

void PlayerInput::ProcessPlayerChanneling() {
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  shared_ptr<GameObject> scepter = resources_->GetObjectByName("scepter-001");
  Camera c = GetCamera();

  if (!current_spell_) {
    return;
  }

  switch (current_spell_->spell_id) {
    case 3: { // Magma Ray.
      if (obj->frame < 20) {
      } else if (lft_click_ && player->mana > current_spell_->mana_cost) { //  && glfwGetTime() < channel_until_
        player->mana -= current_spell_->mana_cost;
        obj->frame = 20;
        scepter->frame = 20;
        animation_frame_ = 0;

        if (!channeling_particle || channeling_particle->life < 0.0f) {
          channeling_particle = 
            resources_->CreateChargeMagicMissileEffect("particle-sparkle-fire");
        }
        resources_->CastMagmaRay(player, c.position, c.direction);
      } else {
        obj->frame = 0;
        scepter->frame = 0;
        player->player_action = PLAYER_IDLE;
        if (channeling_particle) {
          channeling_particle->life = 0.0f;
        }
        channeling_particle = nullptr;
      }
      break;
    }
    case 7: { // Power Shot.
      if (obj->transition_animation && 
        obj->active_animation == "Armature|start_charging") {
        break;
      }

      // resources_->ChangeObjectAnimation(obj, "Armature|charge", true,
      //   TRANSITION_WAIT);

      if (animation_frame_ < 0) {
        animation_frame_ = 0;
      }

      float channeling_power = float(180 - animation_frame_) / 180.0f;
      int level = 0;
      if (channeling_power < 0.5f) {
        if (channeling_particle && channeling_particle->life > 0.0f && channeling_particle->particle_type->name == "particle-sparkle-green") {
        } else {
          if (channeling_particle) channeling_particle->life = 0.0f;
          channeling_particle = 
            resources_->CreateChargeOpenHandEffect("particle-sparkle-green");
        }
      } else if (channeling_power < 0.75f) {
        if (channeling_particle && channeling_particle->life > 0.0f && channeling_particle->particle_type->name == "particle-sparkle-fire") {
        } else {
          if (channeling_particle) channeling_particle->life = 0.0f;
          channeling_particle = 
            resources_->CreateChargeOpenHandEffect("particle-sparkle-fire");
        }
        level = 1;
      } else {
        if (channeling_particle && channeling_particle->life > 0.0f && channeling_particle->particle_type->name == "particle-sparkle") {
        } else {
          if (channeling_particle) channeling_particle->life = 0.0f;
          channeling_particle = 
            resources_->CreateChargeOpenHandEffect("particle-sparkle");
        }
        level = 2;
      }

      if (!lft_click_) { // Finished channeling.
        if (channeling_particle) channeling_particle->life = 0.0f;
        resources_->ChangeObjectAnimation(obj, "Armature|charge_release", true,
          TRANSITION_FINISH_ANIMATION);
        player->player_action = PLAYER_IDLE;
        current_spell_ = nullptr;
        resources_->CastMagicMissile(camera_, level);
      }
      break;
    }
  }
}

void PlayerInput::StartDrawing() {
  if (!resources_->IsHoldingScepter()) return;
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  shared_ptr<GameObject> scepter = resources_->GetObjectByName("scepter-001");
  obj->frame = 0;
  obj->repeat_animation = false;
  scepter->frame = 0;

  shared_ptr<Player> player = resources_->GetPlayer();
  player->player_action = PLAYER_DRAWING;
}

void PlayerInput::StartFlipping() {
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  shared_ptr<GameObject> scepter = resources_->GetObjectByName("scepter-001");
  obj->frame = 0;
  obj->repeat_animation = false;
  scepter->frame = 0;

  shared_ptr<Player> player = resources_->GetPlayer();
  player->player_action = PLAYER_FLIPPING;
}

bool PlayerInput::SelectSpell(int spell_id) {
  shared_ptr<ArcaneSpellData> arcane_spell = resources_->GetArcaneSpell(spell_id);

  if (!arcane_spell) return false;
  if (arcane_spell->spell_id > 0 && !arcane_spell->learned) return false;

  shared_ptr<Configs> configs = resources_->GetConfigs();
  configs->selected_spell = spell_id;
  StartFlipping();
  return true;
}

void PlayerInput::ProcessMovement() {
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();

  float d = resources_->GetDeltaTime() / 0.016666f;

  int num_keys = 0;
  if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS) num_keys++;
  if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS) num_keys++;
  if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS) num_keys++;
  if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) num_keys++;
 
  float player_speed = resources_->GetConfigs()->player_speed; 
  float jump_force = resources_->GetConfigs()->jump_force; 

  float speed = player_speed;
  if (num_keys > 1) {
    speed *= 0.707106781;
  }

  float cur_time = glfwGetTime();

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

  // Move forward.
  if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS) {
    if (cur_time < pressed_w_ && !holding_w_ && cur_time < started_holding_w_ + 0.3f) {
      player->can_jump = false;
      player->speed += front * speed * d * 20.0f;
      player->speed.y += jump_force * 1.0f;
      configs->jumped = true;
      pressed_w_ = 0.0f;
    } else if (!holding_w_) {
      started_holding_w_ = glfwGetTime();
      player->speed += front * speed * d;
      pressed_w_ = glfwGetTime() + 0.3f;
      holding_w_ = true;
    } else {
      player->speed += front * speed * d;
      pressed_w_ = glfwGetTime() + 0.3f;
      holding_w_ = true;
    }
  } else {
    holding_w_ = false;
  }

  // Move backward.
  if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS) {
    if (cur_time < pressed_s_ && !holding_s_ && cur_time < started_holding_s_ + 0.3f) {
      player->can_jump = false;
      player->speed -= front * speed * d * 20.0f;
      player->speed.y += jump_force * 1.0f;
      configs->jumped = true;
      pressed_w_ = 0.0f;
    } else if (!holding_s_) {
      started_holding_s_ = glfwGetTime();
      player->speed -= front * speed * d;
      pressed_s_ = glfwGetTime() + 0.3f;
      holding_s_ = true;
    } else {
      player->speed -= front * speed * d;
      pressed_s_ = glfwGetTime() + 0.3f;
      holding_s_ = true;
    }
  } else {
    holding_s_ = false;
  }

  // Strafe right.
  if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) {
    if (cur_time < pressed_d_ && !holding_d_ && cur_time < started_holding_d_ + 0.3f) {
      player->can_jump = false;
      player->speed += right * speed * d * 20.0f;
      player->speed.y += jump_force * 1.0f;
      configs->jumped = true;
      pressed_d_ = 0.0f;
    } else if (!holding_d_) {
      started_holding_d_ = glfwGetTime();
      player->speed += right * speed * d;
      pressed_d_ = glfwGetTime() + 0.3f;
      holding_d_ = true;
    } else {
      player->speed += right * speed * d;
      pressed_d_ = glfwGetTime() + 0.3f;
      holding_d_ = true;
    }
  } else {
    holding_d_ = false;
  }

  // Strafe left.
  if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS) {
    if (cur_time < pressed_a_ && !holding_a_ && cur_time < started_holding_a_ + 0.3f) {
      player->can_jump = false;
      player->speed -= right * speed * d * 20.0f;
      player->speed.y += jump_force * 1.0f;
      configs->jumped = true;
      pressed_a_ = 0.0f;
    } else if (!holding_a_) {
      started_holding_a_ = glfwGetTime();
      player->speed -= right * speed * d;
      pressed_a_ = glfwGetTime() + 0.3f;
      holding_a_ = true;
    } else {
      player->speed -= right * speed * d;
      pressed_a_ = glfwGetTime() + 0.3f;
      holding_a_ = true;
    }
  } else {
    holding_a_ = false;
  }
}

void PlayerInput::CastWeave() {
  float t;
  vec3 q;
  ObjPtr obj = resources_->IntersectRayObjects(camera_.position, 
    camera_.direction, 100, INTERSECT_ALL, t, q);

  if (!obj) return;
  if (obj->GetAsset()->name != "weave_vortex") return;

  // obj->speed = vec3(0, 0.1, 0);
  obj->active_weave = true;

  shared_ptr<Configs> configs = resources_->GetConfigs();
  configs->selected_weave_vortices.push_back(obj);
}

void PlayerInput::CastRandomSpell() {
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  shared_ptr<GameObject> scepter = resources_->GetObjectByName("scepter-001");

  if (configs->random_spells.empty()) return;

  int spell_id = configs->random_spells[0];
  configs->random_spells.erase(configs->random_spells.begin());

  shared_ptr<ArcaneSpellData> arcane_spell = 
    resources_->GetArcaneSpell(spell_id);
  if (!arcane_spell) return;

  current_spell_ = arcane_spell;

  Camera c = GetCamera();

  switch (arcane_spell->spell_id) {
    case 0: { // Spell shot.
      obj->active_animation = "Armature|shoot";

      player->player_action = PLAYER_CASTING;
      obj->frame = 0;
      scepter->frame = 0;
      animation_frame_ = 60;
      resources_->CreateChargeMagicMissileEffect();
      player->selected_spell = 0;
      debounce_ = 20;
      return;
    }
    case 1: { // Windslash.
      if (!player->CanUseAbility("windslash")) return;
      obj->active_animation = "Armature|shoot";
      player->player_action = PLAYER_CASTING;
      obj->frame = 0;
      scepter->frame = 0;
      animation_frame_ = 60;
      resources_->CreateChargeMagicMissileEffect();
      player->selected_spell = 1;
      --arcane_spell->quantity;
      debounce_ = 20;
      return;
    }
    case 2: { // Flash.
      obj->active_animation = "Armature|shoot";
      player->player_action = PLAYER_CASTING;
      obj->frame = 0;
      scepter->frame = 0;
      animation_frame_ = 60;
      resources_->CreateChargeMagicMissileEffect();
      player->selected_spell = 2;
      debounce_ = 20;
      return;
    }
    case 3: { // Decoy.
      obj->active_animation = "Armature|shoot";
      player->player_action = PLAYER_CASTING;
      obj->frame = 0;
      scepter->frame = 0;
      animation_frame_ = 60;
      resources_->CreateChargeMagicMissileEffect();
      player->selected_spell = 3;
      debounce_ = 20;
      return;
    }
  }
}

Camera PlayerInput::ProcessInput(GLFWwindow* window) {
  static double last_time = glfwGetTime();
  double current_time = glfwGetTime();

  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();

  window_ = window;
  --debounce_;
  --animation_frame_;

  if (configs->quick_casting) {
    --animation_frame_;
  }

  Camera c = GetCamera();
  camera_ = c;

  if (player->status == STATUS_DEAD) {
    return c;
  }

  if (resources_->GetGameState() == STATE_EDITOR) return c;

  // Change orientation.
  double x_pos = 0, y_pos = 0;
  int focused = glfwGetWindowAttrib(window, GLFW_FOCUSED);
  if (focused) {
    glfwGetCursorPos(window, &x_pos, &y_pos);
    glfwSetCursorPos(window, 0, 0);
  }

  float mouse_sensitivity = 0.003f;
  player->rotation.y += mouse_sensitivity * float(-x_pos);
  player->rotation.x += mouse_sensitivity * float(-y_pos);
  if (player->rotation.x < -1.57f) player->rotation.x = -1.57f;
  if (player->rotation.x >  1.57f) player->rotation.x = +1.57f;

  if (!player->actions.empty()) {
    return c;
  }

  lft_click_ = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  rgt_click_ = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

  float player_speed = resources_->GetConfigs()->player_speed; 
  float jump_force = resources_->GetConfigs()->jump_force; 

  float d = resources_->GetDeltaTime() / 0.016666f;
  if (resources_->GetGameState() == STATE_INVENTORY) {
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
      if (throttle_counter_ < 0) {
        inventory_->Disable();
        glfwSetCursorPos(window, 0, 0);
      }
      throttle_counter_ = 20;
    }
  } else {
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && false) { // This is disabled.
      if (throttle_counter_ < 0) {
        if (resources_->CanRest()) {
          float d = resources_->GetDeltaTime() / 0.016666f;
          configs->rest_bar += d;

          if (configs->rest_bar > 60.0f) {
            configs->rest_bar = 0.0f;
            // if (++resources_->GetPlayer()->life > resources_->GetPlayer()->max_life) {
            //   resources_->GetPlayer()->life = resources_->GetPlayer()->max_life;
            // }

            if (++resources_->GetPlayer()->mana > resources_->GetPlayer()->max_mana) {
              resources_->GetPlayer()->mana = resources_->GetPlayer()->max_mana;
            }
          }
          throttle_counter_ = 0;
        } else {
          resources_->AddMessage(string("You can't rest now."));
          throttle_counter_ = 20;
        }
      }
    } else {
      configs->rest_bar = 0.0f;

      if (player->touching_the_ground) {
        ProcessMovement();
      }

      // Move up.
      if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (player->can_jump || configs->levitate) {
          player->can_jump = false;
          player->speed.y += jump_force;
          configs->jumped = true;
        }
      }

      // Move down.
      player->running = false;
      if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        if (player->stamina > 0.0f) {
          if (configs->can_run) {
            player->running = true;
          }
        }
      }

      if (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        player->speed.y -= jump_force;
      }
    }
  }

  last_time = current_time;


  ObjPtr waypoint_obj = resources_->GetObjectByName("waypoint-001");
  waypoint_obj->draw = false;

  ObjPtr obj = resources_->GetObjectByName("hand-001");
  ObjPtr scepter = resources_->GetObjectByName("scepter-001");
  switch (player->player_action) {
    case PLAYER_IDLE: {
      if (resources_->IsHoldingScepter()) {
        obj->active_animation = "Armature|hold_scepter";
        scepter->active_animation = "Armature|hold_scepter";
      } else {
        resources_->ChangeObjectAnimation(obj, "Armature|idle.001", true,
          TRANSITION_FINISH_ANIMATION);
      }

      shared_ptr<ArcaneSpellData> arcane_spell = 
        resources_->GetArcaneSpell(configs->selected_spell);
      if (arcane_spell && configs->render_scene != "town") {
        if (arcane_spell->spell_id == 3) { // Spell wall.
          spell_wall_pos_ = resources_->GetSpellWallRayCollision(player, c.position, 
            c.direction);
          waypoint_obj->draw = true;

          waypoint_obj->position = spell_wall_pos_;
          if (obj->position.y < 0) obj->position.y = 0;
          resources_->UpdateObjectPosition(obj);
        } else if (arcane_spell->spell_id == 5 ||
                   arcane_spell->spell_id == 7) { // Trap or decoy.
          trap_pos_ = resources_->GetTrapRayCollision(player, c.position, 
            c.direction);

          waypoint_obj->draw = true;

          waypoint_obj->position = trap_pos_;
          if (obj->position.y < 0) obj->position.y = 0;
          resources_->UpdateObjectPosition(obj);
        }
      }

      if (!lft_click_) {
        for (int i = 0; i < configs->selected_weave_vortices.size(); i++) {
          ObjPtr obj = configs->selected_weave_vortices[i];
          obj->active_weave = false;
          obj->status = STATUS_DEAD;
         
          if (i == configs->selected_weave_vortices.size() - 1) {
            resources_->CastFireExplosion(player, obj->position, vec3(0));
          }
        }
        configs->selected_weave_vortices.clear();
      }

      if (lft_click_) {
        if (debounce_ < 0) {
          if (configs->gameplay_style == GAMEPLAY_DEFAULT) {     
            CastSpellOrUseItem();
          } else if (configs->gameplay_style == GAMEPLAY_WEAVE) {     
            CastWeave();
          } else if (configs->gameplay_style == GAMEPLAY_RANDOM) {     
            CastRandomSpell();
          } 
        } else {
          debounce_ = 20;
        }
      } else if (rgt_click_) {
        if (debounce_ < 0) {
          CastShield();
        } else {
          debounce_ = 20;
        }
      } else if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
        if (debounce_ < 0) {
          // resources_->CastParalysis(player, player->position + c.direction * 100.0f);
          resources_->CastBurningHands(c);
          debounce_ = 20;
        }
      } else if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS) {
        if (debounce_ < 0) {
          resources_->CastBouncyBall(player, c.position, c.direction);
          // resources_->CastWindslash(c);
          // resources_->CastShotgun(c);
          debounce_ = 20;
        }
      }
      break;
    }
    case PLAYER_CASTING: {
      ProcessPlayerCasting();
      break;
    }
    case PLAYER_SHIELD: {
      ProcessPlayerShield();
      break;
    }
    case PLAYER_CHANNELING: {
      ProcessPlayerChanneling();
      break;
    }
    case PLAYER_DRAWING: {
      ProcessPlayerDrawing();
      break;
    }
    case PLAYER_FLIPPING: {
      ProcessPlayerFlipping();
      break;
    }
    default: break;
  }

  throttle_counter_--;
  bool interacted_with_item = false;
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      if (InteractWithItem(window, c, true)) {
        interacted_with_item = true;
      }
    }
    throttle_counter_ = 20;
  } else if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      inventory_->Enable(window, INVENTORY_SPELLBOOK);
      resources_->SetGameState(STATE_INVENTORY);
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
  } else if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      player->scepter = -1;
      player->charges = 0;
      player->scepter_timeout = 0;

      shared_ptr<ArcaneSpellData> arcane_spell =  
        resources_->GetArcaneSpell(configs->selected_spell);

      bool found_spell = false;
      for (int i = arcane_spell->spell_id + 1; i <= 9; i++) {
        if (!SelectSpell(i)) continue;
        found_spell = true;
        break;
      }

      if (!found_spell) {
        for (int i = 0; i < arcane_spell->spell_id; i++) {
          if (!SelectSpell(i)) continue;
          break;
        }
      }

      shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
      shared_ptr<GameObject> scepter = resources_->GetObjectByName("scepter-001");
      obj->frame = 0;
      scepter->frame = 0;
    }
    throttle_counter_ = 5;
  } else if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      player->scepter = -1;
      player->charges = 0;
      player->scepter_timeout = 0;

      configs->selected_tile = 0;
      UseItem(2);
    }
    throttle_counter_ = 5;
  } else if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && 
     glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      player->scepter = -1;
      player->charges = 0;
      player->scepter_timeout = 0;

      UseItem(0);
      // SelectSpell(7);
    }
    throttle_counter_ = 5;
  } else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && 
     glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      player->scepter = -1;
      player->charges = 0;
      player->scepter_timeout = 0;

      UseItem(1);
      // SelectSpell(8);
    }
    throttle_counter_ = 5;
  } else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && 
     glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      player->scepter = -1;
      player->charges = 0;
      player->scepter_timeout = 0;

      UseItem(2);
    }
    throttle_counter_ = 5;
  } else if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      player->scepter = -1;
      player->charges = 0;
      player->scepter_timeout = 0;

      configs->selected_tile = 1;
      // SelectSpell(0);

      configs->selected_spell = 1;
      CastSpellOrUseItem();
      configs->selected_spell = 0;
    }
    throttle_counter_ = 5;
  } else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      player->scepter = -1;
      player->charges = 0;
      player->scepter_timeout = 0;

      SelectSpell(1);
      configs->selected_tile = 2;
    }
    throttle_counter_ = 5;
  } else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      player->scepter = -1;
      player->charges = 0;
      player->scepter_timeout = 0;

      SelectSpell(2);
      configs->selected_tile = 3;
    }
    throttle_counter_ = 5;
  } else if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      player->scepter = -1;
      player->charges = 0;
      player->scepter_timeout = 0;

      SelectSpell(3);
    }
    throttle_counter_ = 5;
  } else if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      player->scepter = -1;
      player->charges = 0;
      player->scepter_timeout = 0;

      SelectSpell(4);
    }
    throttle_counter_ = 5;
  } else if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      player->scepter = -1;
      player->charges = 0;
      player->scepter_timeout = 0;

      SelectSpell(5);
    }
    throttle_counter_ = 5;
  } else if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      player->scepter = -1;
      player->charges = 0;
      player->scepter_timeout = 0;

      SelectSpell(6);
    }
    throttle_counter_ = 5;
  } else if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) {
    if (configs->place_object) {
      if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        configs->new_building->scale += 0.01f;
        if (configs->new_building->type == GAME_OBJ_PARTICLE) {
          shared_ptr<Particle> p = static_pointer_cast<Particle>(configs->new_building);
          p->size += 0.01f;
        }
      } else {
        configs->new_building->rotation_matrix *= rotate(mat4(1.0), -0.005f, vec3(0, 1, 0));
      }
    }
  } else if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) {
    if (configs->place_object) {
      if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        configs->new_building->scale -= 0.01f;
        if (configs->new_building->type == GAME_OBJ_PARTICLE) {
          shared_ptr<Particle> p = static_pointer_cast<Particle>(configs->new_building);
          p->size -= 0.01f;
        }
      } else {
        configs->new_building->rotation_matrix *= rotate(mat4(1.0), 0.005f, vec3(0, 1, 0));
      }
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
