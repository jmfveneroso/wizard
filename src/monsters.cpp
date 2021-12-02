#include "monsters.hpp"
#include <queue>
#include <iomanip>

Monsters::Monsters(shared_ptr<Resources> resources) : resources_(resources) {
}

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

void Monsters::Spiderling(ObjPtr unit) {
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
  } else if (obj->GetAsset()->name == "white_spine") {
    WhiteSpine(obj); 
  } else if (obj->GetAsset()->name == "wraith") {
    Wraith(obj); 
  } else if (obj->GetAsset()->name == "blood_worm") {
    BloodWorm(obj); 
  }
}
