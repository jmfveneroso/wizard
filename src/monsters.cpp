#include "monsters.hpp"
#include <queue>
#include <iomanip>

Monsters::Monsters(shared_ptr<Resources> resources) : resources_(resources) {}

ObjPtr Monsters::GetTarget(ObjPtr unit) {
  ObjPtr decoy = resources_->GetDecoy();
  if (decoy) {
    float decoy_distance = length2(unit->position - decoy->position);
    if (decoy_distance < 10000) return decoy;
  }
  return resources_->GetPlayer();
}

float GetDistance(vec3 pos1, vec3 pos2) {
  pos1.y = 0;
  pos2.y = 0;
  return length(pos1 - pos2);
}

bool Monsters::IsAttackAction(shared_ptr<Action> action) {
  if (!action) return false;
  switch (action->type) {
    case ACTION_MEELEE_ATTACK:
    case ACTION_RANGED_ATTACK:
    case ACTION_TAKE_AIM:
    case ACTION_SWEEP_ATTACK:
      return true;
    default: 
      break;
  }
  return false;
}

bool Monsters::ShouldHoldback(ObjPtr unit) {
  if (!IsPlayerReachable(unit)) { 
    return true;
  }

  Dungeon& dungeon = resources_->GetDungeon();
  ObjPtr player = resources_->GetPlayer();
  double distance_to_player = GetDistance(player->position, unit->position);

  bool group_is_attacking = false;
  vector<ObjPtr> monsters = resources_->GetMonstersInGroup(unit->monster_group); 
  for (auto& m : monsters) {
    if (m == unit) continue;
    if (m->ai_state == AI_ATTACK) {
    // if (m->ai_state == AI_ATTACK && m->leader) {
      group_is_attacking = true;
      break;
    }
  }

  int unit_relevance = 
    dungeon.GetRelevance(dungeon.GetDungeonTile(unit->position));

  int player_relevance = 
    dungeon.GetRelevance(dungeon.GetDungeonTile(player->position));

  float chase_distance = 50.0f;
  int relevance_diff = 3;

  if (unit->ai_state == AMBUSH) {
    chase_distance = 20.0f;
    relevance_diff = 1;
  } 

  // if (unit->leader) {
  //   group_is_attacking = false;
  // }

  int actual_relevance_diff = (player_relevance - unit_relevance);
  return !group_is_attacking && (distance_to_player > chase_distance) &&
    (actual_relevance_diff < relevance_diff);
}

bool Monsters::CanDetectPlayer(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ObjPtr player = resources_->GetPlayer();
  bool visible = dungeon.IsTileVisible(unit->position);

  if (unit->GetAsset()->name == "white_spine") {
    double distance_to_player = length(player->position - unit->position);
    return (visible || distance_to_player < 100);
  }

  return visible;
}

void Monsters::MiniSpiderling(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ObjPtr player = resources_->GetPlayer();
  double distance_to_player = length(player->position - unit->position);

  bool group_is_attacking = false;
  vector<ObjPtr> monsters = resources_->GetMonstersInGroup(unit->monster_group); 
  for (auto& m : monsters) {
    if (m == unit) continue;
    if (m->ai_state == AI_ATTACK) {
      group_is_attacking = true;
      break;
    }
  }

  switch (unit->ai_state) {
    case AI_ATTACK: {
      shared_ptr<Action> next_action = nullptr;
      if (!unit->actions.empty()) {
        next_action = unit->actions.front();
      }

      if (unit->actions.empty()) {
        if (distance_to_player < 11.0f) {
          unit->actions.push(make_shared<TakeAimAction>());
          unit->actions.push(make_shared<MeeleeAttackAction>());
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else if (distance_to_player < 11.0f && !IsAttackAction(next_action)) {
        unit->ClearActions();
        unit->actions.push(make_shared<TakeAimAction>());
        unit->actions.push(make_shared<MeeleeAttackAction>());
      } else if (next_action->type == ACTION_MOVE) {
        shared_ptr<MoveAction> move_action =  
          static_pointer_cast<MoveAction>(next_action);
        vec3 next_move = move_action->destination;
        double distance = length(next_move - player->position);
        if (distance > distance_to_player) {
          unit->ClearActions();
        }
      }
      break;
    } 
    case START: {
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

bool Monsters::IsPlayerReachable(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();

  ObjPtr target;
  if (unit->current_target) {
    target = unit->current_target;
  } else {
    target = resources_->GetPlayer();
  }

  bool reachable = dungeon.IsReachable(unit->position, target->position);
  // cout << "reachable: " << reachable << endl;
  return reachable;
}

ivec2 Monsters::IsPlayerReachableThroughDoor(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  return dungeon.IsReachableThroughDoor(unit->position, player->position);
}

ivec2 Monsters::FindAmbushTile(ObjPtr unit) {
  shared_ptr<Player> player = resources_->GetPlayer();

  Dungeon& dungeon = resources_->GetDungeon();
  ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);
  ivec2 player_tile_pos = dungeon.GetDungeonTile(player->position);

  vec3 dir3d = vec3(rotate(
    mat4(1.0),
    player->rotation.y,
    vec3(0.0f, 1.0f, 0.0f)
  ) * vec4(0, 0, 1, 1));
  vec2 dir = vec2(dir3d.x, dir3d.z);

  int room1 = dungeon.GetRoom(dungeon.GetDungeonTile(player->position));

  for (int k = 6; k < 15; k++) {
    vector<ivec2> possible_tiles;
    for (int i = -1; i < 1; i++) {
      for (int j = -1; j < 1; j++) {
        if (i == 0 && j == 0) continue;
        ivec2 new_tile = unit_tile_pos + ivec2(i*k, j*k);
       
        if (!dungeon.IsTileClear(new_tile)) continue;

        vec2 tile_dir = normalize(vec2(new_tile) - vec2(player_tile_pos));
        if (dot(dir, tile_dir) > 0.0) continue;
 
        double distance_to_player = length(player->position - dungeon.GetTilePosition(new_tile));
        if (distance_to_player < 40.0f) continue;

        int room2 = dungeon.GetRoom(new_tile);
        if (room2 != room1) continue;

        possible_tiles.push_back(new_tile);
      }
    }

    if (possible_tiles.empty()) continue;

    int index = Random(0, possible_tiles.size());
    return possible_tiles[index];
  }
  return ivec2(-1);
}

ivec2 Monsters::FindSafeTile(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);

  for (int k = 0; k < 10; k++) {
    vector<ivec2> possible_tiles;
    for (int i = -1; i < 1; i++) {
      for (int j = -1; j < 1; j++) {
        if (i == 0 && j == 0) continue;
        ivec2 new_tile = unit_tile_pos + ivec2(i*k, j*k);
       
        if (!dungeon.IsTileClear(new_tile)) continue;
        if (dungeon.IsTileVisible(new_tile)) continue;
        possible_tiles.push_back(new_tile);
      }
    }

    if (possible_tiles.empty()) continue;

    int index = Random(0, possible_tiles.size());
    return possible_tiles[index];
  }
  return ivec2(-1);
}

ivec2 Monsters::FindChokepoint(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);

  for (int k = 0; k < 10; k++) {
    vector<ivec2> possible_tiles;
    for (int i = -1; i < 1; i++) {
      for (int j = -1; j < 1; j++) {
        if (i == 0 && j == 0) continue;
        ivec2 new_tile = unit_tile_pos + ivec2(i*k, j*k);
       
        if (!dungeon.IsTileClear(new_tile)) continue;
        switch (dungeon.AsciiCode(new_tile.x, new_tile.y)) {
          case 'd':
          case 'D': {
            break;
          }
          default: {
            continue;
          }
        }
        possible_tiles.push_back(new_tile);
      }
    }

    if (possible_tiles.empty()) continue;

    int index = Random(0, possible_tiles.size());
    return possible_tiles[index];
  }
  return ivec2(-1);
}

ivec2 Monsters::FindRandomTile(ObjPtr unit, int max_k) {
  Dungeon& dungeon = resources_->GetDungeon();
  ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);

  for (int k = 0; k < max_k; k++) {
    vector<ivec2> possible_tiles;
    for (int i = -1; i < 1; i++) {
      for (int j = -1; j < 1; j++) {
        if (i == 0 && j == 0) continue;
        ivec2 new_tile = unit_tile_pos + ivec2(i*k, j*k);
       
        if (!dungeon.IsTileClear(new_tile)) continue;
        possible_tiles.push_back(new_tile);
      }
    }

    if (possible_tiles.empty()) continue;

    int index = Random(0, possible_tiles.size());
    return possible_tiles[index];
  }
  return ivec2(-1);
}

ivec2 Monsters::FindFleeTile(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();

  ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);

  ObjPtr player = resources_->GetPlayer();

  ivec2 tile = ivec2(-1);
  float max_dist_from_player = 0;
  int k = 5;

  for (int i = 0; i < 20; i++) {
    ivec2 new_tile = unit_tile_pos + ivec2(Random(-k, k+1), Random(-k, k+1));
    if (new_tile.x == unit_tile_pos.x && new_tile.y == unit_tile_pos.y) {
      continue;
    }

    if (!dungeon.IsValidTile(new_tile)) continue;
    if (!dungeon.IsTileClear(new_tile)) continue;
    if (!dungeon.IsReachable(unit->position, dungeon.GetTilePosition(new_tile))) continue;
    if (dungeon.AsciiCode(new_tile.x, new_tile.y) != ' ') continue;

    double dist = length(dungeon.GetTilePosition(new_tile) - player->position);
    if (dist <= max_dist_from_player) continue;

    max_dist_from_player = dist;
    tile = new_tile;
  }
  return tile;
}

vec3 Monsters::FindCloseFlee(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  vec3 center = player->position;

  ivec2 player_tile_pos = dungeon.GetDungeonTile(player->position);
  ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);

  ivec2 step;
  step.x = sign(player_tile_pos.x - unit_tile_pos.x);
  step.y = sign(player_tile_pos.y - unit_tile_pos.y);

  ivec2 next_tile = unit_tile_pos - step;
  if (!dungeon.IsValidTile(next_tile) ||
      !dungeon.IsTileClear(next_tile)) {
    return vec3(0);
  }

  return dungeon.GetTilePosition(next_tile);
}

vec3 Monsters::FindSideMove(ObjPtr unit, bool memorize) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  vec3 center = player->position;

  ivec2 player_tile_pos = dungeon.GetDungeonTile(player->position);
  ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);

  ivec2 step;
  step.x = sign(player_tile_pos.x - unit_tile_pos.x);
  step.y = sign(player_tile_pos.y - unit_tile_pos.y);

  ivec2 next_tile;
  string move = "";

  if (memorize) {
    move = unit->ReadMemory("side_move");
  }

  if (move == "") {
    if (Random(0, 5) == 0) {
      if (memorize) unit->SetMemory("side_move", "right");
      next_tile = ivec2(step.y, -step.x);
    } else {
      if (memorize) unit->SetMemory("side_move", "left");
      next_tile = ivec2(-step.y, step.x);
    }
  } else if (move == "right") {
    next_tile = ivec2(step.y, -step.x);
  } else {
    next_tile = ivec2(-step.y, step.x);
  }

  if (!dungeon.IsValidTile(unit_tile_pos + next_tile) ||
      !dungeon.IsTileClear(unit_tile_pos + next_tile) ||
      dungeon.IsTileBorder(unit_tile_pos + next_tile)) {
    next_tile = -next_tile;
  }

  if (!dungeon.IsValidTile(unit_tile_pos + next_tile) ||
      !dungeon.IsTileClear(unit_tile_pos + next_tile) ||
      dungeon.IsTileBorder(unit_tile_pos + next_tile)) {
    return vec3(0);
  }

  return dungeon.GetTilePosition(unit_tile_pos + next_tile);

  // bool clockwise = (unit->id % 2 == 0);
  // float angle = 0.2f * ((clockwise) ? 1 : -1);
  // float c = cos(angle);
  // float s = sin(angle);
  // mat2 m = mat2(c, -s, s, c);

  // const vec3 u_pos = unit->position;
  // const vec3 p_pos = center;
  // vec2 v = vec2(u_pos.x, u_pos.z) - vec2(p_pos.x, p_pos.z);
  // v = m * v;
  // vec3 pos = player->position + vec3(v.x, 0, v.y);

  // if (dungeon.IsReachable(unit->position, pos)) return pos;
  // return vec3(0);
}

ivec2 Monsters::FindClosestPassage(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);

  for (int k = 0; k < 5; k++) {
    for (int i = -1; i < 1; i++) {
      for (int j = -1; j < 1; j++) {
        if (i == 0 && j == 0) continue;
        ivec2 new_tile = unit_tile_pos + ivec2(i*k, j*k);
        if (!dungeon.IsValidTile(new_tile)) continue;
  
        switch (dungeon.AsciiCode(new_tile.x, new_tile.y)) {
          case 'o':
          case 'O':
          case 'd':
          case 'D':
            return new_tile;
          default:
            break;
        }
      }
    }
  }
  return ivec2(-1);
}

// ivec2 Monsters::FindTileInPlayerPath(ObjPtr unit) {
//   Dungeon& dungeon = resources_->GetDungeon();
//   ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);
//   ivec2 unit_destination = dungeon.GetDungeonTile(unit->position);
// 
//   vector<ivec2> tiles = dungeon.GetPath(player->position, );
// 
//   for (int k = 0; k < 10; k++) {
//     vector<ivec2> possible_tiles;
//     for (int i = -1; i < 1; i++) {
//       for (int j = -1; j < 1; j++) {
//         if (i == j && i == 0) continue;
//         ivec2 new_tile = unit_tile_pos + ivec2(i*k, j*k);
//        
//         if (!dungeon.IsTileClear(new_tile)) continue;
//         if (dungeon.IsTileVisible(new_tile)) continue;
//         possible_tiles.push_back(new_tile);
//       }
//     }
// 
//     if (possible_tiles.empty()) continue;
// 
//     int index = Random(0, possible_tiles.size());
//     return possible_tiles[index];
//   }
//   return ivec2(-1);
// }

void Monsters::Spiderling(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();

  ObjPtr target = unit->GetCurrentTarget();
  double distance_to_player = length(target->position - unit->position);

  bool visible = dungeon.IsTileVisible(unit->position);

  // TODO: change to function.
  bool player_reachable = IsPlayerReachable(unit);

  int unit_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(unit->position));

  // TODO: change to function.
  int player_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(player->position));

  const float kChaseDistance = 50.0f;

  bool holdback = ShouldHoldback(unit);

  if (unit->was_hit) {
    unit->levitating = false;
    unit->was_hit = false;
    unit->ClearTemporaryStatus(STATUS_SPIDER_THREAD);
    unit->ClearActions();
    unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
  }

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!player_reachable) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(FLEE));
        break;
      }

      shared_ptr<Action> next_action = nullptr;
      if (!unit->actions.empty()) {
        next_action = unit->actions.front();
      }

      if (next_action && next_action->type == ACTION_SPIDER_JUMP) break;
      if (!unit->can_jump) {
        break;
      }

      float t;
      bool movement_obstructed = dungeon.IsMovementObstructed(unit->position, target->position, t);
      if (!movement_obstructed && distance_to_player > 30.0f && unit->can_jump) {
        if (unit->CanUseAbility("spider-jump")) {
          unit->cooldowns["spider-jump"] = glfwGetTime() + 1;
          unit->actions.push(make_shared<SpiderJumpAction>(target->position));
          break;
        }
      }

      int r = Random(0, 100);
      if (unit->actions.empty()) {
        if (distance_to_player < 11.0f) {
          unit->actions.push(make_shared<TakeAimAction>());
          unit->actions.push(make_shared<MeeleeAttackAction>());
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else if (distance_to_player < 11.0f && !IsAttackAction(next_action)) {
        unit->ClearActions();
        unit->actions.push(make_shared<TakeAimAction>());
        unit->actions.push(make_shared<MeeleeAttackAction>());
      } else if (next_action->type == ACTION_MOVE) {
        shared_ptr<MoveAction> move_action =  
          static_pointer_cast<MoveAction>(next_action);
        vec3 next_move = move_action->destination;
        double distance = length(next_move - target->position);
        if (distance > distance_to_player && !movement_obstructed) {
          unit->ClearActions();
        }
      }
      break;
    } 
    case AMBUSH: {
      if (!holdback) {
        unit->levitating = false;
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        resources_->ChangeObjectAnimation(unit, "Armature|walking");
        break;
      }

      shared_ptr<Action> next_action = nullptr;
      if (!unit->actions.empty()) {
        next_action = unit->actions.front();
      }

      if (next_action && next_action->type == ACTION_SPIDER_CLIMB) break;

      unit->AddTemporaryStatus(make_shared<SpiderThreadStatus>(0.1f, 20.0f));

      if (!unit->actions.empty()) {
        break;
      }

      unit->levitating = true;
      resources_->ChangeObjectAnimation(unit, "Armature|climbing");
      unit->actions.push(make_shared<TakeAimAction>());
      break;
    }
    case DEFEND: {
      if (!holdback) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        break;
      }
     
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<TakeAimAction>());

      vec3 target;
      vector<ivec2> path = dungeon.GetPath(player->position, unit->position);

      if (path.size() < 3 || !dungeon.IsTileVisible(unit->position)) {
        ivec2 safe_tile = FindSafeTile(unit);
        if (safe_tile.x == -1) {
          unit->ClearActions();
          unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
          break;
        }

        unit->ClearActions();
        unit->actions.push(make_shared<LongMoveAction>(
          dungeon.GetTilePosition(safe_tile)));
        break;
      } else {
        target = dungeon.GetTilePosition(path[Random(1, path.size() - 2)]);
      }
      target += vec3(Random(-5, 5), 0, Random(-5, 5));

      unit->actions.push(make_shared<SpiderWebAction>(target));
      break;
    }
    case HIDE: {
      if (!unit->actions.empty()) {
        break;
      }

      if (!holdback || visible) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        break;
      }

      ivec2 safe_tile = FindSafeTile(unit);
      if (safe_tile.x == -1) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        break;
      }

      unit->actions.push(make_shared<LongMoveAction>(
        dungeon.GetTilePosition(safe_tile)));
      break;
    }
    case FLEE: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;

      if (distance_to_player > 100.0f) {
        unit->ClearActions();
        unit->actions.push(make_shared<SpiderClimbAction>(Random(25, 41)));
        unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
        break;
      }

      if (!holdback || player_reachable) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        break;
      }

      ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);
      ivec2 safe_tile = FindFleeTile(unit);
      if (safe_tile.x == -1) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
        break;
      }

      unit->actions.push(make_shared<LongMoveAction>(
        dungeon.GetTilePosition(safe_tile)));
      break;
    }
    case ACTIVE: {
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      // unit->actions.push(make_shared<ChangeStateAction>(FLEE));
      break;

      if (holdback) {
        unit->ClearActions();

        // int r = Random(0, 100);
        // if (r <= 50) {
        //   unit->actions.push(make_shared<ChangeStateAction>(DEFEND));
        // } else {
        //   unit->actions.push(make_shared<ChangeStateAction>(HIDE));
        // }
        unit->actions.push(make_shared<ChangeStateAction>(HIDE));
        break;
      }

      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case IDLE: {
      int room1 = dungeon.GetRoom(dungeon.GetDungeonTile(unit->position));
      int room2 = dungeon.GetRoom(dungeon.GetDungeonTile(player->position));

      if (visible || room1 == room2) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(ACTIVE));
      } else if (unit->actions.empty()) {
        // unit->actions.push(make_shared<IdleAction>(1));
        unit->actions.push(make_shared<RandomMoveAction>());
      }
      break;
    }
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case START: {
      if (!unit->actions.empty()) {
        break;
      }

      if (Random(0, 10) < 5) {
        unit->actions.push(make_shared<SpiderClimbAction>(Random(25, 41)));
        unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
      } else {
        unit->actions.push(make_shared<ChangeStateAction>(IDLE));
      }
      break;
    }
    default: {
      break;
    }
  }
}

ivec2 Monsters::GetFrogJumpTile(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ivec2 origin = dungeon.GetDungeonTile(unit->position);

  int padding = 4;
  bool move_to_center = origin.x < padding || origin.y < padding ||
                        origin.x >= 14 - padding || origin.y >= 14 - padding;
  if (move_to_center) { 
    origin = ivec2(7, 7);
  }    

  int max_k = 5;
  vector<ivec2> possible_tiles;
  for (int k = 0; k < max_k; k++) {
    for (int i = -1; i < 1; i++) {
      for (int j = -1; j < 1; j++) {
        if (i == 0 && j == 0) continue;
        ivec2 new_tile = origin + ivec2(i*k, j*k);
       
        if (!dungeon.IsTileClear(new_tile)) continue;

        bool is_border = new_tile.x < padding || 
                         new_tile.y < padding ||
                         new_tile.x >= 14 - padding || 
                         new_tile.y >= 14 - padding;
        if (is_border) {
          continue;
        }

        possible_tiles.push_back(new_tile);
      }
    }
  }

  if (possible_tiles.empty()) {
    return ivec2(7, 7);
  }

  int index = Random(0, possible_tiles.size());
  return possible_tiles[index];
}

ivec2 Monsters::GetBroodmotherMoveTile(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ivec2 origin = dungeon.GetDungeonTile(unit->position);
  shared_ptr<Player> player = resources_->GetPlayer();
  ivec2 player_tile = dungeon.GetDungeonTile(player->position);

  int padding = 4;
  bool move_to_center = origin.x < padding || origin.y < padding ||
                        origin.x >= 14 - padding || origin.y >= 14 - padding;
  if (move_to_center) { 
    origin = ivec2(7, 7);
  }    

  ivec2 tile = ivec2(7, 7);
  for (int tries = 0; tries < 10; tries++) {
    int x = Random(1, 3);
    int y = Random(1, 3);
    ivec2 new_tile = origin + ivec2(x, y);
    if (!dungeon.IsValidTile(new_tile)) continue;
    if (length(vec2(player_tile) - vec2(new_tile)) < 5.0f) continue;
    tile = new_tile;
  }

  return tile;
}

void Monsters::Frog(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();

  ObjPtr target = unit->GetCurrentTarget();
  double distance_to_player = length(target->position - unit->position);

  if (unit->was_hit) {
    unit->was_hit = false;
    unit->ClearActions();
    unit->actions.push(make_shared<ChangeStateAction>(FLEE));
    unit->cooldowns["frog-jump"] = glfwGetTime() + 2;
  }

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      if (distance_to_player > 50.0f) {
        float off_x = Random(-10, 10);
        float off_y = Random(-10, 10);
        unit->actions.push(make_shared<FrogShortJumpAction>(
          target->position + vec3(off_x, 0, off_y)));
        break;
      }

      unit->actions.push(make_shared<FrogJumpAction>(target->position));
      unit->actions.push(make_shared<ChangeStateAction>(FLEE));
      if (unit->GetAsset()->name == "red_frog") {
        unit->cooldowns["frog-jump"] = glfwGetTime() + 3;
      } else {
        unit->cooldowns["frog-jump"] = glfwGetTime() + 5;
      }
      break;
    } 
    case FLEE: {
      if (!unit->actions.empty()) {
        break;
      }

      ivec2 target_tile = GetFrogJumpTile(unit);
      vec3 target_pos = dungeon.GetTilePosition(target_tile);
      unit->actions.push(make_shared<FrogShortJumpAction>(target_pos));

      if (unit->CanUseAbility("frog-jump")) {
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      }
      break;
    } 
    case ACTIVE: {
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case START: {
      if (!unit->actions.empty()) {
        break;
      }

      if (Random(0, 10) < 5) {
        unit->actions.push(make_shared<SpiderClimbAction>(Random(25, 41)));
        unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
      } else {
        unit->actions.push(make_shared<ChangeStateAction>(IDLE));
      }
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::RedFrog(ObjPtr unit) {
  return Frog(unit);
}


void Monsters::Goblin(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();

  ObjPtr target = unit->GetCurrentTarget();
  vec3 v_to_player = target->position - unit->position;
  double distance_to_player = length(v_to_player);

  bool visible = dungeon.IsTileVisible(unit->position);

  // TODO: change to function.
  bool player_reachable = IsPlayerReachable(unit);

  int unit_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(unit->position));

  // TODO: change to function.
  int player_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(player->position));

  const float kChaseDistance = 50.0f;
  bool holdback = ShouldHoldback(unit);

  if (unit->saw_player_attack && unit->CanUseAbility("defend")) {
    unit->saw_player_attack = false;

    vec3 dir3d = vec3(rotate(
      mat4(1.0),
      player->rotation.y,
      vec3(0.0f, 1.0f, 0.0f)
    ) * vec4(0, 0, 1, 1));
    vec2 dir = normalize(vec2(dir3d.x, dir3d.z));
    bool monster_is_targeted = (dot(dir, 
      normalize(-vec2(v_to_player.x, v_to_player.z))) > 0.98f);
    
    if (monster_is_targeted) {
      if (unit->was_hit) {
        unit->was_hit = false;
      } else {
        unit->ClearActions();
        unit->ai_state = DEFEND;
        unit->state_changed_at = glfwGetTime();
      }
    }
  }

  switch (unit->ai_state) {
    case AI_ATTACK: {
      shared_ptr<Action> next_action = nullptr;
      if (!unit->actions.empty()) {
        next_action = unit->actions.front();
      }

      int r = Random(0, 100);
      if (unit->actions.empty()) {
        if (distance_to_player < 11.0f) {
          unit->actions.push(make_shared<TakeAimAction>());
          unit->actions.push(make_shared<MeeleeAttackAction>());
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else if (distance_to_player < 11.0f && !IsAttackAction(next_action)) {
        unit->ClearActions();
        unit->actions.push(make_shared<TakeAimAction>());
        unit->actions.push(make_shared<MeeleeAttackAction>());
      } else if (next_action->type == ACTION_MOVE) {
        shared_ptr<MoveAction> move_action =  
          static_pointer_cast<MoveAction>(next_action);
        vec3 next_move = move_action->destination;
        double distance = length(next_move - target->position);
        if (distance > distance_to_player) {
          unit->ClearActions();
        }
      }
      break;
    } 
    case DEFEND: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<DefendAction>());
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::GoblinChieftain(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();

  ObjPtr target = unit->GetCurrentTarget();
  vec3 v_to_player = target->position - unit->position;
  double distance_to_player = length(v_to_player);
  ivec2 player_tile = dungeon.GetDungeonTile(player->position);

  bool visible = dungeon.IsTileVisible(unit->position);

  // TODO: change to function.
  bool player_reachable = IsPlayerReachable(unit);

  int unit_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(unit->position));

  // TODO: change to function.
  int player_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(player->position));

  const float kChaseDistance = 50.0f;
  bool holdback = ShouldHoldback(unit);

  shared_ptr<Action> next_action = nullptr;
  if (!unit->actions.empty()) {
    next_action = unit->actions.front();
  }

  if (unit->saw_player_attack && unit->CanUseAbility("defend") &&
      !IsAttackAction(next_action)) {
    unit->saw_player_attack = false;

    vec3 dir3d = vec3(rotate(
      mat4(1.0),
      player->rotation.y,
      vec3(0.0f, 1.0f, 0.0f)
    ) * vec4(0, 0, 1, 1));
    vec2 dir = normalize(vec2(dir3d.x, dir3d.z));
    bool monster_is_targeted = (dot(dir, 
      normalize(-vec2(v_to_player.x, v_to_player.z))) > 0.98f);
    if (!monster_is_targeted) {
      monster_is_targeted = distance_to_player < 15.0f;
    }
    
    if (monster_is_targeted) {
      if (unit->was_hit) {
        unit->was_hit = false;
      } else {
        unit->ClearActions();
        unit->ai_state = DEFEND;
        unit->state_changed_at = glfwGetTime();
      }
    }
  }

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (unit->CanUseAbility("charge")) {
        unit->actions.push(make_shared<ChargeAction>());
        break;
      } 

      if (unit->actions.empty()) {
        if (distance_to_player < 25.0f) {
          unit->actions.push(make_shared<TakeAimAction>());
          unit->actions.push(make_shared<SweepAttackAction>());
          unit->cooldowns["sweep_attack"] = glfwGetTime() + 2.0f;
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else if (distance_to_player < 25.0f && !IsAttackAction(next_action)) {
        unit->ClearActions();
        unit->actions.push(make_shared<TakeAimAction>());
        unit->actions.push(make_shared<SweepAttackAction>());
        unit->cooldowns["sweep_attack"] = glfwGetTime() + 2.0f;
      } else if (next_action->type == ACTION_MOVE) {
        shared_ptr<MoveAction> move_action =  
          static_pointer_cast<MoveAction>(next_action);
        vec3 next_move = move_action->destination;
        double distance = length(next_move - target->position);
        if (distance > distance_to_player) {
          unit->ClearActions();
        }
      }
      break;
    } 
    case DEFEND: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<DefendAction>());
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::Minotaur(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();

  ObjPtr target = unit->GetCurrentTarget();
  vec3 v_to_player = target->position - unit->position;
  double distance_to_player = length(v_to_player);
  ivec2 player_tile = dungeon.GetDungeonTile(player->position);

  bool visible = dungeon.IsTileVisible(unit->position);

  // TODO: change to function.
  bool player_reachable = IsPlayerReachable(unit);

  int unit_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(unit->position));

  // TODO: change to function.
  int player_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(player->position));

  const float kChaseDistance = 50.0f;
  bool holdback = ShouldHoldback(unit);

  shared_ptr<Action> next_action = nullptr;
  if (!unit->actions.empty()) {
    next_action = unit->actions.front();
  }

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (unit->CanUseAbility("charge")) {
        unit->actions.push(make_shared<ChargeAction>());
        break;
      } 

      if (unit->actions.empty()) {
        if (distance_to_player < 25.0f) {
          unit->actions.push(make_shared<TakeAimAction>());
          unit->actions.push(make_shared<SweepAttackAction>());
          unit->cooldowns["sweep_attack"] = glfwGetTime() + 2.0f;
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else if (distance_to_player < 25.0f && !IsAttackAction(next_action)) {
        unit->ClearActions();
        unit->actions.push(make_shared<TakeAimAction>());
        unit->actions.push(make_shared<SweepAttackAction>());
        unit->cooldowns["sweep_attack"] = glfwGetTime() + 2.0f;
      } else if (next_action->type == ACTION_MOVE) {
        shared_ptr<MoveAction> move_action =  
          static_pointer_cast<MoveAction>(next_action);
        vec3 next_move = move_action->destination;
        double distance = length(next_move - target->position);
        if (distance > distance_to_player) {
          unit->ClearActions();
        }
      }
      break;
    } 
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::Skirmisher(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();

  ObjPtr target = unit->GetCurrentTarget();
  vec3 v_to_player = target->position - unit->position;
  double distance_to_player = length(v_to_player);

  bool visible = dungeon.IsTileVisible(unit->position);

  // TODO: change to function.
  bool player_reachable = IsPlayerReachable(unit);

  int unit_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(unit->position));

  // TODO: change to function.
  int player_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(player->position));

  const float kChaseDistance = 50.0f;
  bool holdback = ShouldHoldback(unit);

  if (unit->saw_player_attack && unit->CanUseAbility("defend")) {
    unit->saw_player_attack = false;

    vec3 dir3d = vec3(rotate(
      mat4(1.0),
      player->rotation.y,
      vec3(0.0f, 1.0f, 0.0f)
    ) * vec4(0, 0, 1, 1));
    vec2 dir = normalize(vec2(dir3d.x, dir3d.z));
    bool monster_is_targeted = (dot(dir, 
      normalize(-vec2(v_to_player.x, v_to_player.z))) > 0.98f);
    
    if (monster_is_targeted) {
      if (unit->was_hit) {
        unit->was_hit = false;
      } else {
        unit->ClearActions();
        unit->ai_state = DEFEND;
        unit->state_changed_at = glfwGetTime();
      }
    }
  }

  float t;
  bool can_hit_player = !dungeon.IsRayObstructed(unit->position, 
    player->position, t);

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      if (distance_to_player > 80.0f) {
        if (Random(0, 10) == 0) {
          vec3 pos = FindSideMove(unit);
          if (length2(pos) > 0.1f) {
            unit->actions.push(make_shared<MoveAction>(pos));
          } else {
            unit->actions.push(make_shared<MoveToPlayerAction>());
          }
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else if (!unit->CanUseAbility("ranged-attack") || !can_hit_player) {
        if (!IsPlayerReachable(unit)) { 
          unit->actions.push(make_shared<ChangeStateAction>(IDLE));
        } else {
          if (distance_to_player < 50.0f && visible) {
            vec3 pos = FindSideMove(unit);
            bool next_move_visible = dungeon.IsTileVisible(pos);
            if (length2(pos) > 0.1f && next_move_visible) {
              unit->actions.push(make_shared<MoveAction>(pos));
            } else {
              unit->actions.push(make_shared<MoveToPlayerAction>());
            }
          } else {
            unit->actions.push(make_shared<MoveToPlayerAction>());
          }
        }
      } else {
        unit->actions.push(make_shared<RangedAttackAction>());
      }
      break;
    }
    case DEFEND: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<DefendAction>());
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::GlaiveMaster(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();

  ObjPtr target = unit->GetCurrentTarget();
  vec3 v_to_player = target->position - unit->position;
  double distance_to_player = length(v_to_player);

  bool visible = dungeon.IsTileVisible(unit->position);

  // TODO: change to function.
  bool player_reachable = IsPlayerReachable(unit);

  int unit_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(unit->position));

  // TODO: change to function.
  int player_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(player->position));

  const float kChaseDistance = 50.0f;
  bool holdback = ShouldHoldback(unit);

  if (unit->saw_player_attack && unit->CanUseAbility("defend")) {
    unit->saw_player_attack = false;

    vec3 dir3d = vec3(rotate(
      mat4(1.0),
      player->rotation.y,
      vec3(0.0f, 1.0f, 0.0f)
    ) * vec4(0, 0, 1, 1));
    vec2 dir = normalize(vec2(dir3d.x, dir3d.z));
    bool monster_is_targeted = (dot(dir, 
      normalize(-vec2(v_to_player.x, v_to_player.z))) > 0.98f);
    
    if (monster_is_targeted) {
      if (unit->was_hit) {
        unit->was_hit = false;
      } else {
        unit->ClearActions();
        unit->ai_state = DEFEND;
        unit->state_changed_at = glfwGetTime();
      }
    }
  }

  float t;
  bool can_hit_player = !dungeon.IsRayObstructed(unit->position, 
    player->position, t);

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      if (unit->CanUseAbility("ranged-attack")) {
        unit->actions.push(make_shared<RangedAttackAction>());
      } else {
        unit->actions.push(make_shared<ChangeStateAction>(FLEE));
      }
      break;
    }
    case FLEE: {
      if (!unit->actions.empty()) {
        break;
      }

      ivec2 target_tile = GetFrogJumpTile(unit);
      vec3 target_pos = dungeon.GetTilePosition(target_tile);
      unit->actions.push(make_shared<FrogShortJumpAction>(target_pos));

      if (unit->CanUseAbility("frog-jump")) {
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      }
      break;
    } 
    case DEFEND: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<DefendAction>());
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::Lancet(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  double distance_to_player = length(player->position - unit->position);

  bool group_is_attacking = false;
  vector<ObjPtr> monsters = resources_->GetMonstersInGroup(unit->monster_group); 
  for (auto& m : monsters) {
    if (m == unit) continue;
    if (m->ai_state == AI_ATTACK) {
      group_is_attacking = true;
      break;
    }
  }

  bool visible = dungeon.IsTileVisible(unit->position);
  bool player_reachable = IsPlayerReachable(unit);

  int unit_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(unit->position));

  int player_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(player->position));

  const float kChaseDistance = 50.0f;

  bool holdback = ShouldHoldback(unit);

  if (unit->was_hit) {
    unit->levitating = false;
    unit->was_hit = false;
    unit->ClearTemporaryStatus(STATUS_SPIDER_THREAD);
    unit->ClearActions();
    unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
  }

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!player_reachable) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(FLEE));
        break;
      }

      shared_ptr<Action> next_action = nullptr;
      if (!unit->actions.empty()) {
        next_action = unit->actions.front();
      }

      int r = Random(0, 100);
      if (unit->actions.empty()) {
        if (distance_to_player < 11.0f) {
          unit->actions.push(make_shared<TakeAimAction>());
          unit->actions.push(make_shared<MeeleeAttackAction>());
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else if (distance_to_player < 11.0f && !IsAttackAction(next_action)) {
        unit->ClearActions();
        unit->actions.push(make_shared<TakeAimAction>());
        unit->actions.push(make_shared<MeeleeAttackAction>());
      } else if (distance_to_player > 30.0f &&  
        unit->CanUseAbility("spider-web")) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(DEFEND));
      } else if (next_action->type == ACTION_MOVE) {
        shared_ptr<MoveAction> move_action =  
          static_pointer_cast<MoveAction>(next_action);
        vec3 next_move = move_action->destination;
        double distance = length(next_move - player->position);

        float t;
        bool movement_obstructed = dungeon.IsMovementObstructed(unit->position, player->position, t);
        if (distance > distance_to_player && !movement_obstructed) {
          unit->ClearActions();
        }
      }
      break;
    } 
    case AMBUSH: {
      if (!holdback) {
        unit->levitating = false;
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        resources_->ChangeObjectAnimation(unit, "Armature|walking");
        break;
      }

      shared_ptr<Action> next_action = nullptr;
      if (!unit->actions.empty()) {
        next_action = unit->actions.front();
      }

      if (next_action && next_action->type == ACTION_SPIDER_CLIMB) break;
      unit->AddTemporaryStatus(make_shared<SpiderThreadStatus>(0.1f, 20.0f));

      if (!unit->actions.empty()) {
        break;
      }

      unit->levitating = true;

      if (distance_to_player < 80.0f) {
        unit->actions.push(make_shared<TakeAimAction>());

        // Create spiderlings.
        // unit->actions.push(make_shared<SpiderEggAction>());
        // for (int i = 0; i < 10; i++) {
        //   unit->actions.push(make_shared<IdleAction>(1, "Armature|climbing"));
        //   unit->actions.push(make_shared<TakeAimAction>());
        // }
      } else {
        unit->actions.push(make_shared<IdleAction>(1, "Armature|climbing"));
        unit->actions.push(make_shared<TakeAimAction>());
      }
      break;
    }
    case DEFEND: {
      if (!unit->actions.empty()) {
        break;
      }

      bool found_target = false;
      vec3 target_pos;
      ivec2 target;

      // Try to cast web in the player path.
      vec3 dir = player->speed;
      dir.y = 0;
      target_pos = player->position + normalize(dir) * 10.0f;
      target = dungeon.GetDungeonTile(target_pos);
      if (!dungeon.GetFlag(target, DLRG_WEB_FLOOR)) {
        found_target = true;
      }
    
      // Try to cast web in the closest passage.
      if (!found_target) { 
        target = FindClosestPassage(player);
        if (target.x != -1 &&
            !dungeon.GetFlag(target, DLRG_WEB_FLOOR)) {
          target_pos = dungeon.GetTilePosition(target);
          found_target = true;
        }
      } 

      // Try to cast web some place near the player.
      if (!found_target) { 
        vector<ivec2> possible_tiles;
        for (int i = -1; i < 1; i++) {
          for (int j = -1; j < 1; j++) {
            if (i == 0 && j == 0) continue;
            ivec2 new_tile = dungeon.GetDungeonTile(player->position) +
              ivec2(i, j);
           
            if (!dungeon.IsTileClear(new_tile)) continue;
            if (dungeon.GetFlag(new_tile, DLRG_WEB_FLOOR)) continue;
            possible_tiles.push_back(new_tile);
          }
        }

        if (!possible_tiles.empty()) {
          int index = Random(0, possible_tiles.size());
          target = possible_tiles[index];
          target_pos = dungeon.GetTilePosition(target);
          found_target = true;
        }
      }

      if (found_target) {
        target_pos.y = 0.15f;
        unit->actions.push(make_shared<TakeAimAction>());
        unit->actions.push(make_shared<SpiderWebAction>(target_pos));
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      } else {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        unit->cooldowns["spider-web"] = glfwGetTime() + 5;
      }
      break;
    }
    case HIDE: {
      if (!unit->actions.empty()) {
        break;
      }

      if (!holdback) {
      // if (!holdback || visible) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        break;
      }

      ivec2 safe_tile = FindSafeTile(unit);
      if (safe_tile.x == -1) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        break;
      }

      unit->actions.push(make_shared<LongMoveAction>(
        dungeon.GetTilePosition(safe_tile)));
      break;
    }
    case ACTIVE: {
      // if (holdback) {
      //   unit->ClearActions();

      //   unit->actions.push(make_shared<ChangeStateAction>(DEFEND));
      //   break;
      // }

      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case FLEE: {
      if (!unit->actions.empty()) {
        break;
      }

      if (!holdback) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        break;
      }

      ivec2 safe_tile = FindFleeTile(unit);
      if (safe_tile.x == -1) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
        break;
      }

      unit->actions.push(make_shared<LongMoveAction>(
        dungeon.GetTilePosition(safe_tile)));
      break;
    }
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case IDLE: {
      int room1 = dungeon.GetRoom(dungeon.GetDungeonTile(unit->position));
      int room2 = dungeon.GetRoom(dungeon.GetDungeonTile(player->position));

      if (visible || room1 == room2) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(ACTIVE));
      } else if (unit->actions.empty()) {
        unit->actions.push(make_shared<IdleAction>(1));
      }
      break;
    }
    case START: {
      if (!unit->actions.empty()) {
        break;
      }

      if (Random(0, 10) < 5) {
        cout << "Ambush" << endl;
        unit->actions.push(make_shared<SpiderClimbAction>(Random(25, 41)));
        unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
      } else {
        unit->actions.push(make_shared<ChangeStateAction>(IDLE));
      }
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::Broodmother(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  ivec2 player_tile = dungeon.GetDungeonTile(player->position);

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      ivec2 far_tile = GetBroodmotherMoveTile(unit);
      vec3 target_pos = dungeon.GetTilePosition(far_tile);
      if (unit->CanUseAbility("spider-egg")) {
        unit->actions.push(make_shared<TakeAimAction>());
        unit->actions.push(make_shared<SpiderEggAction>(target_pos));
      } else {
        unit->actions.push(make_shared<LongMoveAction>(target_pos, 5.0f));
      }
      break;
    } 
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::RedMetalEye(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  double distance_to_player = length(player->position - unit->position);
  bool visible = dungeon.IsTileVisible(unit->position);

  float t;
  bool can_hit_player = !dungeon.IsRayObstructed(unit->position, 
    player->position, t);

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      if (!unit->CanUseAbility("ranged-attack") || !can_hit_player) {
        if (distance_to_player < 50.0f && visible) {
          vec3 pos = FindSideMove(unit);
          bool next_move_visible = dungeon.IsTileVisible(pos);
          if (length2(pos) > 0.1f && next_move_visible) {
            unit->actions.push(make_shared<MoveAction>(pos));
          } else {
            unit->actions.push(make_shared<MoveToPlayerAction>());
          }
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else {
        if (!unit->CanUseAbility("spin")) {
          unit->actions.push(make_shared<RangedAttackAction>());
        } else {
          unit->actions.push(make_shared<ChangeStateAction>(FLEE));
        }
      }
      break;
    }
    case FLEE: {
      if (!unit->actions.empty()) {
        break;
      }

      // vec3 target_pos = dungeon.GetTilePosition(ivec2(7, 7));
      // unit->actions.push(make_shared<LongMoveAction>(target_pos, 3.0f));
      unit->actions.push(make_shared<RedMetalSpinAction>());
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case IDLE: {
      if (!unit->actions.empty()) {
        break;
      } else if (CanDetectPlayer(unit)) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      } else if (unit->actions.empty()) {
        unit->actions.push(make_shared<IdleAction>(1));
      }
      break;
    }
    case START: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<ChangeStateAction>(IDLE));
      break;
    }
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::Scorpion(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  double distance_to_player = length(player->position - unit->position);
  bool visible = dungeon.IsTileVisible(unit->position);

  float t;
  bool can_hit_player = !dungeon.IsRayObstructed(unit->position, 
    player->position, t);

  switch (unit->ai_state) {
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      if (distance_to_player > 80.0f) {
        if (Random(0, 10) == 0) {
          vec3 pos = FindSideMove(unit);
          if (length2(pos) > 0.1f) {
            unit->actions.push(make_shared<MoveAction>(pos));
          } else {
            unit->actions.push(make_shared<MoveToPlayerAction>());
          }
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else if (!unit->CanUseAbility("ranged-attack") || !can_hit_player) {
        if (!IsPlayerReachable(unit)) { 
          unit->actions.push(make_shared<ChangeStateAction>(IDLE));
        } else {
          if (distance_to_player < 50.0f && visible) {
            vec3 pos = FindSideMove(unit);
            bool next_move_visible = dungeon.IsTileVisible(pos);
            if (length2(pos) > 0.1f && next_move_visible) {
              unit->actions.push(make_shared<MoveAction>(pos));
            } else {
              unit->actions.push(make_shared<MoveToPlayerAction>());
            }
          } else {
            unit->actions.push(make_shared<MoveToPlayerAction>());
          }
        }
      } else {
        unit->actions.push(make_shared<RangedAttackAction>());
      }
      break;
    }
    case IDLE: {
      if (!unit->actions.empty()) {
        break;
      } else if (CanDetectPlayer(unit)) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      } else if (unit->actions.empty()) {
        unit->actions.push(make_shared<IdleAction>(1));
      }
      break;
    }
    case START: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<ChangeStateAction>(IDLE));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::BlackMage(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  ivec2 player_tile = dungeon.GetDungeonTile(player->position);
  double distance_to_player = length(player->position - unit->position);
  bool visible = dungeon.IsTileVisible(unit->position);

  float t;
  bool can_hit_player = !dungeon.IsRayObstructed(unit->position, 
    player->position, t);

  switch (unit->ai_state) {
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      if (distance_to_player > 80.0f) {
        if (Random(0, 10) == 0) {
          vec3 pos = FindSideMove(unit);
          if (length2(pos) > 0.1f) {
            unit->actions.push(make_shared<MoveAction>(pos));
          } else {
            unit->actions.push(make_shared<MoveToPlayerAction>());
          }
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else if (!unit->CanUseAbility("ranged-attack") || !can_hit_player) {
        unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
      } else {
        unit->actions.push(make_shared<RangedAttackAction>());
      }
      break;
    }
    case AMBUSH: {
      ivec2 far_tile = ivec2(7, 7);
      for (int tries = 0; tries < 10; tries++) {
        int x = Random(0, 14);
        int y = Random(0, 14);
        ivec2 new_tile = ivec2(x, y);
        if (!dungeon.IsValidTile(new_tile)) continue;
        if (length(vec2(player_tile) - vec2(new_tile)) < 5.0f) continue;
        far_tile = new_tile;
      }

      if (unit->CanUseAbility("teleport")) {
        unit->actions.push(make_shared<TeleportAction>(
          dungeon.GetTilePosition(far_tile)));
      } else {
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      }
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::ShooterBug(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  ObjPtr target = unit->GetCurrentTarget();
  vec3 v_to_player = target->position - unit->position;
  double distance_to_player = length(v_to_player);
  bool visible = dungeon.IsTileVisible(unit->position);

  float t;
  bool can_hit_player = !dungeon.IsRayObstructed(unit->position, 
    player->position, t);

  if (unit->saw_player_attack && unit->CanUseAbility("defend")) {
    unit->saw_player_attack = false;

    vec3 dir3d = vec3(rotate(
      mat4(1.0),
      player->rotation.y,
      vec3(0.0f, 1.0f, 0.0f)
    ) * vec4(0, 0, 1, 1));
    vec2 dir = normalize(vec2(dir3d.x, dir3d.z));
    bool monster_is_targeted = (dot(dir, 
      normalize(-vec2(v_to_player.x, v_to_player.z))) > 0.98f);
    
    if (monster_is_targeted) {
      if (unit->was_hit) {
        unit->was_hit = false;
      } else {
        unit->ClearActions();
        unit->ai_state = DEFEND;
        unit->state_changed_at = glfwGetTime();
      }
    }
  }

  switch (unit->ai_state) {
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      if (distance_to_player > 80.0f) {
        if (Random(0, 10) == 0) {
          vec3 pos = FindSideMove(unit);
          if (length2(pos) > 0.1f) {
            unit->actions.push(make_shared<MoveAction>(pos));
          } else {
            unit->actions.push(make_shared<MoveToPlayerAction>());
          }
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else if (!unit->CanUseAbility("ranged-attack") || !can_hit_player) {
        if (!IsPlayerReachable(unit)) { 
          unit->actions.push(make_shared<ChangeStateAction>(IDLE));
        } else {
          if (distance_to_player < 50.0f && visible) {
            vec3 pos = FindSideMove(unit);
            bool next_move_visible = dungeon.IsTileVisible(pos);
            if (length2(pos) > 0.1f && next_move_visible) {
              unit->actions.push(make_shared<MoveAction>(pos));
            } else {
              unit->actions.push(make_shared<MoveToPlayerAction>());
            }
          } else {
            unit->actions.push(make_shared<MoveToPlayerAction>());
          }
        }
      } else {
        unit->actions.push(make_shared<RangedAttackAction>(true));
        unit->actions.push(make_shared<RangedAttackAction>(true));
        unit->actions.push(make_shared<RangedAttackAction>(false));
      }
      break;
    }
    case DEFEND: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<SideStepAction>());
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case IDLE: {
      if (!unit->actions.empty()) {
        break;
      } else if (CanDetectPlayer(unit)) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      } else if (unit->actions.empty()) {
        // unit->actions.push(make_shared<IdleAction>(1));
        unit->actions.push(make_shared<RandomMoveAction>());
      }
      break;
    }
    case START: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<ChangeStateAction>(IDLE));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::FlyingMetalEye(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  double distance_to_player = length(player->position - unit->position);
  bool visible = dungeon.IsTileVisible(unit->position);

  float t;
  bool can_hit_player = !dungeon.IsRayObstructed(unit->position, 
    player->position, t);

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      if (distance_to_player > 80.0f) {
        if (Random(0, 10) == 0) {
          vec3 pos = FindSideMove(unit);
          if (length2(pos) > 0.1f) {
            unit->actions.push(make_shared<MoveAction>(pos));
          } else {
            unit->actions.push(make_shared<MoveToPlayerAction>());
          }
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else if (!unit->CanUseAbility("ranged-attack") || !can_hit_player) {
        if (!IsPlayerReachable(unit)) { 
          unit->actions.push(make_shared<ChangeStateAction>(IDLE));
        } else {
          if (distance_to_player < 50.0f && visible) {
            vec3 pos = FindSideMove(unit);
            bool next_move_visible = dungeon.IsTileVisible(pos);
            if (length2(pos) > 0.1f && next_move_visible) {
              unit->actions.push(make_shared<MoveAction>(pos));
            } else {
              unit->actions.push(make_shared<MoveToPlayerAction>());
            }
          } else {
            unit->actions.push(make_shared<MoveToPlayerAction>());
          }
        }
      } else {
        unit->actions.push(make_shared<RangedAttackAction>());
      }
      break;
    }
    case IDLE: {
      if (!unit->actions.empty()) {
        break;
      } else if (CanDetectPlayer(unit)) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      } else if (unit->actions.empty()) {
        unit->actions.push(make_shared<IdleAction>(1));
      }
      break;
    }
    case START: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<ChangeStateAction>(IDLE));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::ArrowTrap(ObjPtr unit) {
  if (!unit->CanUseAbility("arrow-trap")) return;

  Dungeon& dungeon = resources_->GetDungeon();

  vec3 direction = round(vec3(unit->rotation_matrix * vec4(vec3(-1, 0, 0), 1.0)));
  ivec2 dir_2d = ivec2(direction.x, direction.z);

  ivec2 player_pos = dungeon.GetDungeonTile(resources_->GetPlayer()->position);
  ivec2 unit_pos = dungeon.GetDungeonTile(unit->position);

  bool trigger = false;
  ivec2 current_tile = unit_pos;
  for (int i = 0; i < 10; i++) {
    current_tile += dir_2d;
    if (!dungeon.IsTileClear(current_tile)) break;

    if (current_tile.x == player_pos.x && current_tile.y == player_pos.y) {
      trigger = true;
      break;
    }
  }

  if (trigger) {
    const int num_missiles = boost::lexical_cast<int>(resources_->GetGameFlag("white_spine_num_missiles"));
    const float missile_speed = boost::lexical_cast<float>(resources_->GetGameFlag("white_spine_missile_speed"));
    const float spread = boost::lexical_cast<float>(resources_->GetGameFlag("white_spine_missile_spread"));

    vec3 pos = unit->position + vec3(0, 5, 0);
    vec3 dir = vec3(direction.x, 0, direction.z);
    pos += dir * 10.0f;

    for (int i = 0; i < num_missiles; i++) {
      vec3 d = dir;
      if (i > 0) {
        float x_ang = (float) Random(-5, 6) * spread;
        float y_ang = (float) Random(-5, 6) * spread;
        vec3 right = cross(dir, vec3(0, 1, 0));
        mat4 m = rotate(mat4(1.0f), x_ang, vec3(0, 1, 0));
        d += vec3(rotate(m, y_ang, right) * vec4(dir, 1.0f));
      }

      resources_->CastMissile(unit, pos, MISSILE_HORN, 
        d, missile_speed);
      unit->cooldowns["arrow-trap"] = glfwGetTime() + 5;
    }
  }
}

void Monsters::WhiteSpine(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  double distance_to_player = length(player->position - unit->position);

  float t;
  bool can_hit_player = !dungeon.IsRayObstructed(unit->position, 
    player->position, t);

  if (unit->was_hit) {
    if (unit->CanUseAbility("defend")) {
      unit->ClearActions();
      unit->actions.push(make_shared<DefendAction>());
    }
    unit->was_hit = false;
  }

  switch (unit->ai_state) {
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      if (unit->CanUseAbility("trample")) {
        unit->actions.push(make_shared<TrampleAction>());
        break;
      } 

      shared_ptr<Action> next_action = nullptr;
      if (!unit->actions.empty()) {
        next_action = unit->actions.front();
      }

      if (unit->actions.empty()) {
        if (distance_to_player < 11.0f) {
          unit->actions.push(make_shared<TakeAimAction>());
          unit->actions.push(make_shared<MeeleeAttackAction>());
          unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      }
      break;
      break;
    }
    case DEFEND: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<TakeAimAction>());
      unit->actions.push(make_shared<RangedAttackAction>());
      break;
    }
    case IDLE: {
      if (!unit->actions.empty()) {
        break;
      } else if (CanDetectPlayer(unit)) {
        if (Random(0, 10) < 5) {
          ivec2 chokepoint = FindChokepoint(unit);
          if (chokepoint.x != -1) {
            unit->ClearActions();
            vec3 target = dungeon.GetTilePosition(chokepoint);
            unit->actions.push(make_shared<LongMoveAction>(target, 3.0f));
            unit->actions.push(make_shared<ChangeStateAction>(DEFEND));
            break;
          }
          unit->ClearActions();
          unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        } else {
          unit->ClearActions();
          unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        }
      } else if (unit->actions.empty()) {
        unit->actions.push(make_shared<IdleAction>(1));
      }
      break;
    }
    case START: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<ChangeStateAction>(IDLE));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::Imp(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  double distance_to_player = length(player->position - unit->position);

  float t;
  bool can_hit_player = !dungeon.IsRayObstructed(unit->position, 
    player->position, t);

  if (unit->was_hit) {
    unit->was_hit = false;
  }

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<MoveToPlayerAction>());
      if (!unit->CanUseAbility("ranged-attack") || !can_hit_player 
        || distance_to_player < 30.0f) {
        if (!IsPlayerReachable(unit)) { 
          unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
        } else {
          if (Random(0, 10) < 2) {
            unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
          } else {
            unit->actions.push(make_shared<MoveToPlayerAction>());
          }
        }
      } else {
        unit->actions.push(make_shared<TakeAimAction>());
        unit->actions.push(make_shared<RangedAttackAction>());
      }
      break;
    }
    case AMBUSH: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;

      ivec2 ambush_tile = FindAmbushTile(unit);
      if (ambush_tile.x == -1) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        break;
      }

      if (unit->CanUseAbility("teleport")) {
        unit->actions.push(make_shared<TeleportAction>(
          dungeon.GetTilePosition(ambush_tile)));
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case IDLE: {
      if (!unit->actions.empty()) {
        break;
      } else if (CanDetectPlayer(unit)) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      } else if (unit->actions.empty()) {
        unit->actions.push(make_shared<IdleAction>(1));
      }
      break;
    }
    case START: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<ChangeStateAction>(IDLE));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::BloodWorm(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ObjPtr player = resources_->GetPlayer();
  double distance_to_player = length(player->position - unit->position);

  if (unit->was_hit) {
    unit->ClearActions();
    unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
  }

  switch (unit->ai_state) {
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
      break;
    }
    case AI_ATTACK: {
      shared_ptr<Action> next_action = nullptr;
      if (!unit->actions.empty()) {
        next_action = unit->actions.front();
      }

      if (unit->actions.empty()) {
        if (distance_to_player < 11.0f) {
          unit->actions.push(make_shared<TakeAimAction>());
          unit->actions.push(make_shared<MeeleeAttackAction>());
          unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      }
      break;
    } 
    case AMBUSH: {
      if (!unit->actions.empty()) {
        break;
      }

      if (Random(0, 60) == 0) {
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        break;
      }

      if (distance_to_player > 60.0f) {
        unit->actions.push(make_shared<MoveToPlayerAction>());
        break;
      } 

      if (distance_to_player < 20.0f) {
        vec3 flee_pos = FindCloseFlee(unit);
        if (length2(flee_pos) > 0.1f) {
          unit->actions.push(make_shared<MoveAction>(flee_pos));
          break;
        } else {
          ivec2 next_tile = FindFleeTile(unit);
          if (next_tile.x != -1) {
            unit->actions.push(make_shared<LongMoveAction>(
              dungeon.GetTilePosition(next_tile)));
            break;
          }
        }
      }

      vec3 pos = FindSideMove(unit, /*memorize=*/true);
      if (length2(pos) > 0.1f) {
        unit->actions.push(make_shared<MoveAction>(pos));
      } else {
        unit->actions.push(make_shared<MoveToPlayerAction>());
      }
 
      break;
    } 
    default: {
      break;
    }
  }
}

void Monsters::Beholder(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ObjPtr player = resources_->GetPlayer();
  double distance_to_player = length(player->position - unit->position);

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      switch (Random(0, 1)) {
        case 0: { // Normal attack.
          unit->actions.push(make_shared<TakeAimAction>());
          unit->actions.push(make_shared<RangedAttackAction>());
          unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
          break;
        }
        // case 1: { // Fireball.
        //   if (unit->CanUseAbility("fireball")) {
        //     ObjPtr target = unit->GetCurrentTarget();
        //     unit->actions.push(make_shared<TakeAimAction>());
        //     unit->actions.push(make_shared<FireballAction>());
        //     unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
        //   }
        //   break;
        // }
        // case 2: { // Paralysis.
        //   if (unit->CanUseAbility("paralysis")) {
        //     ObjPtr target = unit->GetCurrentTarget();
        //     unit->actions.push(make_shared<TakeAimAction>());
        //     unit->actions.push(make_shared<ParalysisAction>());
        //     unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
        //   }
        //   break;
        // }
        case 3: { // CLose door.
          break;
        }
      }
      break;
    } 
    case AMBUSH: {
      if (!unit->actions.empty()) {
        break;
      }

      if (Random(0, 5) == 0) {
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        break;
      }

      if (distance_to_player > 50.0f) {
        unit->actions.push(make_shared<MoveToPlayerAction>());
        break;
      } 

      if (distance_to_player < 20.0f) {
        vec3 flee_pos = FindCloseFlee(unit);
        if (length2(flee_pos) > 0.1f) {
          unit->actions.push(make_shared<MoveAction>(flee_pos));
          break;
        } else {
          ivec2 next_tile = FindFleeTile(unit);
          if (next_tile.x != -1) {
            unit->actions.push(make_shared<LongMoveAction>(
              dungeon.GetTilePosition(next_tile)));
            break;
          }
        }
      }

      vec3 pos = FindSideMove(unit);
      if (length2(pos) > 0.1f) {
        unit->actions.push(make_shared<MoveAction>(pos));
      } else {
        unit->actions.push(make_shared<MoveToPlayerAction>());
      }
 
      break;
    } 
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::BigBeholder(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ObjPtr player = resources_->GetPlayer();
  double distance_to_player = length(player->position - unit->position);

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<TakeAimAction>());
      unit->actions.push(make_shared<RangedAttackAction>());
      unit->actions.push(make_shared<ParalysisAction>());

      unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
      break;
    } 
    case AMBUSH: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<FlyLoopAction>(Random(8, 15), 6.0f));
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));

      // if (Random(0, 5) == 0) {
      //   unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      //   break;
      // }

      // if (distance_to_player > 50.0f) {
      //   unit->actions.push(make_shared<MoveToPlayerAction>());
      //   break;
      // } 

      // if (distance_to_player < 20.0f) {
      //   vec3 flee_pos = FindCloseFlee(unit);
      //   if (length2(flee_pos) > 0.1f) {
      //     unit->actions.push(make_shared<MoveAction>(flee_pos));
      //     break;
      //   } else {
      //     ivec2 next_tile = FindFleeTile(unit);
      //     if (next_tile.x != -1) {
      //       unit->actions.push(make_shared<LongMoveAction>(
      //         dungeon.GetTilePosition(next_tile)));
      //       break;
      //     }
      //   }
      // }

      // vec3 pos = FindSideMove(unit);
      // if (length2(pos) > 0.1f) {
      //   unit->actions.push(make_shared<MoveAction>(pos));
      // } else {
      //   unit->actions.push(make_shared<MoveToPlayerAction>());
      // }
 
      break;
    } 
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::BeholderEye(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ObjPtr player = resources_->GetPlayer();

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->speed += normalize(player->position - unit->position) * 0.1f;
      break;
    } 
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::Spinner(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ivec2 unit_tile = dungeon.GetDungeonTile(unit->position);

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      static const vector<ivec2> tiles { 
        { 0, 7 }, { 13, 7 }, { 7, 0 }, { 7, 13 },
        { 13, 0 }, { 0, 13 }, { 0, 0 }, { 13, 13 }
      };

      int tile_index = 0;

      string memory = unit->ReadMemory("spin_target");
      if (memory.empty() || Random(0, 8) == 0) {
        tile_index = Random(0, tiles.size());
      } else {
        tile_index = boost::lexical_cast<int>(memory);
        tile_index++;
        if (tile_index > tiles.size()) tile_index = 0;
      }

      ivec2 tile = tiles[tile_index];
      vec3 target = dungeon.GetTilePosition(tile);
      unit->actions.push(make_shared<SpinAction>(target));
      break;
    }
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::RunMonsterAi(ObjPtr obj) {
  obj->current_target = GetTarget(obj);
  if (obj->GetAsset()->name == "spiderling") {
    Spiderling(obj); 
  } else if (obj->GetAsset()->name == "frog") {
    Frog(obj); 
  } else if (obj->GetAsset()->name == "red_frog") {
    RedFrog(obj); 
  } else if (obj->GetAsset()->name == "scorpion") {
    Scorpion(obj); 
  } else if (obj->GetAsset()->name == "shooter_bug") {
    ShooterBug(obj); 
  } else if (obj->GetAsset()->name == "spinner") {
    Spinner(obj); 
  } else if (obj->GetAsset()->name == "red_metal_eye") {
    RedMetalEye(obj); 
  } else if (obj->GetAsset()->name == "metal_eye") {
    FlyingMetalEye(obj); 
  } else if (obj->GetAsset()->name == "lancet") {
    Lancet(obj); 
  } else if (obj->GetAsset()->name == "broodmother_body") {
    Broodmother(obj); 
  } else if (obj->GetAsset()->name == "arrow_trap") {
    ArrowTrap(obj); 
  } else if (obj->GetAsset()->name == "mini_spiderling") {
    MiniSpiderling(obj); 
  } else if (obj->GetAsset()->name == "white_spine") {
    WhiteSpine(obj); 
  } else if (obj->GetAsset()->name == "imp") {
    Imp(obj); 
  } else if (obj->GetAsset()->name == "goblin") {
    Goblin(obj); 
  } else if (obj->GetAsset()->name == "goblin_chieftain") {
    GoblinChieftain(obj); 
  } else if (obj->GetAsset()->name == "skirmisher") {
    Skirmisher(obj); 
  } else if (obj->GetAsset()->name == "glaive_master") {
    GlaiveMaster(obj); 
  } else if (obj->GetAsset()->name == "wraith") {
    Wraith(obj); 
  } else if (obj->GetAsset()->name == "skeleton") {
    Skeleton(obj); 
  } else if (obj->GetAsset()->name == "blood_worm") {
    BloodWorm(obj); 
  } else if (obj->GetAsset()->name == "beholder") {
    Beholder(obj); 
  } else if (obj->GetAsset()->name == "beholder_eye") {
    BeholderEye(obj); 
  } else if (obj->GetAsset()->name == "big_beholder") {
    BigBeholder(obj); 
  } else if (obj->GetAsset()->name == "minotaur") {
    Minotaur(obj); 
  } else if (obj->GetAsset()->name == "black_mage_body") {
    BlackMage(obj); 
  } else if (obj->GetAsset()->name == "little_stag") {
    LittleStag(obj); 
  } else if (obj->GetAsset()->name == "little_stag_shadow") {
    LittleStagShadow(obj); 
  } else if (obj->GetAsset()->name == "baphomet") {
    Baphomet(obj); 
  }
}

void Monsters::Skeleton(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ObjPtr player = resources_->GetPlayer();
  double distance_to_player = length(player->position - unit->position);
  bool visible = dungeon.IsTileVisible(unit->position);
  ObjPtr target = unit->GetCurrentTarget();

  switch (unit->ai_state) {
    case AI_ATTACK: {
      shared_ptr<Action> next_action = nullptr;
      if (!unit->actions.empty()) {
        next_action = unit->actions.front();
      }

      int r = Random(0, 100);
      if (unit->actions.empty()) {
        if (distance_to_player < 11.0f) {
          unit->actions.push(make_shared<TakeAimAction>());
          unit->actions.push(make_shared<MeeleeAttackAction>());
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else if (distance_to_player < 11.0f && !IsAttackAction(next_action)) {
        unit->ClearActions();
        unit->actions.push(make_shared<TakeAimAction>());
        unit->actions.push(make_shared<MeeleeAttackAction>());
      } else if (next_action->type == ACTION_MOVE) {
        shared_ptr<MoveAction> move_action =  
          static_pointer_cast<MoveAction>(next_action);
        vec3 next_move = move_action->destination;
        double distance = length(next_move - target->position);
        if (distance > distance_to_player) {
          unit->ClearActions();
        }
      }
      break;
    } 
    case IDLE: {
      if (unit->created_obj) {
        if (unit->created_obj->position.y < -100.0f) {
          unit->status = STATUS_DEAD;
        } else if (unit->CanUseAbility("rebirth")) {
          unit->position = unit->created_obj->position + vec3(0, 2, 0);
          resources_->RemoveObject(unit->created_obj);
          unit->created_obj = nullptr;
          unit->ai_state = AI_ATTACK;
          resources_->CreateParticleEffect(10, unit->position + vec3(0, 2, 0), 
            vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 5.0, 24.0f, 15.0f, "fireball");          
          unit->always_cull = false;
        }
      }
      break;
    } 
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::Wraith(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ObjPtr player = resources_->GetPlayer();
  double distance_to_player = length(player->position - unit->position);
  bool visible = dungeon.IsTileVisible(unit->position);

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }
 
      if (Random(0, 8) == 0) {
        unit->actions.push(make_shared<FlyLoopAction>(Random(3, 5), 2.0f));
        break;
      }

      if (distance_to_player > 80.0f) {
        if (Random(0, 10) == 0) {
          vec3 pos = FindSideMove(unit);
          if (length2(pos) > 0.1f) {
            unit->actions.push(make_shared<MoveAction>(pos));
          } else {
            unit->actions.push(make_shared<MoveToPlayerAction>());
          }
        } else {
          unit->actions.push(make_shared<MoveToPlayerAction>());
        }
      } else if (!unit->CanUseAbility("ranged-attack")) {
        if (!IsPlayerReachable(unit)) { 
          unit->actions.push(make_shared<ChangeStateAction>(IDLE));
        } else {
          if (distance_to_player < 50.0f && visible) {
            vec3 pos = FindSideMove(unit);
            bool next_move_visible = dungeon.IsTileVisible(pos);
            if (length2(pos) > 0.1f && next_move_visible) {
              unit->actions.push(make_shared<MoveAction>(pos));
            } else {
              unit->actions.push(make_shared<MoveToPlayerAction>());
            }
          } else {
            unit->actions.push(make_shared<MoveToPlayerAction>());
          }
        }
      } else {
        unit->actions.push(make_shared<RangedAttackAction>());
      }
      break;
    }
    case IDLE: {
      if (unit->created_obj) {
        if (unit->created_obj->position.y < -100.0f) {
          unit->status = STATUS_DEAD;
        } else if (unit->CanUseAbility("rebirth")) {
          unit->position = unit->created_obj->position + vec3(0, 2, 0);
          resources_->RemoveObject(unit->created_obj);
          unit->created_obj = nullptr;
          unit->ai_state = AI_ATTACK;
          resources_->CreateParticleEffect(10, unit->position + vec3(0, 2, 0), 
            vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 5.0, 24.0f, 15.0f, "fireball");          
          unit->always_cull = false;
        }
      } else {
        if (!unit->actions.empty()) {
          break;
        } else if (CanDetectPlayer(unit)) {
          unit->ClearActions();
          unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        } else if (unit->actions.empty()) {
          // unit->actions.push(make_shared<IdleAction>(1));
          unit->actions.push(make_shared<RandomMoveAction>());
        }
      }
      break;
    } 
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::LittleStag(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      ivec2 tile;
      float max_length = 0.0f;
      for (int tries = 0; tries < 5; tries++) {
        ivec2 t;
        t.x = Random(0, 14);
        t.y = Random(0, 14);
        float l = length(vec2(t) - vec2(unit_tile_pos));
        if (l > max_length) {
          max_length = l;
          tile = t;
        }
      }

      unit->actions.push(make_shared<LongMoveAction>(
        dungeon.GetTilePosition(tile)));
      cout << "tile: " << tile << endl;

      unit->actions.push(make_shared<RangedAttackAction>());
      unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
      break;
    }
    case AMBUSH: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<MirrorImageAction>());
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::LittleStagShadow(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }

      unit->actions.push(make_shared<RangedAttackAction>());
      unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
      break;
    }
    case AMBUSH: {
      if (!unit->actions.empty()) {
        break;
      }

      ivec2 tile;
      float max_length = 0.0f;
      for (int tries = 0; tries < 5; tries++) {
        ivec2 t;
        t.x = Random(0, 14);
        t.y = Random(0, 14);
        float l = length(vec2(t) - vec2(unit_tile_pos));
        if (l > max_length) {
          max_length = l;
          tile = t;
        }
      }
      unit->actions.push(make_shared<LongMoveAction>(
        dungeon.GetTilePosition(tile)));

      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::Baphomet(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();

  switch (unit->ai_state) {
    case AI_ATTACK: {
      if (!unit->actions.empty()) {
        break;
      }
 
      break;
    }
    case LOADING: {
      if (!unit->actions.empty()) {
        break;
      }
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

