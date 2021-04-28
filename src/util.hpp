#ifndef __UTIL_HPP__
#define __UTIL_HPP__

#include <stdio.h>
#include <iostream>
#include <exception>
#include <memory>
#include <queue>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp> 
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>
#include <exception>
#include <tuple>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <png.h>
#include "boost/filesystem.hpp"
#include <boost/lexical_cast.hpp>  
#include <boost/algorithm/string/predicate.hpp>
#include "pugixml.hpp"

using namespace std;
using namespace glm;

enum AssetType {
  ASSET_STATIC = 0,
  ASSET_CREATURE
};

enum CollisionType {
  COL_SPHERE = 0,
  COL_BONES,
  COL_QUICK_SPHERE,
  COL_PERFECT,
  COL_CONVEX_HULL,
  COL_OBB,
  COL_NONE ,
  COL_UNDEFINED
};

enum PhysicsBehavior {
  PHYSICS_UNDEFINED = 0,
  PHYSICS_NONE,
  PHYSICS_NORMAL,
  PHYSICS_LOW_GRAVITY,
  PHYSICS_NO_FRICTION,
  PHYSICS_FIXED,
  PHYSICS_FLY,
  PHYSICS_SWIM
};

enum GameObjectType {
  GAME_OBJ_DEFAULT = 0,
  GAME_OBJ_PLAYER,
  GAME_OBJ_SECTOR,
  GAME_OBJ_PORTAL,
  GAME_OBJ_MISSILE,
  GAME_OBJ_PARTICLE_GROUP,
  GAME_OBJ_REGION,
  GAME_OBJ_WAYPOINT,
  GAME_OBJ_DOOR,
  GAME_OBJ_ACTIONABLE
};

enum Status {
  STATUS_NONE = 0,
  STATUS_TAKING_HIT,
  STATUS_DYING,
  STATUS_DEAD,
  STATUS_BEING_EXTRACTED,
  STATUS_PARALYZED,
  STATUS_BURROWED
};

enum AiState {
  IDLE = 0, 
  MOVE = 1, 
  AI_ATTACK = 2,
  DIE = 3,
  TURN_TOWARD_TARGET = 4,
  WANDER = 5,
  CHASE = 6,
  SCRIPT = 7
};

enum PlayerAction {
  PLAYER_IDLE,
  PLAYER_CASTING,
  PLAYER_EXTRACTING,
};

enum ActionType {
  ACTION_MOVE = 0,
  ACTION_RANDOM_MOVE,
  ACTION_IDLE,
  ACTION_MEELEE_ATTACK,
  ACTION_RANGED_ATTACK,
  ACTION_CHANGE_STATE,
  ACTION_TAKE_AIM,
  ACTION_STAND,
  ACTION_TALK,
  ACTION_LOOK_AT,
  ACTION_CAST_SPELL,
  ACTION_WAIT,
  ACTION_ANIMATION
};

enum ParticleBehavior {
  PARTICLE_FIXED = 0,
  PARTICLE_FALL,
  PARTICLE_NONE
};

enum EventType {
  EVENT_ON_INTERACT_WITH_SECTOR = 0,
  EVENT_ON_INTERACT_WITH_DOOR,
  EVENT_NONE 
};

enum DoorState {
  DOOR_CLOSED = 0,
  DOOR_OPENING,
  DOOR_OPEN,
  DOOR_CLOSING,
  DOOR_WEAK_LOCK
};

struct Camera {
  vec3 position; 
  vec3 up; 
  vec3 direction;
  vec3 rotation;
  vec3 right;

  Camera() {}
  Camera(vec3 position, vec3 direction, vec3 up) : position(position), 
    up(up), direction(direction) {}
};

struct Plane {
  vec3 normal;
  float d;
  Plane() {}
  Plane(vec3 normal, float d) : normal(normal), d(d) {}
};

struct AABB {
  vec3 point;
  vec3 dimensions;
  AABB() {}
  AABB(vec3 point, vec3 dimensions) : point(point), dimensions(dimensions) {}
};

struct BoundingSphere {
  vec3 center;
  float radius;
  BoundingSphere() {}
  BoundingSphere(vec3 center, float radius) : center(center), radius(radius) {}
};

struct OBB {
  vec3 center;
  vec3 axis[3];
  vec3 half_widths;

  OBB() {}
  OBB(const OBB &obb) {
    center = obb.center; 
    half_widths = obb.half_widths;
    for (int i = 0; i < 3; i++) { axis[i] = obb.axis[i]; }
  }
};

struct Edge {
  vec3 a;
  vec3 b;
  unsigned int a_id;
  unsigned int b_id;
  vec3 a_normal;
  vec3 b_normal;
  Edge() {}
  Edge(vec3 a, vec3 b, unsigned int a_id, unsigned int b_id,
    vec3 a_normal, vec3 b_normal) : a(a), b(b), a_id(a_id), b_id(b_id), 
    a_normal(a_normal), b_normal(b_normal) {}
};

struct Polygon {
  vector<vec3> vertices;
  vector<vec3> normals;
  vector<vec2> uvs;
  vector<unsigned int> indices;
  Polygon() {}
  Polygon(const Polygon &p2);
};

struct Keyframe {
  int time;
  vector<mat4> transforms;
};

struct Animation {
  string name;
  vector<Keyframe> keyframes;
};

struct SphereTreeNode {
  BoundingSphere sphere;
  shared_ptr<SphereTreeNode> lft, rgt;

  bool has_polygon = false;
  Polygon polygon;
  SphereTreeNode() {}
};

struct AABBTreeNode {
  AABB aabb;
  shared_ptr<AABBTreeNode> lft = nullptr, rgt = nullptr;

  bool has_polygon = false;
  Polygon polygon;
  AABBTreeNode() {}
};

struct Mesh {
  GLuint shader;
  GLuint vertex_buffer_;
  GLuint uv_buffer_;
  GLuint normal_buffer_;
  GLuint tangent_buffer_;
  GLuint bitangent_buffer_;
  GLuint element_buffer_;
  GLuint vao_ = 0;
  GLuint num_indices;
  vector<Polygon> polygons;
  unordered_map<string, Animation> animations;
  unordered_map<string, int> bones_to_ids;

  Mesh() {}
  // Mesh(const Mesh& mesh) {
  //   shader            = mesh.shader;
  //   vertex_buffer_    = mesh.vertex_buffer_;
  //   uv_buffer_        = mesh.uv_buffer_;
  //   normal_buffer_    = mesh.normal_buffer_;
  //   tangent_buffer_   = mesh.tangent_buffer_;
  //   bitangent_buffer_ = mesh.bitangent_buffer_;
  //   element_buffer_   = mesh.element_buffer_;
  //   vao_              = mesh.vao_;          
  //   num_indices       = mesh.num_indices;          
  // }
};

struct Event {
  EventType type;
  Event(EventType type) : type(type) {}
};

struct RegionEvent : Event {
  string interaction = "enter"; // enter, leave.
  string region;
  string unit;
  string callback;

  RegionEvent() : Event(EVENT_ON_INTERACT_WITH_SECTOR) {}
  RegionEvent(string interaction, string region, string unit, string callback)
    : Event(EVENT_ON_INTERACT_WITH_SECTOR), interaction(interaction),
      region(region), unit(unit), callback(callback) {}
};

struct DoorEvent : Event {
  string interaction = "open";
  string door;
  string callback;

  DoorEvent() : Event(EVENT_ON_INTERACT_WITH_DOOR) {}
  DoorEvent(string interaction, string door, string callback)
    : Event(EVENT_ON_INTERACT_WITH_DOOR), interaction(interaction),
      door(door), callback(callback) {}
};

using MeshPtr = shared_ptr<Mesh>;
using ConvexHull = vector<Polygon>;

GLuint GetUniformId(GLuint program_id, string name);
void BindBuffer(const GLuint& buffer_id, int slot, int dimension);
GLuint LoadPng(const char* file_name, bool poor_filtering=false);
GLuint LoadShader(const std::string& directory, const std::string& name);
vector<vec3> GetAllVerticesFromPolygon(const Polygon& polygon);
vector<vec3> GetAllVerticesFromPolygon(const vector<Polygon>& polygons);
vector<Edge> GetPolygonEdges(const Polygon& polygon);
Mesh CreateMesh(GLuint shader_id, vector<vec3>& vertices, vector<vec2>& uvs, 
  vector<unsigned int>& indices);
void UpdateMesh(Mesh& m, vector<vec3>& vertices, 
  vector<vec2>& uvs, vector<unsigned int>& indices);
Mesh CreateMeshFromConvexHull(const ConvexHull& ch);
Mesh CreateCube(const vector<vec3>& v);
Mesh CreateCube(const vec3& dimensions);
Mesh CreateCube(const vector<vec3>& v, vector<vec3>& vertices, 
  vector<vec2>& uvs, vector<unsigned int>& indices);
Mesh CreateMeshFromAABB(const AABB& aabb);
Mesh CreatePlane(vec3 p1, vec3 p2, vec3 normal);
Mesh CreateJoint(vec3 start, vec3 end);

// ========================
// Logging functions.
// ========================
ostream& operator<<(ostream& os, const vec2& v);
ostream& operator<<(ostream& os, const vec3& v);
ostream& operator<<(ostream& os, const vec4& v);
ostream& operator<<(ostream& os, const vector<vec4>& v);
ostream& operator<<(ostream& os, const ivec3& v);
ostream& operator<<(ostream& os, const mat4& m);
ostream& operator<<(ostream& os, const quat& q);
ostream& operator<<(ostream& os, const Edge& e);
ostream& operator<<(ostream& os, const Polygon& p);
ostream& operator<<(ostream& os, const ConvexHull& ch);
ostream& operator<<(ostream& os, const BoundingSphere& bs);
ostream& operator<<(ostream& os, const AABB& aabb);
ostream& operator<<(ostream& os, const OBB& obb);
ostream& operator<<(ostream& os, const shared_ptr<SphereTreeNode>& 
  sphere_tree_node);
ostream& operator<<(ostream& os, const shared_ptr<AABBTreeNode>& 
  aabb_tree_node);

vec3 operator*(const mat4& m, const vec3& v);
Polygon operator*(const mat4& m, const Polygon& poly);
Polygon operator+(const Polygon& poly, const vec3& v);
vector<Polygon> operator+(const vector<Polygon>& polys, const vec3& v);
BoundingSphere operator+(const BoundingSphere& sphere, const vec3& v);
BoundingSphere operator-(const BoundingSphere& sphere, const vec3& v);
AABB operator+(const AABB& aabb, const vec3& v);
AABB operator-(const AABB& aabb, const vec3& v);
bool operator==(const mat4& m1, const mat4& m2);
bool operator!=(const mat4& m1, const mat4& m2);

template<typename First, typename ...Rest>
void sample_log(First&& first, Rest&& ...rest) {
  static int i = 0;
  if (i++ % 100 == 0) return;
  cout << forward<First>(first) << endl;
  sample_log(forward<Rest>(rest)...);
}

quat RotationBetweenVectors(vec3 start, vec3 dest);
quat RotateTowards(quat q1, quat q2, float max_angle);

AABB GetAABBFromVertices(const vector<vec3>& vertices);
AABB GetAABBFromPolygons(const vector<Polygon>& polygons);
AABB GetAABBFromPolygons(const Polygon& polygon);

Mesh CreateDome(int dome_radius = 9000, int num_circles = 32, 
  int num_points_in_circle = 64);

float clamp(float v, float low, float high);
vec3 clamp(vec3 v, float low, float high);

CollisionType StrToCollisionType(const std::string& s);
std::string CollisionTypeToStr(const CollisionType& col);

ParticleBehavior StrToParticleBehavior(const std::string& s);

PhysicsBehavior StrToPhysicsBehavior(const std::string& s);

DoorState StrToDoorState(const std::string& s);

string DoorStateToStr(const DoorState& state);

AiState StrToAiState(const std::string& s);

string AiStateToStr(const AiState& ai_state);

BoundingSphere GetBoundingSphereFromVertices(
  const vector<vec3>& vertices);

BoundingSphere GetAssetBoundingSphere(const vector<Polygon>& polygons);

vec3 LoadVec3FromXml(const pugi::xml_node& node);
vec4 LoadVec4FromXml(const pugi::xml_node& node);

float LoadFloatFromXml(const pugi::xml_node& node);

mat4 GetBoneTransform(Mesh& mesh, const string& animation_name, 
  int bone_id, int frame);

int GetNumFramesInAnimation(Mesh& mesh, const string& animation_name);

bool MeshHasAnimation(Mesh& mesh, const string& animation_name);

void AppendXmlAttr(pugi::xml_node& node, const string& attr, const string& s);
void AppendXmlAttr(pugi::xml_node& node, const vec3* v);
void AppendXmlNode(pugi::xml_node& node, const string& name, const vec3& v);
void AppendXmlNode(pugi::xml_node& node, const string& name, 
  const Polygon& polygon);
void AppendXmlNode(pugi::xml_node& node, const string& name, 
  const BoundingSphere& bs);
void AppendXmlNode(pugi::xml_node& node, const string& name, const AABB& aabb);
void AppendXmlNode(pugi::xml_node& node, const string& name, const OBB& obb);
void AppendXmlTextNode(pugi::xml_node& node, const string& name, const float f);
void AppendXmlNode(pugi::xml_node& node, const string& name, 
  shared_ptr<AABBTreeNode> aabb_tree_node);
void AppendXmlTextNode(pugi::xml_node& node, const string& name, 
  const string& s);
void AppendXmlNode(pugi::xml_node& node, const string& name, quat new_quat);
void AppendXmlAttr(pugi::xml_node& node, const vec4& v);

void CreateCube(vector<vec3>& vertices, vector<vec2>& uvs, 
  vector<unsigned int>& indices, vector<Polygon>& polygons,
  vec3 dimensions);

Mesh CreateSphere(int dome_radius, int num_circles, int num_points_in_circle);

#endif // __UTIL_HPP__
