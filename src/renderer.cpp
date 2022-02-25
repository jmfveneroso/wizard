#include "renderer.hpp"
#include "boost/filesystem.hpp"
#include <boost/algorithm/string/predicate.hpp>
#include "fbx_loader.hpp"

namespace {

void DoInOrder() {}

template<typename Lambda0, typename ...Lambdas>
void DoInOrder(Lambda0&& lambda0, Lambdas&&... lambdas) {
  forward<Lambda0>(lambda0)();
  DoInOrder(std::forward<Lambdas>(lambdas)...);
}

template<typename First, typename ...Args>
void ThrowError(First first, Args&& ...args) {
  std::stringstream ss;
  ss << first;
  DoInOrder( [&](){
    ss << std::forward<Args>(args);
  }...);
  throw runtime_error(ss.str());
}

} // End of namespace.

const int kMaxDungeonTiles = 2048;

Renderer::Renderer(shared_ptr<Resources> asset_catalog, 
  shared_ptr<Draw2D> draw_2d, shared_ptr<Project4D> project_4d, 
  shared_ptr<Inventory> inventory, GLFWwindow* window, int window_width, 
  int window_height) : 
  resources_(asset_catalog), draw_2d_(draw_2d), project_4d_(project_4d),
  inventory_(inventory), window_(window), window_width_(window_width), 
  window_height_(window_height) {
  cout << "Window width: " << window_width_ << endl;
  cout << "Window height: " << window_height_ << endl;
  Init();
}

Renderer::~Renderer() {
  terminate_ = true;
  for (int i = 0; i < kMaxThreads; i++) {
    // find_threads_[i].join();
  }
}

void Renderer::InitShadowFramebuffer() {
  for (int i = 0; i < 3; i++) {
    // The framebuffer, which regroups 0, 1, or more textures, and 0 or 1 depth buffer.
    glGenFramebuffers(1, &shadow_framebuffers_[i]);
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_framebuffers_[i]);
 
    // Depth texture. Slower than a depth buffer, but you can sample it later in your shader
    glGenTextures(1, &shadow_textures_[i]);
    glBindTexture(GL_TEXTURE_2D, shadow_textures_[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_textures_[i], 0);
    glDrawBuffer(GL_NONE); // No color buffer is drawn to.
 
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      throw runtime_error("Error creating shadow framebuffer");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
}

void Renderer::Init() {
  InitShadowFramebuffer();

  terrain_ = make_shared<Terrain>(resources_->GetShader("terrain"), 
    resources_->GetShader("far_terrain"));
  terrain_->set_asset_catalog(resources_);
  terrain_->set_shadow_texture(shadow_textures_[0], 0);
  terrain_->set_shadow_texture(shadow_textures_[1], 1);
  terrain_->set_shadow_texture(shadow_textures_[2], 2);

  CreateParticleBuffers();

  CreateThreads();
}

void Renderer::GetFrustumPlanes(vec4 frustum_planes[6]) {
  mat4 ModelMatrix = translate(mat4(1.0), camera_.position);
  mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
  mat3 ModelView3x3Matrix = mat3(ModelViewMatrix);
  mat4 MVP = projection_matrix_ * view_matrix_ * ModelMatrix;
  ExtractFrustumPlanes(MVP, frustum_planes);
}

void Renderer::GetVisibleObjects(
  shared_ptr<OctreeNode> octree_node) {
  if (!octree_node) {
    return;
  }

  AABB aabb;
  aabb.point = octree_node->center - octree_node->half_dimensions;
  aabb.dimensions = octree_node->half_dimensions * 2.0f;

  vec3 closest = ClosestPtPointAABB(player_pos_, aabb);
  if (resources_->GetConfigs()->render_scene == "dungeon" && 
      length(closest - player_pos_) > resources_->GetConfigs()->light_radius + 5) {
   return;
  }

  // TODO: find better name for this function.
  if (!CollideAABBFrustum(aabb, frustum_planes_, player_pos_)) {
    return;
  }

  for (int i = 0; i < 8; i++) {
    find_mutex_.lock();
    find_tasks_.push(octree_node->children[i]);
    find_mutex_.unlock();
    // GetPotentiallyVisibleObjects(player_pos, octree_node->children[i]);
  }

  // TODO: remove far objects from octree. Store assets based on location.
  for (auto& [id, obj] : octree_node->objects) {
    switch (obj->type) {
      case GAME_OBJ_DEFAULT:
      case GAME_OBJ_REGION:
      case GAME_OBJ_WAYPOINT:
      case GAME_OBJ_DOOR:
      case GAME_OBJ_ACTIONABLE:
      case GAME_OBJ_DESTRUCTIBLE:
        break;
      case GAME_OBJ_MISSILE: {
        if (obj->life > 0.0f || obj->scale_out > 0.0f) {
          break;
        } else {
          continue;
        }
      }
      case GAME_OBJ_PARTICLE: {
        if (obj->Is3dParticle()) {
          break;
        } else {
          continue;
        }
      }
      default:
        continue;
    }

    // if (obj->status == STATUS_DEAD) continue;
    vector<vector<Polygon>> occluder_convex_hulls;
    if (CullObject(obj, occluder_convex_hulls)) continue;
    find_mutex_.lock();
    if (!obj->IsDungeonPiece()) { 
      visible_objects_.push_back(obj);
    }
    find_mutex_.unlock();
  }

  for (auto& [id, obj] : octree_node->moving_objs) {
    switch (obj->type) {
      case GAME_OBJ_DEFAULT:
      case GAME_OBJ_REGION:
      case GAME_OBJ_WAYPOINT:
      case GAME_OBJ_DOOR:
      case GAME_OBJ_ACTIONABLE:
      case GAME_OBJ_DESTRUCTIBLE:
        break;
      case GAME_OBJ_MISSILE: {
        if (obj->life > 0.0f || obj->scale_out > 0.0f) {
          break;
        } else {
          continue;
        }
      }
      case GAME_OBJ_PARTICLE: {
        if (obj->Is3dParticle()) {
          break;
        } else {
          continue;
        }
      }
      default:
        continue;
    }

    // if (obj->status == STATUS_DEAD) continue;
    vector<vector<Polygon>> occluder_convex_hulls;
    if (CullObject(obj, occluder_convex_hulls)) continue;
    find_mutex_.lock();
    if (!obj->IsDungeonPiece()) { 
      visible_objects_.push_back(obj);
    }
    find_mutex_.unlock();
  }
}

vector<shared_ptr<GameObject>> 
Renderer::GetVisibleObjectsFromSector(shared_ptr<Sector> sector) {
  find_mutex_.lock();
  visible_objects_.clear();
  player_pos_ = camera_.position;
  find_tasks_.push(sector->octree_node);
  find_mutex_.unlock();

  // while (!find_tasks_.empty() || running_tasks_ > 0) {
  //   this_thread::sleep_for(chrono::microseconds(200));
  // }

  // Sync.
  while (!find_tasks_.empty()) {
    shared_ptr<OctreeNode> node = find_tasks_.front();
    find_tasks_.pop();
    GetVisibleObjects(node);
  }

  // Sort from farthest to closest.
  std::sort(visible_objects_.begin(), visible_objects_.end(), 
    [] (const auto& lhs, const auto& rhs) {
    return lhs->distance > rhs->distance;
  });

  return visible_objects_;
}

bool Renderer::CullObject(shared_ptr<GameObject> obj, 
  const vector<vector<Polygon>>& occluder_convex_hulls) {
  if (obj->name == "hand-001") return true;
  if (obj->name == "scepter-001") return true;
  if (obj->name == "skydome") return true;
  if (obj->name == "map-001") return true;
  if (obj->never_cull) return false;
  if (obj->IsInvisible() && !resources_->GetConfigs()->see_invisible) 
    return true;
  if (obj->IsSecret() && resources_->GetConfigs()->see_invisible) 
    return true;
  // TODO: draw hand without object.

  if (!obj->draw) {
    return true;
  }

  if (obj->Is3dParticle() && obj->life <= 0) {
    return true;
  }

  // Frustum cull.
  BoundingSphere bounding_sphere = obj->GetTransformedBoundingSphere();
  if (!CollideSphereFrustum(bounding_sphere, frustum_planes_, camera_.position)) {
    return true;
  }

  // Distance cull.
  obj->distance = length(camera_.position - obj->position);
  if (resources_->GetConfigs()->render_scene == "dungeon" && 
      obj->distance > resources_->GetConfigs()->light_radius + 5) { 
    return true;
  }

  float size_in_camera = bounding_sphere.radius / obj->distance;
  if (obj->type != GAME_OBJ_MISSILE && size_in_camera < 0.005f) {
    return true;
  }

  // TODO: fix IsInConvexHull
  // Occlusion cull.
  // for (auto& ch : occluder_convex_hulls) {
  //   if (IsInConvexHull(bounding_sphere, ch)) {
  //   // if (IsInConvexHull(obj->aabb, ch)) {
  //     return true;
  //   }
  // }
  return false;
}

vector<ObjPtr> Renderer::GetVisibleObjectsInSector(
  shared_ptr<Sector> sector, vec4 frustum_planes[6]) {
  vector<ObjPtr> visible_objects;

  vector<shared_ptr<GameObject>> objs =
    GetVisibleObjectsFromSector(sector);

  vector<shared_ptr<GameObject>> darkness_objs;
  vector<shared_ptr<GameObject>> transparent_objs;
  vector<shared_ptr<GameObject>> particle_3d_objs;
  GLuint transparent_shader = resources_->GetShader("transparent_object");
  GLuint animated_transparent_shader = resources_->GetShader("animated_transparent_object");
  GLuint lake_shader = resources_->GetShader("mana_pool");
  GLuint region_shader = resources_->GetShader("region");

  // Occlusion culling
  // https://www.gamasutra.com/view/feature/2979/rendering_the_great_outdoors_fast_.php?print=1
  vector<vector<Polygon>> occluder_convex_hulls;
  for (auto& obj : objs) {
    if (obj->IsPartiallyTransparent()) { 
      transparent_objs.push_back(obj);
    } else if (obj->Is3dParticle()) {
      particle_3d_objs.push_back(obj);
    } else if (obj->IsDarkness()) {
      darkness_objs.push_back(obj);
    } else {
      visible_objects.push_back(obj);
    }
  }

  if (!resources_->GetConfigs()->darkvision) {
    for (int i = darkness_objs.size() - 1; i >= 0; i--) {
      visible_objects.push_back(darkness_objs[i]);
    }
  }

  for (int i = transparent_objs.size() - 1; i >= 0; i--) {
    visible_objects.push_back(transparent_objs[i]);
  }

  for (int i = particle_3d_objs.size() - 1; i >= 0; i--) {
    visible_objects.push_back(particle_3d_objs[i]);
  }
  return visible_objects;
}

// Portal culling 
// http://di.ubi.pt/~agomes/tjv/teoricas/07-culling.pdf
vector<ObjPtr> Renderer::GetVisibleObjectsInPortal(shared_ptr<Portal> p, 
  shared_ptr<StabbingTreeNode> node,
  vec4 frustum_planes[6]) {
  if (p->cave) return {};

  BoundingSphere sphere = BoundingSphere(camera_.position, 2.5f);
  bool in_frustum = false; 

  const string mesh_name = p->GetAsset()->lod_meshes[0];
  shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
  if (!mesh) {
    throw runtime_error(string("Mesh ") + mesh_name + " does not exist.");
  }

  cout << "Testing portal name" << p->name << endl;

  for (auto& poly : mesh->polygons) {
    vec3 portal_point = p->position + poly.vertices[0];
    vec3 normal = poly.normal;

    // if (TestSphereTriangleIntersection(sphere - p->position, poly.vertices)) {
    //   in_frustum = true;
    //   break;
    // }

    // if (dot(normalize(camera_.position - portal_point), normal) < 0.000001f) {
    //   continue;
    // }

    if (CollideTriangleFrustum(poly.vertices, frustum_planes_,
      camera_.position - p->position)) {
      in_frustum = true;
      break;
    }
  }

  // TODO: check other types of culling.
  if (!in_frustum) {
    return {};
  }

  if (node->sector->name == "outside") {
    clip_terrain_ = true;

    const string mesh_name = p->mesh_name;
    shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
    if (!mesh) {
      throw runtime_error(string("Mesh ") + mesh_name + " does not exist.");
    }

    Polygon& poly = mesh->polygons[0];
    terrain_clipping_point_ = poly.vertices[0] + p->position;
    terrain_clipping_normal_ = poly.normal;
  }

  return GetVisibleObjectsInStabbingTreeNode(node, frustum_planes);
}

vector<ObjPtr> Renderer::GetVisibleObjectsInCaves(
  shared_ptr<StabbingTreeNode> stabbing_tree_node, vec4 frustum_planes[6]) {
  vector<ObjPtr> visible_objects;

  shared_ptr<Sector> s = stabbing_tree_node->sector;
  for (auto& node : stabbing_tree_node->children) {
    if (s->portals.find(node->sector->id) == s->portals.end()) {
      throw runtime_error(string("Sector ") + s->name + 
        " should have portal to " + node->sector->name);
    }

    // TODO: cull portal.
    shared_ptr<Portal> p = s->portals[node->sector->id];
    if (!p->cave) continue;

    // vector<ObjPtr> objs = GetVisibleObjectsInStabbingTreeNode(node, frustum_planes);
    // visible_objects.insert(visible_objects.end(), objs.begin(), objs.end()); 

    if (!p->to_sector) {
      throw runtime_error(string("Portal ") + p->name + 
        " does not have a to sector.");
    }

    for (const auto& [id, obj] : p->to_sector->objects) {
      visible_objects.push_back(obj);
    }

    visible_objects.push_back(p);
  }
  return visible_objects;
}

vector<ObjPtr> Renderer::GetVisibleObjectsInStabbingTreeNode(
  shared_ptr<StabbingTreeNode> stabbing_tree_node, vec4 frustum_planes[6]) {
  if (!stabbing_tree_node) return {};

  vector<ObjPtr> visible_objects;
  shared_ptr<Sector> sector = stabbing_tree_node->sector;
  if (sector->name == "outside") {
    vector<ObjPtr> objs = GetVisibleObjectsInCaves(stabbing_tree_node, 
      frustum_planes);
    visible_objects.insert(visible_objects.end(), objs.begin(), objs.end()); 

    shared_ptr<Configs> configs = resources_->GetConfigs();
    visible_objects.push_back(nullptr); // Means draw terrain.
  }

  vector<ObjPtr> objs = GetVisibleObjectsInSector(sector, frustum_planes);
  visible_objects.insert(visible_objects.end(), objs.begin(), objs.end());

  for (auto& node : stabbing_tree_node->children) {
    if (sector->portals.find(node->sector->id) == sector->portals.end()) {
      throw runtime_error("Sector should have portal.");
    }

    shared_ptr<Portal> p = sector->portals[node->sector->id];
    vector<ObjPtr> objs = GetVisibleObjectsInPortal(p, node, 
      frustum_planes);
    visible_objects.insert(visible_objects.end(), objs.begin(), objs.end());
  }
  return visible_objects;
}

vector<ObjPtr> Renderer::GetVisibleObjects(vec4 frustum_planes[6]) {
  shared_ptr<Sector> sector = 
    resources_->GetSector(camera_.position);

  if (!sector->occlude) {
    sector = resources_->GetSectorByName("outside");
  }

  return GetVisibleObjectsInStabbingTreeNode(sector->stabbing_tree, 
    frustum_planes);
}

void Renderer::DrawOutside() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  if (configs->render_scene == "dungeon") {
    DrawDungeonTiles();
    return;
  }

  ObjPtr player = resources_->GetPlayer();
  ObjPtr skydome = resources_->GetObjectByName("skydome");
  skydome->position = player->position;
  skydome->position.y = -1000;

  DrawObject(skydome);

  // static bool updated_clipmaps = true;
  // if (updated_clipmaps) {
  //   // terrain_->UpdateClipmaps(camera_.position + vec3(100, 0, 100));
  //   terrain_->UpdateClipmaps(camera_.position);
  //   updated_clipmaps = false;
  // }
  terrain_->UpdateClipmaps(camera_.position);

  // if (clip_terrain_) {
  //   terrain_->SetClippingPlane(terrain_clipping_point_, 
  //     terrain_clipping_normal_);
  // }

  mat4 shadow_matrix0 = GetShadowMatrix(true, 0);
  mat4 shadow_matrix1 = GetShadowMatrix(true, 1);
  mat4 shadow_matrix2 = GetShadowMatrix(true, 2);

  // TODO: pass frustum planes.
  terrain_->Draw(camera_, view_matrix_, camera_.position, shadow_matrix0, 
    shadow_matrix1, shadow_matrix2, false, clip_terrain_);
}

void Renderer::Draw3dParticle(shared_ptr<Particle> obj) {
  if (obj == nullptr) return;

  shared_ptr<Particle> particle = obj;
  shared_ptr<GameAsset> asset = obj->GetAsset();

  string mesh_name = obj->mesh_name;
  if (!obj->existing_mesh_name.empty()) {
    mesh_name = obj->existing_mesh_name;
  }

  shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
  if (!mesh) {
    int lod = glm::clamp(int(obj->distance / LOD_DISTANCE), 0, 4);
    for (; lod >= 0; lod--) {
      if (!asset->lod_meshes[lod].empty()) {
        break;
      }
    }

    const string mesh_name = asset->lod_meshes[lod];
    mesh = resources_->GetMeshByName(mesh_name);
  }

  GLuint program_id = resources_->GetShader("3d_particle");
  glUseProgram(program_id);

  glBindVertexArray(mesh->vao_);
  mat4 ModelMatrix = translate(mat4(1.0), obj->position);
  ModelMatrix = ModelMatrix * obj->rotation_matrix;

  float scale = obj->scale;
  if (particle->scale_in < 1.0f) {
    scale = asset->scale * particle->scale_in;
  } else if (particle->life <= 0.0f && particle->scale_out > 0.0f) {
    scale = asset->scale * particle->scale_out;
  }

  ModelMatrix = glm::scale(ModelMatrix, vec3(scale));

  mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
  mat4 MVP = projection_matrix_ * ModelViewMatrix;
  glUniformMatrix4fv(GetUniformId(program_id, "MVP"), 1, GL_FALSE, &MVP[0][0]);
  glUniformMatrix4fv(GetUniformId(program_id, "M"), 1, GL_FALSE, &ModelMatrix[0][0]);
  glUniformMatrix4fv(GetUniformId(program_id, "V"), 1, GL_FALSE, &view_matrix_[0][0]);
  mat3 ModelView3x3Matrix = mat3(ModelViewMatrix);
  glUniformMatrix3fv(GetUniformId(program_id, "MV3x3"), 1, GL_FALSE, &ModelView3x3Matrix[0][0]);

  glUniform3fv(GetUniformId(program_id, "camera_pos"), 1,
    (float*) &camera_.position);

  glUniform3fv(GetUniformId(program_id, "player_pos"), 1,
    (float*) &resources_->GetPlayer()->position);

  glUniform1f(GetUniformId(program_id, "light_radius"), resources_->GetConfigs()->light_radius);

  if (!obj->active_animation.empty()) {
    glUniform1f(GetUniformId(program_id, "enable_animation"), 1);
    vector<mat4> joint_transforms;
    if (mesh->animations.find(obj->active_animation) != mesh->animations.end()) {
      const Animation& animation = mesh->animations[obj->active_animation];
      for (int i = 0; i < animation.keyframes[obj->animation_frame].transforms.size(); i++) {
        joint_transforms.push_back(animation.keyframes[obj->animation_frame].transforms[i]);
      }
    } else {
      ThrowError("Animation ", obj->active_animation, " for object ",
        obj->name, " and asset ", asset->name, " does not exist");
    }

    glUniformMatrix4fv(GetUniformId(program_id, "joint_transforms"), 
      joint_transforms.size(), GL_FALSE, &joint_transforms[0][0][0]);
  } else {
    glUniform1f(GetUniformId(program_id, "enable_animation"), 0);
  }

  GLuint texture_id = 0;
  if (obj->texture_id != 0) {
    texture_id = obj->texture_id;
  } else if (!asset->textures.empty()) {
    texture_id = asset->textures[0];
  }

  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glUniform1i(GetUniformId(program_id, "texture_sampler"), 0);

  glUniform2fv(GetUniformId(program_id, "tile_pos"), 1, (float*) &particle->tile_pos);
  glUniform1f(GetUniformId(program_id, "tile_size"), particle->tile_size);

  glDrawArrays(GL_TRIANGLES, 0, mesh->num_indices);
  glDisable(GL_BLEND);
  glBindVertexArray(0);
}

// TODO: split into functions for each shader.
void Renderer::DrawObject(shared_ptr<GameObject> obj, int mode) {
  if (obj == nullptr) return;

  if (mode == 1 && obj->IsCreature()) return;
  if (mode == 2 && !obj->IsCreature()) return;
  if (mode == 3 && !obj->IsCreature()) return;

  shared_ptr<Configs> configs = resources_->GetConfigs();
  // if (obj->type == GAME_OBJ_PORTAL) {
  //   // TODO: draw portal.
  //   return;
  // }

  if (mode == 2 && configs->detect_monsters && obj->IsCreature()) {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
  } else {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); 
    glDepthMask(GL_TRUE);
  }

  if (obj->Is3dParticle()) {
    return Draw3dParticle(static_pointer_cast<Particle>(obj));
  }

  for (shared_ptr<GameAsset> asset : obj->asset_group->assets) {
    int lod = glm::clamp(int(obj->distance / LOD_DISTANCE), 0, 4);
    for (; lod >= 0; lod--) {
      if (!asset->lod_meshes[lod].empty()) {
        break;
      }
    }

    const string mesh_name = asset->lod_meshes[lod];
    shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
    if (!mesh) {
      throw runtime_error(string("Mesh ") + mesh_name + " does not exist.");
    }

    GLuint program_id = asset->shader;

    bool dying = (obj->IsCreature() && obj->life <= 0.0f);
    if (obj->type == GAME_OBJ_ACTIONABLE) {
      shared_ptr<Actionable> actionable = static_pointer_cast<Actionable>(obj);
      if (actionable->state > 0 && actionable->GetAsset()->name == "large_chest_wood") {
        dying = true;
      }
    }

    if (dying) {
      program_id = resources_->GetShader("death");
    } else if (mode == 2 && configs->detect_monsters && obj->IsCreature()) {
      // program_id = resources_->GetShader("death");
      program_id = resources_->GetShader("detect_monster");
    }

    glUseProgram(program_id);

    glBindVertexArray(mesh->vao_);
    mat4 ModelMatrix = translate(mat4(1.0), obj->position);
    ModelMatrix = ModelMatrix * obj->rotation_matrix;

    float scale = asset->scale;
    if (obj->scale_in < 1.0f) {
      scale = asset->scale * obj->scale_in;
    } else if (obj->life <= 0.0f && obj->scale_out > 0.0f) {
      scale = asset->scale * obj->scale_out;
    }

    ModelMatrix = glm::scale(ModelMatrix, vec3(scale));

    mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
    mat4 MVP = projection_matrix_ * ModelViewMatrix;
    glUniformMatrix4fv(GetUniformId(program_id, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(GetUniformId(program_id, "M"), 1, GL_FALSE, &ModelMatrix[0][0]);
    glUniformMatrix4fv(GetUniformId(program_id, "V"), 1, GL_FALSE, &view_matrix_[0][0]);
    mat3 ModelView3x3Matrix = mat3(ModelViewMatrix);
    glUniformMatrix3fv(GetUniformId(program_id, "MV3x3"), 1, GL_FALSE, &ModelView3x3Matrix[0][0]);

    mat4 shadow_matrix = GetShadowMatrix(true/*bias*/, 0);
    mat4 DepthMVP = shadow_matrix * ModelMatrix;
    glUniformMatrix4fv(GetUniformId(program_id, "DepthMVP"), 1, GL_FALSE, &DepthMVP[0][0]);

    glUniform3fv(GetUniformId(program_id, "camera_pos"), 1,
      (float*) &camera_.position);

    glUniform3fv(GetUniformId(program_id, "light_direction"), 1,
      (float*) &configs->sun_position);

    vec3 light_color = vec3(1, 1, 1);
    glUniform3fv(GetUniformId(program_id, "lighting_color"), 1,
      (float*) &light_color);
    // if (obj->current_sector) {
    //   glUniform3fv(GetUniformId(program_id, "lighting_color"), 1,
    //     (float*) &obj->current_sector->lighting_color);

    //   vec3 light_color = vec3(0.3);
    //   glUniform3fv(GetUniformId(program_id, "lighting_color"), 1,
    //     (float*) &light_color);
    // }

    glUniform1f(GetUniformId(program_id, "light_radius"), configs->light_radius);

    if (configs->render_scene == "dungeon") {
      glUniform1f(GetUniformId(program_id, "outdoors"), 0);
      glUniform1f(GetUniformId(program_id, "in_dungeon"), 1);
    } else {
      glUniform1f(GetUniformId(program_id, "outdoors"), 1);
      glUniform1f(GetUniformId(program_id, "in_dungeon"), 0);
    }

    glUniform3fv(GetUniformId(program_id, "player_pos"), 1,
      (float*) &resources_->GetPlayer()->position);

    glUniform1f(GetUniformId(program_id, "normal_strength"), 
      asset->normal_strength);

    glUniform1f(GetUniformId(program_id, "specular_component"), 
      asset->specular_component);

    // vector<ObjPtr> light_points = resources_->GetKClosestLightPoints( 
    //   obj->position, 3);

    vector<ObjPtr> light_points = obj->closest_lights;
    for (int i = 0; i < 3; i++) {
      vec3 position = vec3(0, 0, 0);
      vec3 light_color = vec3(0, 0, 0);
      float quadratic = 99999999.0f;
      if (i < light_points.size()) {
        position = light_points[i]->position;
        if (light_points[i]->override_light) {
          light_color = light_points[i]->light_color;
          quadratic = light_points[i]->quadratic;
          position.y += 10;
        } else {
          shared_ptr<GameAsset> asset = light_points[i]->GetAsset();
          light_color = asset->light_color;
          quadratic = asset->quadratic;
        }
      }

      string p = string("point_lights[") + boost::lexical_cast<string>(i) + "].";
      GLuint glsl_pos = GetUniformId(program_id, p + "position");
      GLuint glsl_diffuse = GetUniformId(program_id, p + "diffuse");
      GLuint glsl_quadratic = GetUniformId(program_id, p + "quadratic");
      glUniform3fv(glsl_pos, 1, (float*) &position);
      glUniform3fv(glsl_diffuse, 1, (float*) &light_color);
      glUniform1f(glsl_quadratic, quadratic);
    }

    // TODO: implement multiple textures.
    GLuint texture_id = 0;
    if (!asset->textures.empty()) {
      texture_id = asset->textures[0];
    }

    if (program_id == resources_->GetShader("animated_object") ||
        program_id == resources_->GetShader("outdoor_animated_object") ||
        program_id == resources_->GetShader("animated_object_noshadow")) {
      glDisable(GL_CULL_FACE);
      vector<mat4> joint_transforms;
      if (mesh->animations.find(obj->active_animation) != mesh->animations.end()) {
        const Animation& animation = mesh->animations[obj->active_animation];
        for (int i = 0; i < animation.keyframes[obj->frame].transforms.size(); i++) {
          joint_transforms.push_back(animation.keyframes[obj->frame].transforms[i]);
        }
      } else {
        ThrowError("Animation ", obj->active_animation, " for object ",
          obj->name, " and asset ", asset->name, " does not exist");
      }

      glUniformMatrix4fv(GetUniformId(program_id, "joint_transforms"), 
        joint_transforms.size(), GL_FALSE, &joint_transforms[0][0][0]);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, texture_id);
      glUniform1i(GetUniformId(program_id, "texture_sampler"), 0);

      if (asset->bump_map_id == 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glUniform1i(GetUniformId(program_id, "bump_map_sampler"), 1);

        glUniform1i(GetUniformId(program_id, "enable_bump_map"), 0);
      } else {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, asset->bump_map_id);
        glUniform1i(GetUniformId(program_id, "bump_map_sampler"), 1);

        glUniform1i(GetUniformId(program_id, "enable_bump_map"), 1);
      }

      if (asset->specular_id != 0) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, asset->specular_id);
        glUniform1i(GetUniformId(program_id, "specular_sampler"), 2);
      }

      glDrawArrays(GL_TRIANGLES, 0, mesh->num_indices);
    } else if (program_id == resources_->GetShader("animated_transparent_object")) {
      glDisable(GL_CULL_FACE);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      vector<mat4> joint_transforms;
      if (mesh->animations.find(obj->active_animation) != mesh->animations.end()) {
        const Animation& animation = mesh->animations[obj->active_animation];
        if (obj->frame > animation.keyframes.size()) {
          ThrowError("Frame ", obj->frame, " outside the scope of animation ",
          obj->active_animation, " for object ", obj->name, " which has ",
          animation.keyframes.size(), " frames");
        }

        for (int i = 0; i < animation.keyframes[obj->frame].transforms.size(); i++) {
          joint_transforms.push_back(animation.keyframes[obj->frame].transforms[i]);
        }
      } else {
        ThrowError("Animation ", obj->active_animation, " for object ",
          obj->name, " and asset ", asset->name, " does not exist");
      }

      glUniformMatrix4fv(GetUniformId(program_id, "joint_transforms"), 
        joint_transforms.size(), GL_FALSE, &joint_transforms[0][0][0]);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, texture_id);
      glUniform1i(GetUniformId(program_id, "texture_sampler"), 0);

      glDrawArrays(GL_TRIANGLES, 0, mesh->num_indices);
      glDisable(GL_BLEND);
    } else if (program_id == resources_->GetShader("web")) {
      glDisable(GL_CULL_FACE);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      vector<mat4> joint_transforms;
      if (mesh->animations.find(obj->active_animation) != mesh->animations.end()) {
        const Animation& animation = mesh->animations[obj->active_animation];
        if (obj->frame > animation.keyframes.size()) {
          ThrowError("Frame ", obj->frame, " outside the scope of animation ",
          obj->active_animation, " for object ", obj->name, " which has ",
          animation.keyframes.size(), " frames");
        }

        for (int i = 0; i < animation.keyframes[obj->frame].transforms.size(); i++) {
          joint_transforms.push_back(animation.keyframes[obj->frame].transforms[i]);
        }
      } else {
        ThrowError("Animation ", obj->active_animation, " for object ",
          obj->name, " and asset ", asset->name, " does not exist");
      }

      glUniformMatrix4fv(GetUniformId(program_id, "joint_transforms"), 
        joint_transforms.size(), GL_FALSE, &joint_transforms[0][0][0]);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, texture_id);
      glUniform1i(GetUniformId(program_id, "texture_sampler"), 0);

      GLuint noise_texture_id = resources_->GetTextureByName("noise");
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, noise_texture_id);
      glUniform1i(GetUniformId(program_id, "noise_sampler"), 1);

      glUniform3fv(GetUniformId(program_id, "center"), 1,
        (float*) &obj->position);

      glDrawArrays(GL_TRIANGLES, 0, mesh->num_indices);
      glDisable(GL_BLEND);
    } else if (program_id == resources_->GetShader("object") ||
               program_id == resources_->GetShader("outdoor_object")) {
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, texture_id);
      glUniform1i(GetUniformId(program_id, "texture_sampler"), 0);

      if (asset->bump_map_id == 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glUniform1i(GetUniformId(program_id, "bump_map_sampler"), 1);

        glUniform1i(GetUniformId(program_id, "enable_bump_map"), 0);
      } else {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, asset->bump_map_id);
        glUniform1i(GetUniformId(program_id, "bump_map_sampler"), 1);

        glUniform1i(GetUniformId(program_id, "enable_bump_map"), 1);
      }

      if (asset->specular_id != 0) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, asset->specular_id);
        glUniform1i(GetUniformId(program_id, "specular_sampler"), 2);
      }

      glDrawArrays(GL_TRIANGLES, 0, mesh->num_indices);
    } else if (program_id == resources_->GetShader("transparent_object")) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, texture_id);
      glUniform1i(GetUniformId(program_id, "texture_sampler"), 0);

      glDrawArrays(GL_TRIANGLES, 0, mesh->num_indices);
      glDisable(GL_BLEND);
    } else if (program_id == resources_->GetShader("noshadow_object")) {
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, texture_id);
      glUniform1i(GetUniformId(program_id, "texture_sampler"), 0);

      glDrawArrays(GL_TRIANGLES, 0, mesh->num_indices);
    } else if (program_id == resources_->GetShader("hypercube")) {
      glDisable(GL_CULL_FACE);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDrawArrays(GL_TRIANGLES, 0, mesh->num_indices);
      glDisable(GL_BLEND);
    } else if (program_id == resources_->GetShader("region")) {
      glDisable(GL_CULL_FACE);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDrawArrays(GL_TRIANGLES, 0, mesh->num_indices);
      glDisable(GL_BLEND);
    } else if (program_id == resources_->GetShader("mana_pool")) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, texture_id);
      glUniform1i(GetUniformId(program_id, "dudv_map"), 0);

      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, texture_id);
      glUniform1i(GetUniformId(program_id, "normal_map"), 1);

      vec3 light_position_worldspace = vec3(0, 1, 0);
      glUniform3fv(GetUniformId(program_id, "light_position_worldspace"), 1,
        (float*) &light_position_worldspace);

      static float move_factor = 0.0f;
      move_factor += 0.0005f;
      const vec3 player_pos = resources_->GetPlayer()->position;
      glUniform3fv(GetUniformId(program_id, "camera_position"), 1, (float*) &player_pos);
      glUniform1f(GetUniformId(program_id, "move_factor"), move_factor);
      glDrawArrays(GL_TRIANGLES, 0, mesh->num_indices);
      glDisable(GL_BLEND); 
    } else if (program_id == resources_->GetShader("death") ||
              program_id == resources_->GetShader("detect_monster")) {
      glUniform1f(GetUniformId(program_id, "enable_animation"), 1);
      vector<mat4> joint_transforms;
      Animation& animation = mesh->animations[obj->active_animation];
      if (mesh->animations.find(obj->active_animation) != mesh->animations.end()) {
        const Animation& animation = mesh->animations[obj->active_animation];
        for (int i = 0; i < animation.keyframes[obj->frame].transforms.size(); i++) {
          joint_transforms.push_back(animation.keyframes[obj->frame].transforms[i]);
        }

        glUniformMatrix4fv(GetUniformId(program_id, "joint_transforms"), 
          joint_transforms.size(), GL_FALSE, &joint_transforms[0][0][0]);

        float dissolve_value = obj->frame / (float) animation.keyframes.size();
        glUniform1f(GetUniformId(program_id, "dissolve_value"), dissolve_value);

        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glUniform1i(GetUniformId(program_id, "texture_sampler"), 0);

        if (asset->bump_map_id == 0) {
          glActiveTexture(GL_TEXTURE1);
          glBindTexture(GL_TEXTURE_2D, texture_id);
          glUniform1i(GetUniformId(program_id, "bump_map_sampler"), 1);

          glUniform1i(GetUniformId(program_id, "enable_bump_map"), 0);
        } else {
          glActiveTexture(GL_TEXTURE1);
          glBindTexture(GL_TEXTURE_2D, asset->bump_map_id);
          glUniform1i(GetUniformId(program_id, "bump_map_sampler"), 1);

          glUniform1i(GetUniformId(program_id, "enable_bump_map"), 1);
        }

        if (asset->specular_id != 0) {
          glActiveTexture(GL_TEXTURE2);
          glBindTexture(GL_TEXTURE_2D, asset->specular_id);
          glUniform1i(GetUniformId(program_id, "specular_sampler"), 2);
        }

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, resources_->GetTextureByName("dissolve"));
        glUniform1i(GetUniformId(program_id, "mask_sampler"), 3);

        glDrawArrays(GL_TRIANGLES, 0, mesh->num_indices);
        glDisable(GL_BLEND);
        glBindVertexArray(0);
      } else {
        ThrowError("Animation ", obj->active_animation, " for object ",
          obj->name, " and asset ", asset->name, " does not exist");
      }
    } else {
      if (program_id == resources_->GetShader("sky")) {
        shared_ptr<Configs> configs = resources_->GetConfigs();
        const vec3 sun_position = configs->sun_position;
        glUniform3f(GetUniformId(program_id, "sun_position"), 
          sun_position.x, sun_position.y, sun_position.z);

        const vec3 player_pos = resources_->GetPlayer()->position;
        glUniform3f(GetUniformId(program_id, "player_position"), 
          player_pos.x, player_pos.y, player_pos.z);
      }

      glDisable(GL_CULL_FACE);
      glDrawElements(GL_TRIANGLES, mesh->num_indices, GL_UNSIGNED_INT, nullptr);
    }

    glBindVertexArray(0);
  }
  glBindVertexArray(0);
}

void Renderer::DrawObjects(vector<ObjPtr> objs, int mode) {
  glDisable(GL_CULL_FACE);
  for (auto& obj : objs) {
    if (!obj) { // Terrain.
      DrawOutside();
    } else if (obj->type == GAME_OBJ_PORTAL) {
      // TODO: to display particles inside the sector.
      // DrawParticles();
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      DrawObject(obj, mode);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    } else {
      DrawObject(obj, mode);
    }
  }

  DrawParticles();
  glEnable(GL_CULL_FACE);
}

void Renderer::DrawHypercube() {
  glViewport(0, 0, window_width_, window_height_);
  glClearColor(0, 0, 0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  static vector<float> hypercube_rotation { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

  projection_matrix_ = glm::perspective(glm::radians(FIELD_OF_VIEW), 
    4.0f / 3.0f, NEAR_CLIPPING, FAR_CLIPPING);

  view_matrix_ = glm::lookAt(
    vec3(0, 0, 0), // Camera is here
    vec3(0, 0, 1), // Look here.
    vec3(0, 1, 0)  // Up. 
  );

  project_4d_->CreateHypercube(vec3(0, 0, 20), hypercube_rotation);
  hypercube_rotation[0] += 0.02;
  hypercube_rotation[1] += 0.02;

  ObjPtr* cubes = project_4d_->GetCubes();
  for (int i = 0; i < 8; i++) {
    ObjPtr obj = cubes[i];
    if (!obj) continue;
    DrawObject(obj);
  }

  glClear(GL_DEPTH_BUFFER_BIT);
}

void Renderer::Draw() {
  shared_ptr<Configs> configs = resources_->GetConfigs();

  if (configs->update_renderer) {
    configs->update_renderer = false;
    CreateDungeonBuffers();
  }

  projection_matrix_ = glm::perspective(glm::radians(FIELD_OF_VIEW), 
    4.0f / 3.0f, NEAR_CLIPPING, FAR_CLIPPING);

  // https://www.3dgep.com/understanding-the-view-matrix/#:~:text=The%20view%20matrix%20is%20used,things%20are%20the%20same%20thing!&text=The%20View%20Matrix%3A%20This%20matrix,of%20the%20camera's%20transformation%20matrix.
  view_matrix_ = glm::lookAt(
    camera_.position,                     // Camera is here
    camera_.position + camera_.direction, // and looks here : at the same position, plus "direction"
    camera_.up                            // Head is up (set to vec3(0, -1, 0) to look upside-down)
  );

  if (configs->override_camera_pos) {
    view_matrix_ = glm::lookAt(
      configs->camera_pos,
      configs->camera_pos + camera_.direction, // and looks here : at the same position, plus "direction"
      vec3(0, 1, 0)
    );
  }

  // DrawShadows();

  vec3 clear_color = vec3(0.73, 0.81, 0.92);
  if (configs->render_scene == "dungeon") {
    clear_color = vec3(0);
  }

  glViewport(0, 0, window_width_, window_height_);
  glClearColor(clear_color.x, clear_color.y, clear_color.z, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  clip_terrain_ = false;
  GetFrustumPlanes(frustum_planes_);
  vector<ObjPtr> objects = GetVisibleObjects(frustum_planes_);

  if (configs->render_scene == "dungeon") {
    shared_ptr<Player> player = resources_->GetPlayer();
    Dungeon& dungeon = resources_->GetDungeon();
    ivec2 player_tile = dungeon.GetDungeonTile(player->position);

    if (configs->detect_monsters) {
      DrawObjects(objects, 1); // Draw non creatures.
      DrawObjects(objects, 2); // Draw detect shader.
      DrawObjects(objects, 3); // Draw creatures.
    } else {
      DrawObjects(objects, 0);
    }
  } else {
    DrawObjects(objects);
  }

  glClear(GL_DEPTH_BUFFER_BIT);

  DrawHand();
  DrawScreenEffects();

  glClear(GL_DEPTH_BUFFER_BIT);
  if (resources_->GetGameState() == STATE_MAP) {
    DrawMap();
  }
}

// ==========================
//  Shadows
// ==========================
mat4 Renderer::GetShadowMatrix(bool bias, int level) {
  vec3 sphere_center = cascade_shadows_[level].bounding_sphere.center;
  float sphere_radius = cascade_shadows_[level].bounding_sphere.radius;

  // float size = 100;
  float size = sphere_radius;
  mat4 projection_matrix = ortho<float>(-size, size, -size, size, 0, 1000);

  shared_ptr<Configs> configs = resources_->GetConfigs();

  vec3 sun_dir = normalize(configs->sun_position);
  vec3 light_pos = sphere_center + sun_dir * 100.0f;

  mat4 view_matrix = lookAt(
    light_pos,
    sphere_center,
    // camera_.up
    vec3(0, 1, 0)
  );

  const mat4 bias_matrix (
    0.5, 0.0, 0.0, 0.0, 
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 0.5, 0.0,
    0.5, 0.5, 0.5, 1.0
  );

  if (bias) {
    return bias_matrix * projection_matrix * view_matrix;
  } else {
    return projection_matrix * view_matrix;
  }
}

void Renderer::DrawObjectShadow(shared_ptr<GameObject> obj, int level) {
  for (shared_ptr<GameAsset> asset : obj->asset_group->assets) {
    int lod = glm::clamp(int(obj->distance / LOD_DISTANCE), 0, 4);
    for (; lod >= 0; lod--) {
      if (!asset->lod_meshes[lod].empty()) {
        break;
      }
    }

    const string mesh_name = asset->lod_meshes[lod];
    shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
    if (!mesh) {
      throw runtime_error(string("Mesh ") + mesh_name + " does not exist.");
    }

    GLuint program_id;

    bool is_animated = (
      asset->shader == resources_->GetShader("animated_object") ||
      asset->shader == resources_->GetShader("animated_transparent_object"));

    bool in_dungeon = resources_->GetConfigs()->render_scene == "dungeon";

    if (is_animated && obj->IsCreature()) {
      program_id = resources_->GetShader("animated_shadow");
    } else if (in_dungeon && obj->IsItem()) {
      if (obj->asset_group->name != "dungeon_platform_up" &&
          obj->asset_group->name != "dungeon_platform_down" &&
          obj->asset_group->name != "chest") {
        program_id = resources_->GetShader("shadow");
      } else {
        continue;
      }
    } else if (!in_dungeon) {
      program_id = resources_->GetShader("shadow");
    } else {
      continue;
    }
 
    glUseProgram(program_id);
    glBindVertexArray(mesh->vao_);

    // Check if animated object.
    mat4 projection_view_matrix = GetShadowMatrix(false, level);
    mat4 model_matrix = translate(mat4(1.0), obj->position);
    model_matrix = model_matrix * obj->rotation_matrix;
    mat4 MVP = projection_view_matrix * model_matrix;

    glUniformMatrix4fv(GetUniformId(program_id, "MVP"), 1, GL_FALSE, &MVP[0][0]);

    if (is_animated) {
      vector<mat4> joint_transforms;
      if (mesh->animations.find(obj->active_animation) != mesh->animations.end()) {
        const Animation& animation = mesh->animations[obj->active_animation];
        for (int i = 0; i < animation.keyframes[obj->frame].transforms.size(); i++) {
          joint_transforms.push_back(animation.keyframes[obj->frame].transforms[i]);
        }
      } else {
        ThrowError("Animation ", obj->active_animation, " for object ",
          obj->name, " and asset ", asset->name, " does not exist");
      }

      glUniformMatrix4fv(GetUniformId(program_id, "joint_transforms"), 
        joint_transforms.size(), GL_FALSE, &joint_transforms[0][0][0]);
    }

    glDrawArrays(GL_TRIANGLES, 0, mesh->num_indices);
  }
}

void Renderer::UpdateCascadedShadows() {
  float near3[3] = { NEAR_CLIPPING, FAR_CLIPPING / 100.0f, FAR_CLIPPING / 10.0f };
  float far3[3] = { FAR_CLIPPING / 100.0f, FAR_CLIPPING / 10.0f, FAR_CLIPPING };

  for (int i = 0; i < 3; i++) {
    cascade_shadows_[i].near_range = near3[i];
    cascade_shadows_[i].far_range = far3[i];

    float near = cascade_shadows_[i].near_range; 
    float far = cascade_shadows_[i].far_range; 
    float end = near + far;
 
    vec3 dir = camera_.direction;
    vec3 sphere_center = camera_.position + dir * (near + 0.5f * end);

    // Create a vector to the frustum far corner
    float hypotenuse = sqrt(16.0f * 9.0f);

    float tan_fov_x = 4.0f / hypotenuse;
    float tan_fov_y = 3.0f / hypotenuse;

    vec3 right = cross(camera_.up, camera_.direction);
    vec3 far_corner = dir + right * tan_fov_x + camera_.up * tan_fov_y;

    // Compute the frustumBoundingSphere radius
    vec3 bound_vec = camera_.position + far_corner * far - sphere_center;
    float sphere_radius = length(bound_vec);

    // vec3 cascade_translation = sphere_center - 
    //   cascade_shadows_[0].bounding_sphere.center;

    // float texels_per_meter = 1024.0f / (2.0f * cascade_shadows_[0].bounding_sphere.radius);
    // vec3 cascade_offset = cascade_translation * texels_per_meter;
    // bool update = abs(cascade_offset.x) > 0.5f || abs(cascade_offset.y) > 0.5f 
    //   || abs(cascade_offset.z) > 0.5f;

    // if (update) { 
    //   vec3 offset;
    //   offset.x = floor(cascade_offset.x + 0.5f) / texels_per_meter;
    //   offset.y = floor(cascade_offset.y + 0.5f) / texels_per_meter;
    //   offset.z = floor(cascade_offset.z + 0.5f) / texels_per_meter;

    //   cascade_shadows_[0].bounding_sphere.center += offset;
    //   cascade_shadows_[0].bounding_sphere.radius = sphere_radius;
    // }

    cascade_shadows_[i].bounding_sphere.center = sphere_center;
    cascade_shadows_[i].bounding_sphere.radius = sphere_radius;
  }
}

void Renderer::DrawShadows() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  if (dot(vec3(0, 1, 0), normalize(configs->sun_position)) < 0.0f) {
    return;
  }

  UpdateCascadedShadows();

  GLuint transparent_shader = resources_->GetShader("transparent_object");
  for (int i = 0; i < 3; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_framebuffers_[i]);
    glViewport(0, 0, 1024, 1024);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); 
    // glEnable(GL_CULL_FACE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    clip_terrain_ = false;

    mat4 shadow_matrix = GetShadowMatrix(true, i);

    vec3 sun_dir = normalize(configs->sun_position);
    vec3 position = cascade_shadows_[i].bounding_sphere.center + sun_dir * 100.0f;

    mat4 ModelMatrix = translate(mat4(1.0), position);
    mat4 MVP = shadow_matrix * ModelMatrix;
    ExtractFrustumPlanes(MVP, frustum_planes_);
    vector<ObjPtr> objects = GetVisibleObjects(frustum_planes_);

    glDisable(GL_CULL_FACE);
    for (auto& obj : objects) {
      if (!obj) { // Terrain.
        // DrawOutside();
      } else if (obj->type == GAME_OBJ_PORTAL) {
        // glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        // DrawObject(obj);
        // glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      } else if (obj->GetAsset()->shader != transparent_shader) {
        DrawObjectShadow(obj, i);
      }
    }
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glEnable(GL_CULL_FACE);
}


// ==================
// Static draw
// ==================

void Renderer::DrawStatusBars() {
  draw_2d_->DrawImage("status_bars", 0, 0, 1440, 1440, 1.0);

  shared_ptr<Player> player = resources_->GetPlayer();

  float armor = player->armor;
  float hp_bar_width = player->life / (player->max_life + armor);
  hp_bar_width = (hp_bar_width > 0) ? hp_bar_width : 0;

  // Draw HP / armor bar.
  float armor_width = (player->life + armor) / (player->max_life + armor);
  draw_2d_->DrawImageWithMask("brass", "spellbar_hp_mask", 29, 824, armor_width * 330, 330, 1.0, vec2(0, 0), vec2(armor_width, 1));
  draw_2d_->DrawImageWithMask("red", "spellbar_hp_mask", 29, 824, hp_bar_width * 330, 330, 1.0, vec2(0, 0), vec2(hp_bar_width, 1));
  
  string hp_str = boost::lexical_cast<string>(player->life) + " / " +
    boost::lexical_cast<string>(player->max_life);
  if (armor > 0) { 
    hp_str += " (" + boost::lexical_cast<string>(armor) + ")";
  }

  draw_2d_->DrawText(hp_str, 300, 900 - 818, 
    vec4(1, 1, 1, 1), 1.0, false, "avenir_light_oblique");

  float w, h;

  // Draw stamina.
  float stamina_bar_width = player->stamina / player->max_stamina;
  stamina_bar_width = (stamina_bar_width > 0) ? stamina_bar_width : 0;
  float stamina_recover_bar_width = player->recover_stamina / player->max_stamina;
  stamina_recover_bar_width = (stamina_recover_bar_width > 0) ? stamina_recover_bar_width : 0;

  if (player->recover_stamina > 0.0f) {
    float w = 0.235555 * stamina_recover_bar_width;
    float h = 1.0;
    draw_2d_->DrawImageWithMask("red", "spellbar_stamina_mask", 0, 0, w * 1440, h * 1440, 
      1.0, vec2(0, 0), vec2(w, h));
  } else {
    float w = 0.235555 * stamina_bar_width;
    float h = 1.0;
    draw_2d_->DrawImageWithMask("green", "spellbar_stamina_mask", 0, 0, w * 1440, h * 1440, 
      1.0, vec2(0, 0), vec2(w, h));
  }

  string stamina_str = boost::lexical_cast<string>(int(player->stamina)) + " / " +
    boost::lexical_cast<string>(int(player->max_stamina));
  draw_2d_->DrawText(stamina_str, 240, 900 - 872, 
    vec4(1, 1, 1, 1), 1.0, false, "avenir_light_oblique");

  // Draw mana.
  float mana_bar_width = player->mana / player->max_mana;
  mana_bar_width = (mana_bar_width > 0) ? mana_bar_width : 0;
  draw_2d_->DrawImageWithMask("blue", "mana_bar_mask", 
    1089 + (1 - mana_bar_width) * 329, 824, 
    mana_bar_width * 329, 329, 
    1.0, vec2(1 - mana_bar_width, 0), vec2(mana_bar_width, 1));

  string mana_str = boost::lexical_cast<string>(int(player->mana)) + " / " +
    boost::lexical_cast<string>(player->max_mana);
  draw_2d_->DrawText(mana_str, 1110, 900 - 818, 
    vec4(1, 1, 1, 1), 1.0, false, "avenir_light_oblique");

  shared_ptr<Configs> configs = resources_->GetConfigs();
  int item_id =  configs->spellbar[configs->selected_spell];
  shared_ptr<ArcaneSpellData> arcane_spell =  
    resources_->WhichArcaneSpell(item_id);

  string active_name;
  if (arcane_spell) {
    active_name = arcane_spell->name;
  } else {
    const ItemData& item_data = resources_->GetItemData()[item_id];
    active_name = item_data.name;
  }

  float t;
  vec3 q;
  ObjPtr obj = resources_->IntersectRayObjects(camera_.position, 
    camera_.direction, 50, INTERSECT_ALL, t, q);
  if (obj) {
    stringstream ss;
    ss << obj->name << " (" << obj->GetDisplayName() << ")" << endl;
    draw_2d_->DrawText(ss.str(), 100, 900 - 800, 
      vec4(1, 1, 1, 1), 1.0, false, "avenir_light_oblique");
  }

  if (!active_name.empty()) {
    draw_2d_->DrawText(active_name, 1120, 900 - 870, 
      vec4(1, 1, 1, 1), 1.0, false, "avenir_light_oblique");
  }
}

void Renderer::DrawScreenEffects() {
  const int kWindowWidth = 1440;
  const int kWindowHeight = 900;
  shared_ptr<Configs> configs = resources_->GetConfigs();
  float taking_hit = configs->taking_hit;
  if (taking_hit > 0.0 || resources_->GetPlayer()->status == STATUS_DEAD) {
    draw_2d_->DrawImage("hit-effect", 0, 0, kWindowWidth, kWindowHeight, taking_hit / 30.0f);
  }

  if (configs->fading_out > -60.0) {
    draw_2d_->DrawImage("fade-out", 0, 0, kWindowWidth, kWindowHeight, 
      1.0f - (abs(configs->fading_out) / 60.0f));
  }

  draw_2d_->DrawImage("crosshair", kWindowWidth/2-4, kWindowHeight/2-4, 8, 8, 0.5);

  inventory_->DrawSpellbar();
  DrawStatusBars();

  // Draw messages.
  double cur_time = glfwGetTime();
  int num_msgs = configs->messages.size();
  for (int i = num_msgs-1, y = 0; i >= num_msgs - 5 && i >= 0; i--, y += 20) {
    const string& msg = get<0>(configs->messages[i]);
    const float time_remaining = (get<1>(configs->messages[i]) - cur_time) / 10.0f;
    draw_2d_->DrawText(msg, 25, 800 - 700 + y, 
      vec4(1, 1, 1, time_remaining), 1.0, false, "avenir_light_oblique");
  }

  // Draw level up.
  if (configs->level_up_frame >= 0) {
    int frame = int(configs->level_up_frame);
    auto particle_type = resources_->GetParticleTypeByName("level-up");
    float w = 1.0 / float(particle_type->grid_size);
    float x = (frame % particle_type->grid_size) * w;
    float y = 0.875 - (frame / particle_type->grid_size) * w;
    if (frame >= 0) {
      draw_2d_->DrawImage("level_up", 40,
        700, 128, 128, 1.0, vec2(x, y), vec2(w, w)); 
    }

    configs->level_up_frame += 0.5f;
    if (configs->level_up_frame >= 64) {
      configs->level_up_frame = -1;
    }
  }

  // draw_2d_->DrawImage("level_up", 200,
  //   200, 256, 256, 1.0, vec2(0, 0), vec2(1, 1)); 

  if (configs->interacting_item && resources_->GetGameState() == STATE_GAME) {
    ObjPtr item = configs->interacting_item;

    // TODO: event. On hover item.
    switch (item->type) {
      case GAME_OBJ_DEFAULT: {
        draw_2d_->DrawImage("interact_item", 384, 588, 512, 512, 1.0);
        const string item_name = item->GetAsset()->GetDisplayName();
        draw_2d_->DrawText(string("Pick ") + item_name, 384 + 70, 
          kWindowHeight - 588 - 40, vec4(1), 1.0, false, 
          "avenir_light_oblique");
        break;
      }
      case GAME_OBJ_ACTIONABLE: {
        draw_2d_->DrawImage("interact_item", 384, 588, 512, 512, 1.0);
        const string item_name = item->GetAsset()->GetDisplayName();
        draw_2d_->DrawText(string("Open chest"), 384 + 70, 
          kWindowHeight - 588 - 40, vec4(1), 1.0, false, 
          "avenir_light_oblique");
        break;
      }
      case GAME_OBJ_DOOR: {
        shared_ptr<Door> door = static_pointer_cast<Door>(item);
        if (door->state == 0) {
          draw_2d_->DrawImage("interact_item", 384, 588, 512, 512, 1.0);
          draw_2d_->DrawText("Open door", 384 + 70, kWindowHeight - 588 - 40, vec4(1), 1.0, false, "avenir_light_oblique");
        } else if (door->state == 2) {
          draw_2d_->DrawImage("interact_item", 384, 588, 512, 512, 1.0);
          draw_2d_->DrawText("Close door", 384 + 70, kWindowHeight - 588 - 40, vec4(1), 1.0, false, "avenir_light_oblique");
        } else if (door->state == 4) {
          draw_2d_->DrawText("The door is locked", 384 + 70, kWindowHeight - 588 - 40, vec4(1), 1.0, false, "avenir_light_oblique");
        }
        break;
      }
      default:
        break;
    }
  }

  if (!configs->overlay.empty()) {
    draw_2d_->DrawImage(configs->overlay, 400, 100, 600, 600, 0.1);
  }
}

void Renderer::DrawHand() {
  shared_ptr<GameObject> obj = resources_->GetObjectByName("hand-001");
  DrawObject(obj);

  if (resources_->IsHoldingScepter()) {
    obj = resources_->GetObjectByName("scepter-001");
    DrawObject(obj);
  }
}

void Renderer::DrawMap() {
  shared_ptr<GameObject> obj = resources_->GetObjectByName("map-001");
  shared_ptr<GameAsset> asset = obj->GetAsset();
  shared_ptr<Mesh> mesh = asset->first_mesh;
  if (!mesh) {
    const string mesh_name = asset->lod_meshes[0];
    asset->first_mesh = resources_->GetMeshByName(mesh_name);
    mesh = asset->first_mesh;
  }

  float animation_speed = 1.0f;
  if (obj->asset_group != nullptr) {
    animation_speed = obj->GetAsset()->animation_speed;
  }

  // TODO: store pointer to active animation. (This lookup takes 1.6% of frame time).
  const Animation& animation = mesh->animations[obj->active_animation];
  float d = resources_->GetDeltaTime() / 0.016666f;
  obj->frame += 1.0f * d * animation_speed;
  if (obj->frame >= animation.keyframes.size()) {
    if (obj->GetRepeatAnimation()) {
      obj->frame = 0;
    } else {
      obj->frame = animation.keyframes.size() - 1;
    }
  }

  shared_ptr<Player> player = shared_ptr<Player>(resources_->GetPlayer());
  mat4 rotation_matrix = rotate(
    mat4(1.0),
    player->rotation.y + 4.71f,
    vec3(0.0f, 1.0f, 0.0f)
  );
  rotation_matrix = rotate(
    rotation_matrix,
    player->rotation.x,
    vec3(0.0f, 0.0f, 1.0f)
  );

  obj->rotation_matrix = rotation_matrix;
  obj->position = camera_.position;

  DrawObject(obj);
}

// ===================
// Particles
// ===================

void Renderer::CreateParticleBuffers() {
  static const GLfloat vertex_buffer_data[] = {
    -0.5f, -0.5f, 0.0f,
    0.5f, -0.5f, 0.0f,
    -0.5f, 0.5f, 0.0f,
    0.5f, 0.5f, 0.0f,
  };

  glGenBuffers(1, &particle_vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, particle_vbo_);
  glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), &vertex_buffer_data, 
    GL_STATIC_DRAW);  

  unordered_map<string, shared_ptr<ParticleType>>& particle_types = 
    resources_->GetParticleTypes();
  for (const auto& [name, particle_type] : particle_types) {
    particle_render_data_[name] = ParticleRenderData();
    ParticleRenderData& prd = particle_render_data_[name];
    prd.type = particle_type;

    glGenVertexArrays(1, &prd.vao);
    glBindVertexArray(prd.vao);

    glGenBuffers(1, &prd.position_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, prd.position_buffer);
    glBufferData(GL_ARRAY_BUFFER, kMaxParticles * 4 * sizeof(GLfloat), NULL, 
      GL_STREAM_DRAW);

    glGenBuffers(1, &prd.color_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, prd.color_buffer);
    glBufferData(GL_ARRAY_BUFFER, kMaxParticles * 4 * sizeof(GLfloat), NULL, 
      GL_STREAM_DRAW);

    glGenBuffers(1, &prd.uv_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, prd.uv_buffer);
    glBufferData(GL_ARRAY_BUFFER, kMaxParticles * 2 * sizeof(GLfloat), NULL, 
      GL_STREAM_DRAW);

    glBindVertexArray(0);
  }
}

void Renderer::UpdateParticleBuffers() {
  vector<shared_ptr<Particle>>& particle_container = resources_->GetParticleContainer();
  for (int i = 0; i < kMaxParticles; i++) {
    shared_ptr<Particle> p = particle_container[i];
    if (!p->particle_type || p->life < 0) {
      p->camera_distance = -1;
      continue;
    }
    if (p->particle_type->behavior == PARTICLE_FIXED) {
      p->camera_distance = 0;
      continue;
    }
    p->camera_distance = length2(p->position - camera_.position);
  }
  std::sort(particle_container.begin(), particle_container.end());

  for (auto& [name, prd] : particle_render_data_) {
    prd.count = 0;
  }

  for (int i = 0; i < kMaxParticles; i++) {
    shared_ptr<Particle> p = particle_container[i];
    if (p->life < 0 || !p->particle_type) continue;

    vec2 uv;
    int index = p->frame + p->particle_type->first_frame;

    float tile_size = 1.0f / float(p->particle_type->grid_size);
    uv.x = int(index % p->particle_type->grid_size) * tile_size + tile_size / 2;
    uv.y = (p->particle_type->grid_size - int(index / p->particle_type->grid_size) - 1) * tile_size 
      + tile_size / 2;

    ParticleRenderData& prd = particle_render_data_[p->particle_type->name];
    prd.particle_positions[prd.count] = vec4(p->position, p->size);
    prd.particle_colors[prd.count] = p->color;
    prd.particle_uvs[prd.count] = uv;
    prd.count++;
  }

  for (auto& [name, prd] : particle_render_data_) {
    // Buffer orphaning.
    glBindBuffer(GL_ARRAY_BUFFER, prd.position_buffer);
    glBufferData(GL_ARRAY_BUFFER, kMaxParticles * sizeof(vec4), NULL, 
      GL_STREAM_DRAW); 
    glBufferSubData(GL_ARRAY_BUFFER, 0, prd.count * sizeof(vec4), 
      &prd.particle_positions[0]);
    
    glBindBuffer(GL_ARRAY_BUFFER, prd.color_buffer);
    glBufferData(GL_ARRAY_BUFFER, kMaxParticles * sizeof(vec4), NULL, 
      GL_STREAM_DRAW); 
    glBufferSubData(GL_ARRAY_BUFFER, 0, prd.count * sizeof(vec4), 
      &prd.particle_colors[0]);

    glBindBuffer(GL_ARRAY_BUFFER, prd.uv_buffer);
    glBufferData(GL_ARRAY_BUFFER, kMaxParticles * sizeof(vec2), NULL, 
      GL_STREAM_DRAW); 
    glBufferSubData(GL_ARRAY_BUFFER, 0, prd.count * sizeof(vec2), 
      &prd.particle_uvs[0]);
  }
}

void Renderer::DrawParticles() {
  UpdateParticleBuffers();

  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  for (const auto& [name, prd] : particle_render_data_) {
    glBindVertexArray(prd.vao);

    GLuint program_id = resources_->GetShader("particle");
    glUseProgram(program_id);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, prd.type->texture_id);

    glUniform1i(GetUniformId(program_id, "texture_sampler"), 0);
    
    glUniform3f(GetUniformId(program_id, "camera_right_worldspace"), 
      view_matrix_[0][0], view_matrix_[1][0], view_matrix_[2][0]);
    glUniform3f(GetUniformId(program_id, "camera_up_worldspace"), 
      view_matrix_[0][1], view_matrix_[1][1], view_matrix_[2][1]);

    mat4 VP = projection_matrix_ * view_matrix_;  
    glUniformMatrix4fv(GetUniformId(program_id, "VP"), 1, GL_FALSE, &VP[0][0]);
 
    BindBuffer(particle_vbo_, 0, 3);
    BindBuffer(prd.position_buffer, 1, 4);
    BindBuffer(prd.color_buffer, 2, 4);
    BindBuffer(prd.uv_buffer, 3, 2);

    glVertexAttribDivisor(0, 0); // Always reuse the same 4 vertices.
    glVertexAttribDivisor(1, 1); // One per quad.
    glVertexAttribDivisor(2, 1); // One per quad.
    glVertexAttribDivisor(3, 1); // One per quad.

    float tile_size = 1.0 / float(prd.type->grid_size);
    glUniform1f(GetUniformId(program_id, "tile_size"), tile_size);
    glUniform1i(GetUniformId(program_id, "is_fixed"), 
      (prd.type->behavior == PARTICLE_FIXED) ? 1 : 0);

    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, prd.count);
  }

  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  glBindVertexArray(0);
}

void Renderer::FindVisibleObjectsAsync() {
  while (!terminate_) {
    find_mutex_.lock();
    if (find_tasks_.empty()) {
      find_mutex_.unlock();
      this_thread::sleep_for(chrono::milliseconds(1));
      continue;
    }

    shared_ptr<OctreeNode> node = find_tasks_.front();
    find_tasks_.pop();
    running_tasks_++;
    find_mutex_.unlock();

    GetVisibleObjects(node);

    find_mutex_.lock();
    running_tasks_--;
    find_mutex_.unlock();
  }
}

void Renderer::CreateThreads() {
  for (int i = 0; i < kMaxThreads; i++) {
    // find_threads_.push_back(thread(&Renderer::FindVisibleObjectsAsync, this));
  }
}

// void Renderer::UpdateDungeonBufferMatrices() {
// }

void Renderer::CreateDungeonBuffers() {
  double start_time = glfwGetTime();

  vector<char> instanced_tiles { ' ', '+', '|', 'o', 'd', 'g', 'P', 'c', 's' };

  vector<string> model_names { 
    "resources/models_fbx/dungeon_floor.fbx", 
    "resources/models_fbx/dungeon_corner.fbx", 
    "resources/models_fbx/dungeon_wall.fbx", 
    "resources/models_fbx/dungeon_arch.fbx",
    "resources/models_fbx/dungeon_door.fbx",
    "resources/models_fbx/dungeon_arch_gate.fbx",
    "resources/models_fbx/dungeon_pillar.fbx",
    "resources/models_fbx/dungeon_ceiling.fbx",
    "resources/models_fbx/dungeon_floor.fbx",
    "resources/models_fbx/dungeon_floor.fbx",
  };

  vector<string> texture_names { 
    "paving", 
    "cathedral_wall_diffuse", 
    "cathedral_wall_diffuse", 
    "cathedral_wall_diffuse", 
    "cathedral_wall_diffuse", 
    "metal_diffuse",
    "cathedral_wall_diffuse",
    "granite_wall_diffuse",
    "medieval_floor_diffuse",
   };

  vector<string> normal_texture_names { 
    "paving_normal", 
    "cathedral_wall_normal", 
    "cathedral_wall_normal", 
    "cathedral_wall_normal", 
    "cathedral_wall_normal", 
    "metal_normal",
    "cathedral_wall_normal", 
    "granite_wall_normal", 
    "medieval_floor_normal",
  };

  vector<string> specular_texture_names { 
    "paving_roughness", 
    "cathedral_wall_roughness", 
    "cathedral_wall_roughness", 
    "cathedral_wall_roughness", 
    "cathedral_wall_roughness", 
    "metal_roughness",
    "cathedral_wall_roughness", 
    "granite_wall_roughness", 
    "medieval_floor_roughness",
  };

  Dungeon& dungeon = resources_->GetDungeon();

  vector<FbxData> models;
  vector<GLuint> textures;
  vector<GLuint> normal_textures;
  vector<GLuint> specular_textures;
  for (int tile = 0; tile < instanced_tiles.size(); tile++) {
    FbxData data;
    FbxLoad(model_names[tile], data);

    models.push_back(data);

    textures.push_back(resources_->GetTextureByName(texture_names[tile]));
    normal_textures.push_back(resources_->GetTextureByName(normal_texture_names[tile]));
    specular_textures.push_back(resources_->GetTextureByName(specular_texture_names[tile]));
  }

  for (int cx = 0; cx < kDungeonCells; cx++) {
    for (int cz = 0; cz < kDungeonCells; cz++) {
      for (int i = 0; i < instanced_tiles.size(); i++) {
        char tile = instanced_tiles[i];

        if (!created_dungeon_buffers) {
          glGenBuffers(1, &dungeon_render_data[cx][cz].vbos[tile]);
          glGenBuffers(1, &dungeon_render_data[cx][cz].uvs[tile]);
          glGenBuffers(1, &dungeon_render_data[cx][cz].normals[tile]);
          glGenBuffers(1, &dungeon_render_data[cx][cz].element_buffers[tile]);
          glGenBuffers(1, &dungeon_render_data[cx][cz].matrix_buffers[tile]);
          glGenBuffers(1, &dungeon_render_data[cx][cz].tangent_buffers[tile]);
          glGenBuffers(1, &dungeon_render_data[cx][cz].bitangent_buffers[tile]);
        }

        const FbxData& data = models[i];
        dungeon_render_data[cx][cz].textures[tile] = textures[i];
        dungeon_render_data[cx][cz].normal_textures[tile] = normal_textures[i];
        dungeon_render_data[cx][cz].specular_textures[tile] = specular_textures[i];

        vector<vec3> vertices;
        vector<unsigned int> indices;
        for (int i = 0; i < data.raw_mesh.indices.size(); i++) {
          vertices.push_back(data.raw_mesh.vertices[data.raw_mesh.indices[i]]);
          indices.push_back(i);
        }
        dungeon_render_data[cx][cz].num_indices[tile] = indices.size();

        vector<vec3> tangents(vertices.size());
        vector<vec3> bitangents(vertices.size());
        for (int i = 0; i < vertices.size(); i += 3) {
          vec3 delta_pos1 = vertices[i+1] - vertices[i+0];
          vec3 delta_pos2 = vertices[i+2] - vertices[i+0];
          vec2 delta_uv1 = data.raw_mesh.uvs[i+1] - data.raw_mesh.uvs[i+0];
          vec2 delta_uv2 = data.raw_mesh.uvs[i+2] - data.raw_mesh.uvs[i+0];

          float r = 1.0f / (delta_uv1.x * delta_uv2.y - delta_uv1.y * delta_uv2.x);
          vec3 tangent = (delta_pos1 * delta_uv2.y - delta_pos2 * delta_uv1.y) * r;
          vec3 bitangent = (delta_pos2 * delta_uv1.x - delta_pos1 * delta_uv2.x) * r;
          tangents[i+0] = tangent;
          tangents[i+1] = tangent;
          tangents[i+2] = tangent;
          bitangents[i+0] = bitangent;
          bitangents[i+1] = bitangent;
          bitangents[i+2] = bitangent;
        }

        glBindBuffer(GL_ARRAY_BUFFER, dungeon_render_data[cx][cz].vbos[tile]);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vec3), 
          &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, dungeon_render_data[cx][cz].uvs[tile]);
        glBufferData(GL_ARRAY_BUFFER, data.raw_mesh.uvs.size() * sizeof(vec2), 
          &data.raw_mesh.uvs[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, dungeon_render_data[cx][cz].normals[tile]);
        glBufferData(GL_ARRAY_BUFFER, data.raw_mesh.normals.size() * sizeof(vec3), 
          &data.raw_mesh.normals[0], GL_STATIC_DRAW);

        vector<mat4> model_matrices;
        char** dungeon_map = resources_->GetDungeonMap();
        float y = 0;

        int start_x = 14 * cx;
        int end_x = start_x + 14;
        int start_z = 14 * cz;
        int end_z = start_z + 14;
        for (int x = start_x; x < end_x; x++) {
          for (int z = start_z; z < end_z; z++) {
            float room_x = 10.0f * x;
            float room_z = 10.0f * z;
            vec3 pos = kDungeonOffset + vec3(room_x, y, room_z);

            if (tile == '|') {
              if (dungeon_map[x][z] != '|' && dungeon_map[x][z] != '-') continue;
            } else if (tile == 'o') {
              if (dungeon_map[x][z] != 'o' && dungeon_map[x][z] != 'O' && 
                dungeon_map[x][z] != 'g' && dungeon_map[x][z] != 'G') continue;
            } else if (tile == 'd') {
              if (dungeon_map[x][z] != 'd' && dungeon_map[x][z] != 'D') continue;
            } else if (tile == 'g') {
              if (dungeon_map[x][z] != 'g' && dungeon_map[x][z] != 'G') continue;
            } else if (tile == 'P') {
              if (dungeon_map[x][z] != 'p' && dungeon_map[x][z] != 'P') continue;
            } else if (tile == ' ') {
            } else if (tile == 'c') {
            } else if (tile == 's') {
            } else if (tile == '+') {
              if (dungeon_map[x][z] != '+' && dungeon_map[x][z] != '^' && 
                  dungeon_map[x][z] != '/') continue;
            } else {
              if (dungeon_map[x][z] != tile) continue;
            }
           
            mat4 ModelMatrix = translate(mat4(1.0), pos);

            // Rotate.
            if (dungeon_map[x][z] == '-' || dungeon_map[x][z] == 'O' || 
              dungeon_map[x][z] == 'D' || dungeon_map[x][z] == 'G') {
              ModelMatrix *= rotate(mat4(1.0), 1.57f, vec3(0, 1, 0));
            }

            if (tile == 'c') {  
              if (dungeon_map[x][z] != '<' && dungeon_map[x][z] != '\'' && 
                !resources_->GetDungeon().IsChamber(x, z)) { // Not upstairs. Draw ceiling.
                // Create ceiling.
                mat4 ModelMatrix = translate(mat4(1.0), pos + vec3(0, 50, 0));
                // mat4 ModelMatrix = translate(mat4(1.0), pos + vec3(0, 25, 0));
                model_matrices.push_back(ModelMatrix);
              }
            } else if (tile == ' ') {  
              if (dungeon_map[x][z] != '>' && dungeon_map[x][z] != '~' &&
                !resources_->GetDungeon().IsChamber(x, z)) { // Not downstairs. Draw floor.
                model_matrices.push_back(ModelMatrix);
              }
            } else if (tile == 's') {  
              if (dungeon_map[x][z] != '>' && dungeon_map[x][z] != '~' &&
                  resources_->GetDungeon().IsChamber(x, z)) {
                model_matrices.push_back(ModelMatrix);
              }
            } else {
              model_matrices.push_back(ModelMatrix);
            }
          }
        }
        dungeon_render_data[cx][cz].num_objs[tile] = model_matrices.size();

        glBindBuffer(GL_ARRAY_BUFFER, dungeon_render_data[cx][cz].matrix_buffers[tile]);
        glBufferData(GL_ARRAY_BUFFER, model_matrices.size() * sizeof(mat4), 
          &model_matrices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, dungeon_render_data[cx][cz].tangent_buffers[tile]);
        glBufferData(GL_ARRAY_BUFFER, tangents.size() * sizeof(vec3), 
          &tangents[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, dungeon_render_data[cx][cz].bitangent_buffers[tile]);
        glBufferData(GL_ARRAY_BUFFER, bitangents.size() * sizeof(vec3), 
          &bitangents[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dungeon_render_data[cx][cz].element_buffers[tile]); 
        glBufferData(
          GL_ELEMENT_ARRAY_BUFFER, 
          indices.size() * sizeof(unsigned int), 
          &indices[0], 
          GL_STATIC_DRAW
        );

        glGenVertexArrays(1, &dungeon_render_data[cx][cz].vaos[tile]);
        glBindVertexArray(dungeon_render_data[cx][cz].vaos[tile]);

        BindBuffer(dungeon_render_data[cx][cz].vbos[tile], 0, 3);
        glVertexAttribDivisor(0, 0); // Always reuse the same vertices.
        BindBuffer(dungeon_render_data[cx][cz].uvs[tile], 1, 2);
        glVertexAttribDivisor(1, 0); // Always reuse the same vertices.
        BindBuffer(dungeon_render_data[cx][cz].normals[tile], 2, 3);
        glVertexAttribDivisor(2, 0); // Always reuse the same vertices.

        std::size_t vec4_size = sizeof(vec4);
        glEnableVertexAttribArray(3); 
        glBindBuffer(GL_ARRAY_BUFFER, dungeon_render_data[cx][cz].matrix_buffers[tile]);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * vec4_size, (void*)0);
        glVertexAttribDivisor(3, 1);
        glEnableVertexAttribArray(4); 
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * vec4_size, (void*)(1 * vec4_size));
        glVertexAttribDivisor(4, 1);
        glEnableVertexAttribArray(5); 
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * vec4_size, (void*)(2 * vec4_size));
        glVertexAttribDivisor(5, 1);
        glEnableVertexAttribArray(6); 
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 4 * vec4_size, (void*)(3 * vec4_size));
        glVertexAttribDivisor(6, 1);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dungeon_render_data[cx][cz].element_buffers[tile]);
        glVertexAttribDivisor(7, 0); // Always reuse the same vertices.

        BindBuffer(dungeon_render_data[cx][cz].tangent_buffers[tile], 8, 3);
        glVertexAttribDivisor(8, 0); // Always reuse the same vertices.

        BindBuffer(dungeon_render_data[cx][cz].bitangent_buffers[tile], 9, 3);
        glVertexAttribDivisor(9, 0); // Always reuse the same vertices.

        glBindVertexArray(0);
        int num_slots = 8;
        for (int slot = 0; slot < num_slots; slot++) {
          glDisableVertexAttribArray(slot);
        }
      }
    }
  }
  created_dungeon_buffers = true;

  double elapsed_time = glfwGetTime() - start_time;
  cout << "CreateDungeonBuffers took " << elapsed_time << " seconds" << endl;
}

void Renderer::DrawDungeonTiles() {
  shared_ptr<Configs> configs = resources_->GetConfigs();

  int num_culled = 0;
  vector<char> instanced_tiles { ' ', '+', '|', 'o', 'd', 'g', 'P', 'c', 's' };
  for (int cx = 0; cx < kDungeonCells; cx++) {
    for (int cz = 0; cz < kDungeonCells; cz++) {
      float size = 140.0f;
      float room_x = cx * size;
      float room_z = cz * size;

      AABB aabb;
      aabb.point = kDungeonOffset + vec3(room_x, 0, room_z) - vec3(5, 0, 5);
      aabb.dimensions = vec3(size, 50, size);

      // Min distance is more than light radius.
      vec3 closest = ClosestPtPointAABB(player_pos_, aabb);
      if (length(closest - player_pos_) > configs->light_radius) {
        num_culled++;
        continue;
      }

      // Cull?
      if (!CollideAABBFrustum(aabb, frustum_planes_, player_pos_)) {
        num_culled++;
        continue;
      }

      for (int i = 0; i < instanced_tiles.size(); i++) {
        char tile = instanced_tiles[i];

        GLuint program_id = resources_->GetShader("dungeon");
        glUseProgram(program_id);

        glBindVertexArray(dungeon_render_data[cx][cz].vaos[tile]);
        glDisable(GL_CULL_FACE);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, dungeon_render_data[cx][cz].textures[tile]);
        glUniform1i(GetUniformId(program_id, "texture_sampler"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, dungeon_render_data[cx][cz].normal_textures[tile]);
        glUniform1i(GetUniformId(program_id, "normal_sampler"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, dungeon_render_data[cx][cz].specular_textures[tile]);
        glUniform1i(GetUniformId(program_id, "specular_sampler"), 2);

        int draw_diffuse = boost::lexical_cast<int>(resources_->GetGameFlag("dungeon_draw_diffuse"));
        int specular_component = boost::lexical_cast<float>(resources_->GetGameFlag("dungeon_specular_component"));
        int normal_strength = boost::lexical_cast<float>(resources_->GetGameFlag("dungeon_normal_strength"));
        glUniform1i(GetUniformId(program_id, "draw_diffuse"), draw_diffuse);
        glUniform1f(GetUniformId(program_id, "specular_component"), specular_component);
        glUniform1f(GetUniformId(program_id, "normal_strength"), normal_strength);

        // Shadow.
        mat4 shadow_matrix0 = GetShadowMatrix(true, 0);
        glUniformMatrix4fv(GetUniformId(program_id, "shadow_matrix0"), 1, GL_FALSE, &shadow_matrix0[0][0]);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, shadow_textures_[0]);
        glUniform1i(GetUniformId(program_id, "shadow_sampler"), 3);

        if (tile == ' ') {
          glUniform1i(GetUniformId(program_id, "draw_shadows"), 1);
        } else {
          glUniform1i(GetUniformId(program_id, "draw_shadows"), 0);
        }
        
        mat4 VP = projection_matrix_ * view_matrix_;  
        glUniformMatrix4fv(GetUniformId(program_id, "VP"), 1, GL_FALSE, &VP[0][0]);
        glUniformMatrix4fv(GetUniformId(program_id, "V"), 1, GL_FALSE, &view_matrix_[0][0]);
        glUniformMatrix4fv(GetUniformId(program_id, "P"), 1, GL_FALSE, &projection_matrix_[0][0]);

        glUniform3fv(GetUniformId(program_id, "player_pos"), 1,
          (float*) &camera_.position);

        glUniform3fv(GetUniformId(program_id, "light_direction"), 1,
          (float*) &configs->sun_position);

        vec3 light_color = vec3(1, 1, 1);
        glUniform3fv(GetUniformId(program_id, "lighting_color"), 1,
          (float*) &light_color);

        glUniform1f(GetUniformId(program_id, "light_radius"), configs->light_radius);

        glUniform3fv(GetUniformId(program_id, "player_pos"), 1,
          (float*) &resources_->GetPlayer()->position);

        glDrawElementsInstanced(
          GL_TRIANGLES, dungeon_render_data[cx][cz].num_indices[tile], 
          GL_UNSIGNED_INT, 0, dungeon_render_data[cx][cz].num_objs[tile]
        );
        glBindVertexArray(0);
      }
    }
  }
  // cout << "num_culled: " << num_culled << " of " << 
  //   kDungeonCells * kDungeonCells << endl;
}

void Renderer::SetCamera(const Camera& camera) { camera_ = camera; }
