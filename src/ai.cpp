#include "ai.hpp"

AI::AI(shared_ptr<AssetCatalog> asset_catalog) : asset_catalog_(asset_catalog) {
}

void AI::InitSpider() {
  // TODO: spider should be created in AI.
  shared_ptr<GameObject> spider = asset_catalog_->GetObjectByName("spider-001");
  spider->next_waypoint = asset_catalog_->GetWaypointByName("waypoint-001");
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

void AI::RunSpiderAI() {
  for (ObjPtr obj1 : asset_catalog_->GetMovingObjects()) {
    if (obj1->GetAsset()->name != "spider") continue;

    // shared_ptr<GameObject> spider = asset_catalog_->GetObjectByName("spider-001");
    shared_ptr<GameObject> spider = obj1;

    float turn_rate = 0.01;
    float speed = 0.03;

    if (!spider->next_waypoint) {
      spider->next_waypoint = GetNextWaypoint(spider);
      cout << "WHAT: " << spider->next_waypoint->position << endl;
    }

    cout << spider->name << endl;
    // Currently, spider action is just to move to next waypoint. 
    // TODO: actions should be
    //   - Patrol (current)
    //   - Hide
    //   - Attack
    //   - Idle (regenerate life)
    //   - Cast web (in the future)
    vec3 to_next_waypoint = spider->next_waypoint->position - spider->position;
    cout << to_next_waypoint << endl;

    to_next_waypoint.y = 0;
    if (length(to_next_waypoint) < 1.0f) {
      if (spider->next_waypoint->next_waypoints.empty()) {
        throw runtime_error("No next waypoint");      
      }
      spider->next_waypoint = spider->next_waypoint->next_waypoints[0];
    }

    if (dot(spider->up, vec3(0, 1, 0)) < 0) {
      spider->up = vec3(0, 1, 0);
    }

    // At rest position, the object forward direction is +Z and up is +Y.
    spider->forward = normalize(to_next_waypoint);
    quat h_rotation = RotationBetweenVectors(vec3(0, 0, 1), spider->forward);
    quat v_rotation = RotationBetweenVectors(vec3(0, 1, 0), spider->up);
    quat target_rotation = v_rotation * h_rotation;
    cout << spider->name << " --> " << spider->up << endl;

    // Check if object is facing the correct direction.
    vec3 cur_forward = spider->rotation_matrix * vec3(0, 0, 1);
    cur_forward.y = 0;
    quat cur_h_rotation = RotationBetweenVectors(vec3(0, 0, 1), 
      normalize(cur_forward));
    bool is_rotating = abs(dot(h_rotation, cur_h_rotation)) < 0.75f;

    if (dot(spider->cur_rotation, target_rotation) < 0.999f) {
      spider->cur_rotation =  
        RotateTowards(spider->cur_rotation, target_rotation, turn_rate);
    }
    spider->rotation_matrix = mat4_cast(spider->cur_rotation);

    if (spider->ai_action == MOVE) {
      // TODO: check if spider is taking hit before attempting to move.
      // There should be a status field. Taking hit should be a status.
      // Dying should be a status.
      // if (spider->active_animation == "Armature|hit") {
      //   if (spider->frame == 39) {
      //     spider->active_animation = "Armature|walking";
      //     spider->frame = 0;
      //   }
      // }

      spider->active_animation = "Armature|walking";
      if (!is_rotating) {
        spider->speed += spider->forward * speed;
      }

      // TODO: check death animation.
      // spider->active_animation = "Armature|death";
    }

    // Create poison spit.
    // if (spider->ai_action == ATTACK) {
    //   vec3 dir = normalize(vec3(spider->rotation_matrix * vec4(0, 0, 1, 1)));
    //   asset_catalog_->CreateParticleEffect(40, spider->position + dir * 12.0f + vec3(0, 2.0, 0), 
    //     dir * 5.0f, vec3(0.0, 1.0, 0.0), -1.0, 40.0f, 3.0f);

    //   // TODO: create missiles in asset_catalog.
    //   // int first_unused_index = 0;
    //   // for (int i = 0; i < 10; i++) {
    //   //   if (magic_missiles_[i].life <= 0) {
    //   //     first_unused_index = i;
    //   //     break;
    //   //   }
    //   // }

    //   // MagicMissile& mm = magic_missiles_[first_unused_index];
    //   // mm.frame = 0;
    //   // mm.life = 30000;

    //   // vec3 right = normalize(cross(camera.up, camera.direction));
    //   // mm.position = spider->position;
    //   // mm.direction = normalize(camera.position - spider->position);
    //   // mm.rotation_matrix = mat4(1.0);
    //   // mm.objects[0]->rotation_matrix = rotation_matrix;
    //   // mm.owner = spider;
    // }
  }
}
