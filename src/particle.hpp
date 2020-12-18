#ifndef __PARTICLE_HPP__
#define __PARTICLE_HPP__

#include "game_object.hpp"

enum ParticleBehavior {
  PARTICLE_FIXED = 0,
  PARTICLE_FALL = 1
};

struct ParticleType {
  int id;
  string name;
  ParticleBehavior behavior = PARTICLE_FALL;
  int grid_size;
  int first_frame;
  int num_frames;
  int keep_frame = 1;
  GLuint texture_id = 0;
};

struct Particle;
struct GameObject;

struct ParticleGroup : GameObject {
  vector<Particle> particles;
  ParticleGroup() : GameObject(GAME_OBJ_PARTICLE_GROUP) {}
};

struct Particle {
  shared_ptr<ParticleType> type = nullptr;

  float size;
  vec4 color = vec4(0.0, 0.0, 0.0, 0.0);

  vec3 pos;
  vec3 speed;
  int life = -1;
  int frame = 0;

  float camera_distance;
  bool operator<(const Particle& that) const {
    if (!this->type) return false;
    if (!that.type) return true;

    // Sort in reverse order : far particles drawn first.
    if (this->type->id == that.type->id) {
      return this->camera_distance > that.camera_distance;
    }

    // Particles with the smaller particle type id first.
    return this->type->id < that.type->id;
  }
};

#endif // __PARTICLE_HPP__

