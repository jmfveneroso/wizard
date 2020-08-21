#ifndef __ASSET_HPP__
#define __ASSET_HPP__

#include <fbxsdk.h>
#include <stdio.h>
#include <iostream>
#include <exception>
#include <memory>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp> 
#include <exception>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <unordered_map>
#include "util.hpp"
#include "collision.hpp"
#include "fbx_loader.hpp"

//   enum CollisionType {
//     COL_UNDEFINED = 0,
//     COL_PERFECT,
//     COL_AABB,
//     COL_OBB,
//     COL_SPHERE,
//     COL_OBB_TREE,
//     COL_NONE 
//   };
//   
//   enum ShaderType {
//     SHADER_UNDEFINED = 0,
//     SHADER_SOLID,
//     SHADER_WIREFRAME,
//     SHADER_CEL_SHADER,
//     SHADER_NORMAL
//   };
//   
//   struct Mesh {
//     GLuint vao;
//     GLuint vertex_buffer;
//     GLuint uv_buffer;
//     GLuint normal_buffer;
//     GLuint element_buffer;
//     GLuint num_indices;
//     vector<Polygon> polygons;
//   };
//   
//   class GameAsset {
//     int id;
//   
//     // Mesh.
//     vector<shared_ptr<Mesh>> lod_meshes;
//   
//     // Texture.
//     int texture_id;
//   
//     // Animation.
//     shared_ptr<AnimationData> anim;
//   
//     // Rendering.
//     ShaderType shader_type;
//     ConvexHull occluding_hull;
//   
//     // Collision.
//     CollisionType collision_type;
//     BoundingSphere bounding_sphere;
//     AABB aabb;
//     ConvexHull convex_hull;
//   
//     // TODO: implement OBB tree.
//     // shared_ptr<OBBTree> obb_tree;
//   
//     // TODO: collision for joints.
//   };

// TODO: repeat game asset instead of replicating aabb, sphere polygons for
// every object.
struct GameObject {
  int id;
  vec3 position;
  vec3 rotation;
  float distance;
  vector<Polygon> polygons;
  bool collide = false;
  string name;
  int occluder_id = -1;
  bool draw = true;
  AABB aabb;
  BoundingSphere bounding_sphere;

  Mesh lods[5]; // 0 - 5.

  vector<vector<mat4>> joint_transforms;
  int active_animation = 0;
  int frame = 0;

  GameObject() {}
  GameObject(Mesh mesh, vec3 position) : position(position) {
    lods[0] = mesh;
  }
  GameObject(Mesh mesh, vec3 position, vec3 rotation) : position(position), 
    rotation(rotation) {
    lods[0] = mesh;
  }
};

#endif // __ASSET_HPP__
