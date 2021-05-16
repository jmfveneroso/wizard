#ifndef __PARTICLE_HPP__
#define __PARTICLE_HPP__

#include "game_object.hpp"

struct Particle;
struct GameObject;

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

struct ParticleGroup : GameObject {
  vector<shared_ptr<Particle>> particles;
  ParticleGroup(Resources* resources) 
    : GameObject(resources, GAME_OBJ_PARTICLE_GROUP) {}
};

struct Particle : GameObject {
  shared_ptr<ParticleType> particle_type = nullptr;

  float size;
  vec4 color = vec4(0.0, 0.0, 0.0, 0.0);

  int frame = 0;

  float camera_distance;
  bool operator<(const shared_ptr<Particle>& that) const {
    if (!this->particle_type) return false;
    if (!that->particle_type) return true;

    // Sort in reverse order : far particles drawn first.
    if (this->particle_type->id == that->particle_type->id) {
      return this->camera_distance > that->camera_distance;
    }

    // Particles with the smaller particle type id first.
    return this->particle_type->id < that->particle_type->id;
  }

  Particle(Resources* resources) : GameObject(resources, GAME_OBJ_PARTICLE) {
    life = -1;
  }
};

#endif // __PARTICLE_HPP__

