#include "monsters.hpp"
#include <queue>
#include <iomanip>

Monsters::Monsters(shared_ptr<Resources> resources) : resources_(resources) {}

bool Monsters::IsAttackAction(shared_ptr<Action> action) {
  if (!action) return false;
  switch (action->type) {
    case ACTION_MEELEE_ATTACK:
    case ACTION_RANGED_ATTACK:
    case ACTION_TAKE_AIM:
      return true;
    default: 
      break;
  }
  return false;
}

bool Monsters::ShouldHoldback(ObjPtr unit) {
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

  int unit_relevance = 
    dungeon.GetRelevance(dungeon.GetDungeonTile(unit->position));

  int player_relevance = 
    dungeon.GetRelevance(dungeon.GetDungeonTile(player->position));

  const float kChaseDistance = 50.0f;

  if (unit->leader) {
    group_is_attacking = false;
  }

  return !group_is_attacking && distance_to_player > kChaseDistance &&
    unit_relevance >= player_relevance;
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
    case IDLE: {
      unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      break;
    }
    default: {
      break;
    }
  }
}

ivec2 Monsters::FindSafeTile(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);

  for (int k = 0; k < 10; k++) {
    vector<ivec2> possible_tiles;
    for (int i = -1; i < 1; i++) {
      for (int j = -1; j < 1; j++) {
        if (i == j && i == 0) continue;
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

void Monsters::Spiderling(ObjPtr unit) {
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

  bool visible = dungeon.IsTileVisible(unit->position);

  int unit_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(unit->position));

  int player_relevance = 
    dungeon.GetRelevance(dungeon.GetTilePosition(player->position));

  const float kChaseDistance = 50.0f;

  bool holdback = ShouldHoldback(unit);

  if (unit->was_hit) {
    unit->levitating = false;
    unit->was_hit = false;
    resources_->ChangeObjectAnimation(unit, "Armature|taking_hit");
    unit->ClearActions();
    unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
  }

  switch (unit->ai_state) {
    case AI_ATTACK: {
      shared_ptr<Action> next_action = nullptr;
      if (!unit->actions.empty()) {
        next_action = unit->actions.front();
      }

      if (next_action && next_action->type == ACTION_SPIDER_JUMP) break;
      if (!unit->can_jump) break;

      bool can_hit_player = false;
      if (visible) {
        float t;
        can_hit_player = !dungeon.IsRayObstructed(unit->position, player->position, t);
      }

      if (distance_to_player > 50.0f && unit->can_jump) {
        unit->actions.push(make_shared<SpiderJumpAction>(player->position));
        break;
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
        double distance = length(next_move - player->position);
        if (distance > distance_to_player) {
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

      if (!unit->line_obj) {
        unit->line_obj = CreateGameObjFromAsset(resources_.get(), "spider_thread", 
          unit->position);
      } 
      unit->line_obj->position = unit->position + vec3(0, 4.0f, 0);

      if (!unit->actions.empty()) {
        break;
      }

      unit->levitating = true;
      unit->actions.push(make_shared<TakeAimAction>());
      unit->actions.push(make_shared<SpiderEggAction>());

      for (int i = 0; i < 10; i++) {
        unit->actions.push(make_shared<IdleAction>(1, "Armature|climbing"));
        unit->actions.push(make_shared<TakeAimAction>());
      }
      break;
    }
    case DEFEND: {
      if (!holdback) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        break;
      }
     
      if (!dungeon.IsTileVisible(unit->position)) {
        unit->actions.push(make_shared<TakeAimAction>());
        unit->actions.push(make_shared<RangedAttackAction>());
      } else {
        ivec2 safe_tile = FindSafeTile(unit);
        if (safe_tile.x == -1) {
          unit->ClearActions();
          unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
          break;
        }

        unit->actions.push(make_shared<LongMoveAction>(
          dungeon.GetTilePosition(safe_tile)));
      }
      break;
    }
    case HIDE: {
      if (!unit->actions.empty()) {
        break;
      }

      if (!holdback) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
        break;
      }

      if (!dungeon.IsTileVisible(unit->position)) {
        unit->ClearActions();
        int r = Random(0, 100);
        if (r < 30) {
          unit->actions.push(make_shared<SpiderClimbAction>(Random(25, 41)));
          unit->actions.push(make_shared<ChangeStateAction>(AMBUSH));
        } else {
          unit->actions.push(make_shared<IdleAction>(0.5));
        }
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
      if (holdback) {
        unit->ClearActions();

        int r = Random(0, 100);
        if (r < 30) {
          unit->ClearActions();
          unit->actions.push(make_shared<ChangeStateAction>(DEFEND));
        } else {
          unit->actions.push(make_shared<ChangeStateAction>(HIDE));
        }
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
        // unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      } else if (unit->actions.empty()) {
        unit->actions.push(make_shared<IdleAction>(1));
      }
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::WhiteSpine(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  ObjPtr player = resources_->GetPlayer();
  double distance_to_player = length(player->position - unit->position);
  bool visible = dungeon.IsTileVisible(unit->position);

  switch (unit->ai_state) {
    case AI_ATTACK: {
      bool can_hit_player = false;
      if (visible) {
        float t;
        can_hit_player = !dungeon.IsRayObstructed(unit->position, player->position, t);
      }

      shared_ptr<Action> next_action = nullptr;
      if (!unit->actions.empty()) {
        next_action = unit->actions.front();
      }

      if (!next_action) {
        if (can_hit_player && distance_to_player < 121.0) {
          unit->actions.push(make_shared<TakeAimAction>());
          unit->actions.push(make_shared<RangedAttackAction>());
          unit->actions.push(make_shared<IdleAction>(3));
        }
        unit->actions.push(make_shared<MoveToPlayerAction>());
      } else if (distance_to_player < 91.0 && can_hit_player && 
        !IsAttackAction(next_action) && next_action->type != ACTION_IDLE) {
        unit->ClearActions();
        cout << "Clearing white spine" << endl;
        unit->actions.push(make_shared<TakeAimAction>());
        unit->actions.push(make_shared<RangedAttackAction>());
        unit->actions.push(make_shared<IdleAction>(3));
      }
      break;
    }
    case IDLE: {
      if (visible) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      } else if (unit->actions.empty()) {
        unit->actions.push(make_shared<IdleAction>(1));
      }
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
      bool can_hit_player = false;
      if (visible) {
        float t;
        can_hit_player = !dungeon.IsRayObstructed(unit->position, player->position, t);
      }

      shared_ptr<Action> next_action = nullptr;
      if (!unit->actions.empty()) {
        next_action = unit->actions.front();
      }

      if (unit->actions.empty()) {
        if (distance_to_player < 121.0) {
          if (can_hit_player) {
            unit->actions.push(make_shared<TakeAimAction>());
            unit->actions.push(make_shared<RangedAttackAction>());
          }
          unit->actions.push(make_shared<MoveToPlayerAction>());
        } else {
          unit->actions.push(make_shared<ChangeStateAction>(IDLE));
        }
      } else if (distance_to_player < 121.0 && can_hit_player && 
        !IsAttackAction(next_action)) {
        unit->ClearActions();
        unit->actions.push(make_shared<TakeAimAction>());
        unit->actions.push(make_shared<RangedAttackAction>());
        unit->actions.push(make_shared<MoveToPlayerAction>());
      }
      break;
    }
    case IDLE: {
      if (visible) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      } else if (unit->actions.empty()) {
        unit->actions.push(make_shared<IdleAction>(1));
      }
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
    case IDLE: {
      bool visible = dungeon.IsTileVisible(unit->position);
      if (visible) {
        unit->ClearActions();
        unit->actions.push(make_shared<ChangeStateAction>(AI_ATTACK));
      } else if (unit->actions.empty()) {
        unit->actions.push(make_shared<IdleAction>(1));
      }
      break;
    }
    default: {
      break;
    }
  }
}

void Monsters::RunMonsterAi(ObjPtr obj) {
  if (obj->GetAsset()->name == "spiderling") {
    Spiderling(obj); 
  } else if (obj->GetAsset()->name == "mini_spiderling") {
    MiniSpiderling(obj); 
  } else if (obj->GetAsset()->name == "white_spine") {
    WhiteSpine(obj); 
  } else if (obj->GetAsset()->name == "wraith") {
    Wraith(obj); 
  } else if (obj->GetAsset()->name == "blood_worm") {
    BloodWorm(obj); 
  }
}
