#include "ai.hpp"

AI::AI(shared_ptr<AssetCatalog> asset_catalog) : asset_catalog_(asset_catalog) {
}

void AI::InitSpider() {
  shared_ptr<GameObject> spider = asset_catalog_->GetObjectByName("spider-001");
  spider->next_waypoint = asset_catalog_->GetWaypointByName("waypoint-001");
}

void AI::ChangeAction(ObjPtr obj, AiAction action) {
  obj->ai_action = action;
  obj->frame = 0;
}

shared_ptr<Waypoint> AI::GetNextWaypoint(ObjPtr obj) {
  const unordered_map<string, shared_ptr<Waypoint>>& waypoints = 
    asset_catalog_->GetWaypoints();

  shared_ptr<Waypoint> res_waypoint = nullptr;
  float min_distance = 99999999.9f;
  for (const auto& [name, waypoint] : waypoints) {
    float distance = length(waypoint->position - obj->position);

    if (distance > min_distance) continue;
    min_distance = distance;
    res_waypoint = waypoint;
  }
  return res_waypoint;
}

bool AI::RotateSpider(ObjPtr spider, vec3 point, float rotation_threshold) {
  float turn_rate = 0.01;

  vec3 to_point = point - spider->position;
  to_point.y = 0;

  const vec3 front = vec3(0, 0, 1);
  const vec3 up = vec3(0, 1, 0);
 
  // At rest position, the object forward direction is +Z and up is +Y.
  spider->forward = normalize(to_point);
  quat h_rotation = RotationBetweenVectors(front, spider->forward);
  quat v_rotation = RotationBetweenVectors(up, spider->up);
  quat target_rotation = v_rotation * h_rotation;

  // Check if object is facing the correct direction.
  vec3 cur_forward = spider->rotation_matrix * front;
  cur_forward.y = 0;
  quat cur_h_rotation = RotationBetweenVectors(front, 
    normalize(cur_forward));

  // bool is_rotating = abs(dot(h_rotation, cur_h_rotation)) < 0.75f;
  bool is_rotating = abs(dot(h_rotation, cur_h_rotation)) < rotation_threshold;
  if (dot(spider->cur_rotation, target_rotation) < 0.999f) {
    spider->cur_rotation =  
      RotateTowards(spider->cur_rotation, target_rotation, turn_rate);
  }
  spider->rotation_matrix = mat4_cast(spider->cur_rotation);
  return is_rotating; 
}

void AI::Move(ObjPtr spider) {
  // TODO: if taking hit
  // if (spider->active_animation == "Armature|hit") {
  //   if (spider->frame == 39) {
  //     spider->active_animation = "Armature|walking";
  //     spider->frame = 0;
  //   }
  // }

  spider->active_animation = "Armature|walking";
  float speed = 0.02;

  // Check if spider reached waypoint.
  if (!spider->next_waypoint) {
    spider->next_waypoint = GetNextWaypoint(spider);
  } else {
    vec3 to_waypoint = spider->next_waypoint->position - spider->position;
    to_waypoint.y = 0;
    if (length(to_waypoint) < 1.0f) {
      int dice = rand() % 1000; 
      if (dice < 500) {
        ChangeAction(spider, WANDER);
        return;
      } else {
        spider->next_waypoint = spider->next_waypoint->next_waypoints[0];
      }
    }
  }

  bool is_rotating = RotateSpider(spider, spider->next_waypoint->position);
  if (!is_rotating) {
    spider->speed += spider->forward * speed;
  }

  int dice = rand() % 700; 
  if (dice < 5) {
    ChangeAction(spider, TURN_TOWARD_TARGET);
  }
}

void AI::TurnTowardPlayer(ObjPtr spider) {
  spider->active_animation = "Armature|walking";

  ObjPtr player = asset_catalog_->GetObjectByName("player");
  bool is_rotating = RotateSpider(spider, player->position, 0.99f);
  if (!is_rotating) {
    ChangeAction(spider, ATTACK);
  }
}

void AI::Attack(ObjPtr spider) {
  spider->active_animation = "Armature|attack";
  if (spider->frame == 44) {
    ObjPtr player = asset_catalog_->GetObjectByName("player");
    vec3 dir = player->position - spider->position;
    // dir.y = 0;

    vec3 forward = normalize(vec3(dir.x, 0, dir.z));
    vec3 right = normalize(cross(forward, vec3(0, 1, 0)));

    float x = dot(dir, forward);
    float y = dot(dir, vec3(0, 1, 0));
    float v = 4.0f;
    float v2 = v * v;
    float v4 = v * v * v * v;
    float g = 0.016f;
    float x2 = x * x;

    // https://en.wikipedia.org/wiki/Projectile_motion
    // Angle required to hit coordinate.
    float tan = (v2 - sqrt(v4 - g * (g * x2 + 2 * y * v2))) / (g * x);
    float angle = atan(tan); 

    dir = vec3(rotate(mat4(1.0f), angle, right) * vec4(forward, 1.0f));  

    // dir = normalize(player->position - spider->position);
    // dir = normalize(dir + vec3(0, 0.2, 0));

    asset_catalog_->CreateParticleEffect(40, 
      spider->position + dir * 5.0f + vec3(0, 2.0, 0), 
      dir * 5.0f, vec3(0.0, 1.0, 0.0), -1.0, 40.0f, 3.0f);

    asset_catalog_->SpiderCastMagicMissile(spider, dir);

  } else if (spider->frame == 79) {
    ChangeAction(spider, WANDER);
  } else {
    // Wait.
  }
}

void AI::Idle(ObjPtr spider) {
  spider->active_animation = "Armature|idle";

  int dice = rand() % 1000; 
  if (dice < 5) {
    ChangeAction(spider, WANDER);
  }
}

void AI::Wander(ObjPtr spider) {
  spider->active_animation = "Armature|walking";
  shared_ptr<Sector> sector = spider->current_sector;

  // TODO: there should be AI scripts to do stuff like this.
  if (spider->current_sector->name == "spider-hole-sector" || 
    spider->current_sector->name == "spider-hole-sector-2") {
    ChangeAction(spider, MOVE);
    return;
  }

  if (!spider->next_waypoint) {
    spider->next_waypoint = GetNextWaypoint(spider);
  }

  ObjPtr player = asset_catalog_->GetObjectByName("player");
  vec3 to_player = player->position - spider->position;
  to_player.y = 0;
  if (length(to_player) < 120.0f) {
    int dice = rand() % 1000; 
    if (dice < 50) {
      ChangeAction(spider, TURN_TOWARD_TARGET);
      return;
    }
  }

  vec3 to_next_location = spider->next_location - spider->position;
  to_next_location.y = 0;
  float dist_next_location = length(to_next_location);

  if (dist_next_location < 1.0f) {
    int dice = rand() % 1000; 
    if (dice < 50) {
      ChangeAction(spider, IDLE);
      spider->next_location = vec3(0, 0, 0);
      return;
    } else if (dice < 100) {
      ChangeAction(spider, MOVE);
      spider->next_location = vec3(0, 0, 0);
      return;
    }
  } 

  float current_time = glfwGetTime();
  if (dist_next_location < 1.0f ||
      dist_next_location > 100.0f ||
      current_time - spider->time_wandering > 100.0f
  ) {
    if (!spider->next_waypoint) {
      spider->next_waypoint = GetNextWaypoint(spider);
    }

    vec3 to_waypoint = spider->next_waypoint->position - spider->position;
    to_waypoint.y = 0;
    if (length(to_waypoint) > 100.0f) {
      spider->next_location = spider->next_waypoint->position;
    } else {
      float angle = radians(-45.0f + rand() % 90);
      vec3 dir = vec3(rotate(mat4(1.0f), angle, vec3(0, 1, 0)) * 
        vec4(spider->forward, 1.0f));  
      spider->next_location = spider->position + dir * 25.0f; 
    }
    spider->time_wandering = glfwGetTime();
  }

  float speed = 0.02;
  bool is_rotating = RotateSpider(spider, spider->next_location);
  if (!is_rotating) {
    spider->speed += spider->forward * speed;
  }
}

// TODO: Split AI into actions and intention. If the intention is to hunt, the
// spider may need to take many actions. For example: to wander, a spider
// needs to select a new location, move there, check if it is close to it,
// take another action, check if the player is nearby. Actions also must take 
// the object status into consideration.
void AI::RunSpiderAI() {
  int num_spiders = 0;
  for (ObjPtr obj1 : asset_catalog_->GetMovingObjects()) {
    if (obj1->GetAsset()->name != "spider") continue;
    num_spiders++;

    ObjPtr spider = obj1;

    // TODO: maybe should go to physics.
    if (dot(spider->up, vec3(0, 1, 0)) < 0) {
      spider->up = vec3(0, 1, 0);
    }

    // Check status. If taking hit, or dying.
    switch (spider->status) {
      case STATUS_TAKING_HIT: {
        spider->active_animation = "Armature|hit";
        if (spider->frame >= 39) {
          spider->status = STATUS_NONE;
        }
        continue;
      }
      case STATUS_DYING: {
        spider->active_animation = "Armature|death";
        if (spider->frame >= 78) {
          asset_catalog_->CreateParticleEffect(64, spider->position, 
            vec3(0, 2.0f, 0), vec3(0.6, 0.2, 0.8), 5.0, 60.0f, 10.0f);
          spider->status = STATUS_DEAD;
        }
        continue;
      }
      case STATUS_NONE:
      case STATUS_DEAD:
      default:
        break;
    }

    // Currently, spider action is just to move to next waypoint. 
    // TODO: actions should be
    //   - Patrol (current)
    //   - Hide
    //   - Attack
    //   - Idle (regenerate life)
    //   - Cast web (in the future)
    switch (obj1->ai_action) {
      case MOVE:
        Move(obj1);
        break;
      case TURN_TOWARD_TARGET:
        TurnTowardPlayer(obj1);
        break;
      case ATTACK:
        Attack(obj1);
        break;
      case WANDER:
        Wander(obj1);
        break;
      case IDLE:
        Idle(obj1);
        break;
      default:
        break;
    }
  }

  // TODO: this should go somewhere else.
  if (num_spiders < 10) {
    int dice = rand() % 1000; 
    if (dice > 2) return;
    cout << "Creating spider" << endl;

    dice = rand() % 1000; 
    vec3 position = vec3(9649, 134, 10230);
    if (dice < 500) {
      position = vec3(9610, 132, 9885);
    } 

    shared_ptr<GameObject> obj = asset_catalog_->CreateGameObjFromAsset(
      "spider", position);
    obj->position = position;
    obj->ai_action = WANDER;
  }
}
