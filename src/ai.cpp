#include "ai.hpp"

AI::AI(shared_ptr<AssetCatalog> asset_catalog) : asset_catalog_(asset_catalog) {
}

void AI::InitSpider() {
  // TODO: spider should be created in AI.
  spider_next_waypoint_ = asset_catalog_->GetWaypointByName("waypoint-001");
  shared_ptr<GameObject> spider = asset_catalog_->GetObjectByName("spider-001");
  spider->ai_action = MOVE;
}

void AI::RunSpiderAI(const Camera& camera) {
  shared_ptr<GameObject> spider = asset_catalog_->GetObjectByName("spider-001");

  vec3 direction = spider_next_waypoint_->position - spider->position;
  direction.y = 0;
  if (spider->life <= 0) {
    spider->active_animation = "Armature|death";
    if (spider->frame == 78) {
      spider->draw = false;
    }
    return;
  }

  if (length(direction) < 1.0f) {
    if (spider_next_waypoint_->next_waypoints.empty()) {
      throw runtime_error("No next waypoint");      
    }

    spider_next_waypoint_ = spider_next_waypoint_->next_waypoints[0];
    spider->ai_action = ATTACK;
    spider->frame = 0;
  }

  float turn_rate = 0.04f;
  // float speed = 0.11f;
  float speed = 0.02;

  direction = normalize(direction);

  float target_angle;
  if (spider->ai_action == MOVE) {
    // if (rand() % 1000 < 2) {
    //   spider->ai_action = IDLE;
    //   spider->active_animation = "Armature|idle";
    //   spider->frame = 0;
    // }

    target_angle = atan2(1, 0) - atan2(direction.z, direction.x);
  } else if (spider->ai_action == ATTACK) {
    vec3 player_direction = camera.position - spider->position;
    player_direction.y = 0;
    player_direction = normalize(player_direction);
    target_angle = atan2(1, 0) - atan2(player_direction.z, player_direction.x);
  } else if (spider->ai_action == IDLE) {
    if (rand() % 1000 < 2) {
      spider->ai_action = MOVE;
      spider->active_animation = "Armature|walking";
      spider->frame = 0;
    }
  }

  if (target_angle < 0.0f) target_angle = 6.28f + target_angle;

  bool create_particle = false;

  float angle_diff = abs(target_angle - float(spider->rotation.y));
  if (spider->active_animation == "Armature|walking") {
    if (angle_diff > turn_rate) {
      float clockwise_angle = spider->rotation.y + turn_rate;
      if (clockwise_angle < 0.0f) clockwise_angle = 6.28f + clockwise_angle;

      float counter_clockwise_angle = spider->rotation.y - turn_rate;
      if (counter_clockwise_angle < 0.0f) counter_clockwise_angle = 6.28f + counter_clockwise_angle;

      float min_clockwise_angle = std::min(abs(target_angle - clockwise_angle - 6.28f), abs(target_angle - clockwise_angle));
      float min_counter_clockwise_angle = std::min(abs(target_angle - counter_clockwise_angle - 6.28f), abs(target_angle - counter_clockwise_angle));

      if (min_clockwise_angle < min_counter_clockwise_angle) {
        spider->rotation.y += turn_rate;
        if (spider->rotation.y > 6.28f) spider->rotation.y = spider->rotation.y - 6.28f;
      } else {
        spider->rotation.y -= turn_rate;
        if (spider->rotation.y < 0.0f) spider->rotation.y = 6.28f + spider->rotation.y;
      }
    } else {
      if (spider->ai_action == MOVE) {
        // spider->position += direction * speed;
        spider->speed += direction * speed;
        spider->rotation.y = target_angle;
      } else if (spider->ai_action == ATTACK) {
        spider->active_animation = "Armature|attack";
        spider->rotation.y = target_angle;
      }
    }
  } else if (spider->active_animation == "Armature|hit") {
    if (spider->frame == 39) {
      spider->active_animation = "Armature|walking";
      spider->frame = 0;
    }
  } else if (spider->active_animation == "Armature|attack") {
    if (spider->frame == 44) {
      create_particle = true;
    }

    if (spider->frame == 59) {
      spider->active_animation = "Armature|walking";
      spider->frame = 0;
      spider->ai_action = MOVE;
    }
  }

  // Create poison spit.
  if (create_particle) {
    vec3 dir = normalize(vec3(spider->rotation_matrix * vec4(0, 0, 1, 1)));
    asset_catalog_->CreateParticleEffect(40, spider->position + dir * 12.0f + vec3(0, 2.0, 0), 
      dir * 5.0f, vec3(0.0, 1.0, 0.0), -1.0, 40.0f, 3.0f);

    // TODO: create missiles in asset_catalog.
    // int first_unused_index = 0;
    // for (int i = 0; i < 10; i++) {
    //   if (magic_missiles_[i].life <= 0) {
    //     first_unused_index = i;
    //     break;
    //   }
    // }

    // MagicMissile& mm = magic_missiles_[first_unused_index];
    // mm.frame = 0;
    // mm.life = 30000;

    // vec3 right = normalize(cross(camera.up, camera.direction));
    // mm.position = spider->position;
    // mm.direction = normalize(camera.position - spider->position);
    // mm.rotation_matrix = mat4(1.0);
    // mm.objects[0]->rotation_matrix = rotation_matrix;
    // mm.owner = spider;
  }

  asset_catalog_->UpdateObjectPosition(spider);
}

// TODO: should be moved to a Physics module.
void AI::UpdateSpiderForces() {
  shared_ptr<GameObject> spider = asset_catalog_->GetObjectByName("spider-001");

  spider->speed += glm::vec3(0, -0.016, 0);

  // Friction.
  spider->speed.x *= 0.9;
  spider->speed.y *= 0.99;
  spider->speed.z *= 0.9;

  vec3 prev_pos = spider->position;
  spider->position += spider->speed;
  asset_catalog_->UpdateObjectPosition(spider);
}
