#include "player.hpp"

PlayerInput::PlayerInput(shared_ptr<AssetCatalog> asset_catalog, 
    shared_ptr<Project4D> project_4d, shared_ptr<Craft> craft, 
    shared_ptr<Terrain> terrain,
    shared_ptr<Dialog> dialog)
  : asset_catalog_(asset_catalog), project_4d_(project_4d), craft_(craft), 
    terrain_(terrain), dialog_(dialog) {
}

void PlayerInput::InteractWithItem(const Camera& c) {
  vector<tuple<shared_ptr<GameAsset>, int>>& inventory = asset_catalog_->GetInventory();
  vector<shared_ptr<GameObject>> items = asset_catalog_->GetItems();

  bool hit = false;
  for (auto item : items) {
    vec3 p = c.position;
    vec3 d = c.direction;

    float t;
    vec3 q;
    if (IntersectRaySphere(p, d, item->GetBoundingSphere(), t, q)) {
      inventory.push_back({ item->GetAsset(), 1 });
      asset_catalog_->RemoveObject(item);
      hit = true;
      break;
    }
  }

  if (hit) {
    return;
  }

  shared_ptr<GameObject> obj = asset_catalog_->GetObjectByName("mana-pool-001");

  vec3 p = c.position;
  vec3 d = c.direction;
  float t;
  vec3 q;
  if (IntersectRaySphere(p, d, obj->GetBoundingSphere(), t, q)) {
    craft_->Enable();
    asset_catalog_->SetGameState(STATE_CRAFT);
    hit = true;
  }

  if (hit) {
    return;
  }

  ObjPtr fisherman = asset_catalog_->GetObjectByName("fisherman-001");
  if (IntersectRaySphere(p, d, fisherman->GetBoundingSphere(), t, q)) {
    dialog_->SetDialogOptions({ "Build", "Goodbye" });
    dialog_->Enable();
    asset_catalog_->SetGameState(STATE_DIALOG);
    while (!fisherman->actions.empty()) fisherman->actions.pop();
    fisherman->actions.push(make_shared<TalkAction>());
  }
}

void PlayerInput::Extract(const Camera& c) {
  vector<shared_ptr<GameObject>> extractables = asset_catalog_->GetExtractables();
  for (auto obj : extractables) {
    vec3 p = c.position;
    vec3 d = c.direction;
 
    float t;
    vec3 q;
    if (IntersectRaySphere(p, d, obj->GetBoundingSphere(), t, q)) {
      asset_catalog_->MakeGlow(obj);
      obj->status = STATUS_BEING_EXTRACTED;
    }
  }
}

void PlayerInput::EditTerrain(GLFWwindow* window, const Camera& c) {
  shared_ptr<Configs> configs = asset_catalog_->GetConfigs();
  if (configs->edit_terrain == "none") {
    return;
  }

  int state1 = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
  int state2 = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
  int left_or_right = (state1 == GLFW_PRESS) ? 1 : 0;
  left_or_right = (state2 == GLFW_PRESS) ? -1 : left_or_right;
  if (left_or_right != 0) {
    vec3 start = c.position;
    vec3 end = c.position + c.direction * 200.0f;
    ivec2 tile;
    if (asset_catalog_->CollideRayAgainstTerrain(start, end, tile)) {
      int size = configs->brush_size;

      float mean_height = 0;
      for (int x = -size; x <= size; x++) {
        for (int y = -size; y <= size; y++) {
          TerrainPoint p = asset_catalog_->GetTerrainPoint(tile.x + x, tile.y + y);
          mean_height += p.height;
        }
      }
      mean_height /= 4*size*size;

      for (int x = -size; x <= size; x++) {
        for (int y = -size; y <= size; y++) {
          float distance = sqrt(x*x + y*y);
          if (distance > size) continue;
          float factor = ((size - distance) * (size - distance)) * 0.04f;

          TerrainPoint p = asset_catalog_->GetTerrainPoint(tile.x + x, tile.y + y);
          if (configs->edit_terrain == "tile") {
            p.blending = vec3(0, 0, 0);
            if (configs->selected_tile > 0) {
              p.blending[configs->selected_tile-1] = 1;
            }
            p.tile_set = vec2(0, 0);
          } else if (configs->edit_terrain == "height") {
            if (left_or_right == 1) {
              p.height += 0.03 * factor * configs->raise_factor;
            } else {
              p.height -= 0.03 * factor * configs->raise_factor;
            }
          } else if (configs->edit_terrain == "flatten") {
            p.height += 0.001f * (mean_height - p.height) * factor * configs->raise_factor;
          }
          asset_catalog_->SetTerrainPoint(tile.x+x, tile.y+y, p);
          terrain_->InvalidatePoint(tile + ivec2(x, y));
        }
      }
    }
  }
}

void PlayerInput::Build(GLFWwindow* window, const Camera& c) {
  if (asset_catalog_->GetGameState() != STATE_BUILD) {
    return;
  }

  shared_ptr<Configs> configs = asset_catalog_->GetConfigs();

  vec3 start = c.position;
  vec3 end = c.position + c.direction * 500.0f;
  ivec2 tile;
  if (asset_catalog_->CollideRayAgainstTerrain(start, end, tile)) {
    TerrainPoint p = asset_catalog_->GetTerrainPoint(tile.x, tile.y);
    configs->new_building->position = vec3(tile.x, p.height, tile.y);

    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    if (state == GLFW_PRESS) {
      asset_catalog_->SetGameState(STATE_GAME);
      ObjPtr player = asset_catalog_->GetPlayer();
      shared_ptr<Configs> configs = asset_catalog_->GetConfigs();
      player->position = configs->old_position;
    }
  }
}

void PlayerInput::PlaceObject(GLFWwindow* window, const Camera& c) {
  shared_ptr<Configs> configs = asset_catalog_->GetConfigs();
  if (!configs->place_object) {
    return;
  }

  vec3 start = c.position;
  vec3 end = c.position + c.direction * 500.0f;
  ivec2 tile;
  if (asset_catalog_->CollideRayAgainstTerrain(start, end, tile)) {
    TerrainPoint p = asset_catalog_->GetTerrainPoint(tile.x, tile.y);
    configs->new_building->position = vec3(tile.x, p.height, tile.y);

    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
    if (state == GLFW_PRESS) {
      configs->place_object = false;
      asset_catalog_->RemoveObject(configs->new_building);  
      return;
    }

    state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    if (state == GLFW_PRESS) {
      asset_catalog_->SetGameState(STATE_GAME);
      configs->place_object = false;
    }
  }
}

Camera PlayerInput::ProcessInput(GLFWwindow* window) {
  static double last_time = glfwGetTime();
  double current_time = glfwGetTime();

  static int debounce = 0;
  --debounce;
  static int animation_frame = 0;
  --animation_frame;

  shared_ptr<Player> player = asset_catalog_->GetPlayer();

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
  
  glm::vec3 up = glm::cross(right, direction);

  Camera c = Camera(player->position + vec3(0, 3.0, 0), direction, up);
  c.rotation.x = player->rotation.x;
  c.rotation.y = player->rotation.y;
  
  float player_speed = asset_catalog_->GetConfigs()->player_speed; 
  float jump_force = asset_catalog_->GetConfigs()->jump_force; 

  if (asset_catalog_->GetGameState() == STATE_BUILD) {
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      player->position += front * 10.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      player->position -= front * 10.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      player->position += right * 10.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      player->position -= right * 10.0f;

    if (player->position.x < 11200)
      player->position.x = 11200;
    if (player->position.x > 11800)
      player->position.x = 11800;
    if (player->position.z < 7400)
      player->position.z = 7400;
    if (player->position.z > 7900)
      player->position.z = 7900;

  } else {
    // Move forward.
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      player->speed += front * player_speed;

    // Move backward.
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      player->speed -= front * player_speed;

    // Strafe right.
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      player->speed += right * player_speed;

    // Strafe left.
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      player->speed -= right * player_speed;

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

  double x_pos, y_pos;
  glfwGetCursorPos(window, &x_pos, &y_pos);
  glfwSetCursorPos(window, 0, 0);

  // Change orientation.
  float mouse_sensitivity = 0.003f;
  player->rotation.y += mouse_sensitivity * float(-x_pos);
  player->rotation.x += mouse_sensitivity * float(-y_pos);
  if (player->rotation.x < -1.57f) player->rotation.x = -1.57f;
  if (player->rotation.x >  1.57f) player->rotation.x = +1.57f;
  last_time = current_time;

  shared_ptr<GameObject> obj = asset_catalog_->GetObjectByName("hand-001");
  switch (player->player_action) {
    case PLAYER_IDLE: {
      obj->active_animation = "Armature|idle";
      if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        if (debounce < 0) {
          if (player->num_spells > 0) {
            obj->active_animation = "Armature|shoot";
            player->player_action = PLAYER_CASTING;
            obj->frame = 0;
            animation_frame = 60;
            asset_catalog_->CreateChargeMagicMissileEffect();
            player->selected_spell = 0;
            player->num_spells--;
          }
        }
        debounce = 20;
      } else if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (debounce < 0) {
          if (player->num_spells_2 > 0) {
            obj->active_animation = "Armature|shoot";
            player->player_action = PLAYER_CASTING;
            obj->frame = 0;
            animation_frame = 60;
            asset_catalog_->CreateChargeMagicMissileEffect();
            player->selected_spell = 1;
            player->num_spells_2--;
          }
        }
        debounce = 20;
      } else if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        if (debounce < 0) {
          player->player_action = PLAYER_EXTRACTING;
          obj->active_animation = "Armature|extract";
          animation_frame = 90;
          obj->frame = 0;
        }
      }
      break;
    }
    case PLAYER_CASTING: {
      obj->active_animation = "Armature|shoot";
      if (animation_frame == 0 && glfwGetKey(window, GLFW_KEY_C) != GLFW_PRESS) {
        obj->active_animation = "Armature|idle";
        obj->frame = 0;
        player->player_action = PLAYER_IDLE;
      } else if (animation_frame <= 0 && glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        if (player->num_spells > 0) {
          obj->active_animation = "Armature|shoot";
          obj->frame = 0;
          animation_frame = 60;
          asset_catalog_->CreateChargeMagicMissileEffect();
          player->num_spells--;
        }
      } else if (animation_frame == 20) {
        if (player->selected_spell == 0) {
          asset_catalog_->CastMagicMissile(c);
        } else if (player->selected_spell == 1) {
          Camera c2 = c;
          Camera c3 = c;
          c2.direction = vec3(rotate(mat4(1.0), -0.1f, vec3(0.0f, 1.0f, 0.0f)) * vec4(c.direction, 1.0f));
          c3.direction = vec3(rotate(mat4(1.0), 0.1f, vec3(0.0f, 1.0f, 0.0f)) * vec4(c.direction, 1.0f));

          asset_catalog_->CastMagicMissile(c);
          asset_catalog_->CastMagicMissile(c2);
          asset_catalog_->CastMagicMissile(c3);
        }
      }
      break;
    }
    case PLAYER_EXTRACTING: {
      if (animation_frame == 60) {
        Extract(c);
      }

      if (animation_frame == 0) {
        obj->active_animation = "Armature|idle";
        obj->frame = 0;
        player->player_action = PLAYER_IDLE;
        debounce = 20;
      }
      break;
    }
  }

  throttle_counter_--;
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
    if (throttle_counter_ < 0) {
      InteractWithItem(c);
    }
    throttle_counter_ = 20;
  }

  EditTerrain(window, c);
  Build(window, c);
  PlaceObject(window, c);

  ObjPtr plank = asset_catalog_->GetObjectByName("plank-001");
  plank->position.y = 50;
  plank->speed = vec3(0.01, 0, 0);
  plank->torque = vec3(0.0, 0.5, 0);

  return c;
}
