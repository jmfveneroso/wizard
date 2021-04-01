#include "renderer.hpp"
#include "boost/filesystem.hpp"
#include <boost/algorithm/string/predicate.hpp>

namespace {

const int kTileSize = 52;

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

Renderer::Renderer(shared_ptr<Resources> asset_catalog, 
  shared_ptr<Draw2D> draw_2d, shared_ptr<Project4D> project_4d, 
  GLFWwindow* window, int window_width, int window_height) : 
  resources_(asset_catalog), draw_2d_(draw_2d), project_4d_(project_4d),
  window_(window), window_width_(window_width), window_height_(window_height) {
  cout << "Window width: " << window_width_ << endl;
  cout << "Window height: " << window_height_ << endl;
  Init();
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
    resources_->GetShader("water"));
  terrain_->set_asset_catalog(resources_);
  terrain_->set_shadow_texture(shadow_textures_[0], 0);
  terrain_->set_shadow_texture(shadow_textures_[1], 1);
  terrain_->set_shadow_texture(shadow_textures_[2], 2);

  CreateParticleBuffers();
}

void Renderer::GetFrustumPlanes(vec4 frustum_planes[6]) {
  mat4 ModelMatrix = translate(mat4(1.0), camera_.position);
  mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
  mat3 ModelView3x3Matrix = mat3(ModelViewMatrix);
  mat4 MVP = projection_matrix_ * view_matrix_ * ModelMatrix;
  ExtractFrustumPlanes(MVP, frustum_planes);
}

void Renderer::GetPotentiallyVisibleObjects(const vec3& player_pos, 
  shared_ptr<OctreeNode> octree_node,
  vector<shared_ptr<GameObject>>& objects) {
  if (!octree_node) {
    return;
  }

  AABB aabb;
  aabb.point = octree_node->center - octree_node->half_dimensions;
  aabb.dimensions = octree_node->half_dimensions * 2.0f;

  // TODO: find better name for this function.
  if (!CollideAABBFrustum(aabb, frustum_planes_, player_pos)) {
    return;
  }

  for (int i = 0; i < 8; i++) {
    GetPotentiallyVisibleObjects(player_pos, octree_node->children[i], objects);
  }

  for (auto& [id, obj] : octree_node->objects) {
    switch (obj->type) {
      case GAME_OBJ_DEFAULT:
      case GAME_OBJ_REGION:
      case GAME_OBJ_WAYPOINT:
      case GAME_OBJ_DOOR:
      case GAME_OBJ_ACTIONABLE:
        break;
      case GAME_OBJ_MISSILE: {
        if (obj->life > 0.0f) {
          break;
        } else {
          continue;
        }
      }
      default:
        continue;
    }

    if (obj->status != STATUS_DEAD) {
      objects.push_back(obj);
    }
  }

  for (auto& [id, obj] : octree_node->moving_objs) {
    switch (obj->type) {
      case GAME_OBJ_DEFAULT:
      case GAME_OBJ_REGION:
      case GAME_OBJ_WAYPOINT:
      case GAME_OBJ_DOOR:
      case GAME_OBJ_ACTIONABLE:
        break;
      case GAME_OBJ_MISSILE: {
        if (obj->life > 0.0f) {
          break;
        } else {
          continue;
        }
      }
      default:
        continue;
    }

    if (obj->status != STATUS_DEAD) {
      objects.push_back(obj);
    }
  }
}

vector<shared_ptr<GameObject>> 
Renderer::GetPotentiallyVisibleObjectsFromSector(shared_ptr<Sector> sector) {
  vector<shared_ptr<GameObject>> objs;
  GetPotentiallyVisibleObjects(camera_.position, sector->octree_node, objs);

  // Sort from closest to farthest.
  for (auto& obj : objs) {
    obj->distance = length(camera_.position - obj->position);
  }
  std::sort(objs.begin(), objs.end(), [] (const auto& lhs, const auto& rhs) {
    return lhs->distance < rhs->distance;
  });

  return objs;
}

bool Renderer::CullObject(shared_ptr<GameObject> obj, 
  const vector<vector<Polygon>>& occluder_convex_hulls) {
  if (obj->name == "hand-001") return true;
  if (obj->name == "skydome") return true;
  // TODO: draw hand without object.

  if (!obj->draw) {
    return true;
  }

  // Frustum cull.
  BoundingSphere bounding_sphere = obj->GetTransformedBoundingSphere();
  if (!CollideSphereFrustum(bounding_sphere, frustum_planes_, camera_.position)) {
    return true;
  }

  // Distance cull.
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

ConvexHull Renderer::CreateConvexHullFromOccluder(
  const vector<Polygon>& polygons, const vec3& player_pos) {
  ConvexHull convex_hull;

  // Find occlusion hull edges.
  unordered_map<string, Edge> edges;
  for (const Polygon& p : polygons) {
    const vec3& plane_point = p.vertices[0];
    const vec3& normal = p.normals[0];
    if (IsBehindPlane(player_pos, plane_point, normal)) {
      continue;
    }

    convex_hull.push_back(p);

    // If the polygon faces viewer, do the following for all its edges: 
    // If the edge is already in the edge list, remove the edge from the 
    // list. Otherwise, add the edge into the list.
    const vector<vec3>& vertices = p.vertices;
    const vector<vec3>& normals = p.normals;
    const vector<unsigned int>& indices = p.indices;

    // TODO: contemplate not triangular polygons. Maybe?
    vector<pair<int, int>> comparisons { { 0, 1 }, { 1, 2 }, { 2, 0 } };
    for (auto& [a, b] : comparisons) {
      int i = (p.indices[a] < p.indices[b]) ? a : b;
      int j = (p.indices[a] < p.indices[b]) ? b : a;
      
      string key = boost::lexical_cast<string>(indices[i]) + "-" + 
        boost::lexical_cast<string>(indices[j]);
      if (edges.find(key) == edges.end()) {
        edges[key] = Edge(vertices[i], vertices[j], indices[i], indices[j], 
          normals[i], normals[j]);
      } else {
        edges.erase(key);
      }
    }
  }

  if (edges.empty()) {
    return convex_hull;
  }

  for (auto& [key, e] : edges) {
    Polygon plane = CreatePolygonFrom3Points(player_pos, e.a, e.b, e.a_normal);
    convex_hull.push_back(plane);
  }
  return convex_hull;
}

vector<ObjPtr> Renderer::GetVisibleObjectsInSector(
  shared_ptr<Sector> sector, vec4 frustum_planes[6]) {
  vector<ObjPtr> visible_objects;

  vector<shared_ptr<GameObject>> objs =
    GetPotentiallyVisibleObjectsFromSector(sector);

  vector<shared_ptr<GameObject>> transparent_objs;
  GLuint transparent_shader = resources_->GetShader("transparent_object");
  GLuint lake_shader = resources_->GetShader("mana_pool");
  GLuint region_shader = resources_->GetShader("region");

  // Occlusion culling
  // https://www.gamasutra.com/view/feature/2979/rendering_the_great_outdoors_fast_.php?print=1
  vector<vector<Polygon>> occluder_convex_hulls;
  for (auto& obj : objs) {
    if (CullObject(obj, occluder_convex_hulls)) {
      continue;
    }

    if (obj->GetAsset()->shader == transparent_shader || 
        obj->GetAsset()->shader == lake_shader || 
        obj->GetAsset()->shader == region_shader) {
      transparent_objs.push_back(obj);
    } else {
      visible_objects.push_back(obj);
    }

    if (obj->GetAsset()->occluder.empty()) continue;

    // Add occlusion hull.
    vector<Polygon> polygons = obj->GetAsset()->occluder;
    for (auto& poly : polygons) {
      for (auto& v : poly.vertices) {
        v += obj->position;
      }
    }

    ConvexHull convex_hull = CreateConvexHullFromOccluder(polygons, 
      camera_.position);
    if (!convex_hull.empty()) {
      occluder_convex_hulls.push_back(convex_hull);
    }
  }

  for (int i = transparent_objs.size() - 1; i >= 0; i--) {
    visible_objects.push_back(transparent_objs[i]);
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

  for (auto& poly : mesh->polygons) {
    vec3 portal_point = p->position + poly.vertices[0];
    vec3 normal = poly.normals[0];

    if (TestSphereTriangleIntersection(sphere - p->position, poly.vertices)) {
      in_frustum = true;
      break;
    }

    if (dot(normalize(camera_.position - portal_point), normal) < 0.000001f) {
      continue;
    }

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
    terrain_clipping_normal_ = poly.normals[0];
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

    vector<ObjPtr> objs = GetVisibleObjectsInStabbingTreeNode(node, frustum_planes);
    visible_objects.insert(visible_objects.end(), objs.begin(), objs.end()); 

    visible_objects.push_back(p);
  }
  return visible_objects;
}

vector<ObjPtr> Renderer::GetVisibleObjectsInStabbingTreeNode(
  shared_ptr<StabbingTreeNode> stabbing_tree_node, vec4 frustum_planes[6]) {
  vector<ObjPtr> visible_objects;
  shared_ptr<Sector> sector = stabbing_tree_node->sector;
  if (sector->name == "outside") {
    vector<ObjPtr> objs = GetVisibleObjectsInCaves(stabbing_tree_node, 
      frustum_planes);
    visible_objects.insert(visible_objects.end(), objs.begin(), objs.end()); 

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
  terrain_->UpdateClipmaps(camera_.position);

  if (clip_terrain_) {
    terrain_->SetClippingPlane(terrain_clipping_point_, 
      terrain_clipping_normal_);
  }

  mat4 shadow_matrix0 = GetShadowMatrix(true, 0);
  mat4 shadow_matrix1 = GetShadowMatrix(true, 1);
  mat4 shadow_matrix2 = GetShadowMatrix(true, 2);

  // TODO: pass frustum planes.
  terrain_->Draw(camera_, view_matrix_, camera_.position, shadow_matrix0, 
    shadow_matrix1, shadow_matrix2, false, clip_terrain_);

  ObjPtr player = resources_->GetPlayer();
  ObjPtr skydome = resources_->GetObjectByName("skydome");
  skydome->position = player->position;
  skydome->position.y = -1000;
  DrawObject(skydome);
}

// TODO: split into functions for each shader.
void Renderer::DrawObject(shared_ptr<GameObject> obj) {
  if (obj == nullptr) return;

  if (obj->type == GAME_OBJ_PORTAL) {
    // TODO: draw portal.
    return;
  }

  if (obj->GetAsset()->type == ASSET_CREATURE) {
    if (obj->status == STATUS_BURROWED) {
      return;
    }
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

    glUseProgram(program_id);

    glBindVertexArray(mesh->vao_);
    mat4 ModelMatrix = translate(mat4(1.0), obj->position);
    ModelMatrix = ModelMatrix * obj->rotation_matrix;

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

    shared_ptr<Configs> configs = resources_->GetConfigs();
    glUniform3fv(GetUniformId(program_id, "light_direction"), 1,
      (float*) &configs->sun_position);

    glUniform1f(GetUniformId(program_id, "outdoors"), 1);
    if (obj->current_sector) {
      glUniform3fv(GetUniformId(program_id, "lighting_color"), 1,
        (float*) &obj->current_sector->lighting_color);

      if (obj->current_sector->name != "outside") {
        glUniform1f(GetUniformId(program_id, "outdoors"), 0);
      }
    }

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

    if (program_id == resources_->GetShader("animated_object")) {
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

      glDrawArrays(GL_TRIANGLES, 0, mesh->num_indices);
    } else if (program_id == resources_->GetShader("animated_transparent_object")) {
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
    } else if (program_id == resources_->GetShader("object")) {
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

void Renderer::DrawObjects(vector<ObjPtr> objs) {
  glDisable(GL_CULL_FACE);
  for (auto& obj : objs) {
    if (!obj) { // Terrain.
      DrawOutside();
    } else if (obj->type == GAME_OBJ_PORTAL) {
      DrawParticles();
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      DrawObject(obj);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    } else {
      DrawObject(obj);
    }
  }
  DrawParticles();
  glEnable(GL_CULL_FACE);
}

void Renderer::Draw() {
  vec3 normal;
  float h = resources_->GetHeightMap().GetTerrainHeight(vec2(camera_.position.x, camera_.position.z), &normal);
  float delta_h = camera_.position.y - h;
  if (delta_h < 500.0f) {
    delta_h = 0.0f;
  }
  projection_matrix_ = glm::perspective(glm::radians(FIELD_OF_VIEW), 
    4.0f / 3.0f, NEAR_CLIPPING + delta_h / 2.0f, FAR_CLIPPING);

  // https://www.3dgep.com/understanding-the-view-matrix/#:~:text=The%20view%20matrix%20is%20used,things%20are%20the%20same%20thing!&text=The%20View%20Matrix%3A%20This%20matrix,of%20the%20camera's%20transformation%20matrix.
  view_matrix_ = glm::lookAt(
    camera_.position,                     // Camera is here
    camera_.position + camera_.direction, // and looks here : at the same position, plus "direction"
    camera_.up                            // Head is up (set to vec3(0, -1, 0) to look upside-down)
  );

  DrawShadows();

  glViewport(0, 0, window_width_, window_height_);
  glClearColor(0.73, 0.81, 0.92, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  clip_terrain_ = false;
  GetFrustumPlanes(frustum_planes_);
  vector<ObjPtr> objects = GetVisibleObjects(frustum_planes_);
  DrawObjects(objects);

  glClear(GL_DEPTH_BUFFER_BIT);
  DrawHand();
  DrawScreenEffects();
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
  mat4 view_matrix = lookAt(
    sphere_center + sun_dir * 500.0f,
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

    GLuint program_id = resources_->GetShader("shadow");
    glUseProgram(program_id);
    glBindVertexArray(mesh->vao_);

    // Check if animated object.
    mat4 projection_view_matrix = GetShadowMatrix(false, level);
    mat4 model_matrix = translate(mat4(1.0), obj->position);
    model_matrix = model_matrix * obj->rotation_matrix;
    mat4 MVP = projection_view_matrix * model_matrix;

    glUniformMatrix4fv(GetUniformId(program_id, "MVP"), 1, GL_FALSE, &MVP[0][0]);
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
  GLuint lake_shader = resources_->GetShader("mana_pool");
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
    vec3 position = cascade_shadows_[i].bounding_sphere.center + sun_dir * 500.0f;
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
      } else if (obj->GetAsset()->shader != transparent_shader &&
        obj->GetAsset()->shader != lake_shader) {
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

void Renderer::DrawSpellbar() {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  vector<ItemData>& item_data = resources_->GetItemData();
  int (&item_matrix)[8][7] = configs->item_matrix;
  int (&spellbar)[8] = configs->spellbar;

  draw_2d_->DrawImage("spell_bar", 400, 730, 600, 600, 1.0);
  for (int x = 0; x < 8; x++) {
    int top = 742;
    int left = 433 + kTileSize * x;
    int item_id = configs->spellbar[x];
    if (item_id != 0) {
      draw_2d_->DrawImage(item_data[item_id].icon, left, top, 64, 64, 1.0); 
    }

    if (configs->selected_spell == x) {
      draw_2d_->DrawImage("selected_item", left, top, 64, 64, 1.0); 
    }
  }
}

void Renderer::DrawScreenEffects() {
  const int kWindowWidth = 1280;
  const int kWindowHeight = 800;
  shared_ptr<Configs> configs = resources_->GetConfigs();
  float taking_hit = configs->taking_hit;
  if (taking_hit > 0.0) {
    draw_2d_->DrawImage("hit-effect", 0, 0, kWindowWidth, kWindowHeight, taking_hit / 30.0f);
  }

  if (configs->fading_out > -60.0) {
    draw_2d_->DrawImage("fade-out", 0, 0, kWindowWidth, kWindowHeight, 
      1.0f - (abs(configs->fading_out) / 60.0f));
  }

  draw_2d_->DrawImage("crosshair", 640-4, 400-4, 8, 8, 0.5);

  DrawSpellbar();

  // HP.
  shared_ptr<Player> player = resources_->GetPlayer();
  float hp_bar_width = player->life / 100.0f;
  hp_bar_width = (hp_bar_width > 0) ? hp_bar_width : 0;

  draw_2d_->DrawImage("hp_liquid", 25, 760, hp_bar_width * 266, 266, 1.0, 
    vec2(hp_bar_width, 1));
  draw_2d_->DrawImage("hp_bar", 20, 750, 300, 300, 1.0);

  if (configs->edit_terrain != "none") {
    draw_2d_->DrawText("Brush size:", 350, 22, vec4(1, 0.3, 0.3, 1.0));
    draw_2d_->DrawText(boost::lexical_cast<string>(configs->brush_size), 450, 22, vec4(1, 0.3, 0.3, 1));

    draw_2d_->DrawText("Selected tile:", 550, 22, vec4(1, 0.3, 0.3, 1.0));
    draw_2d_->DrawText(boost::lexical_cast<string>(configs->selected_tile), 670, 22, vec4(1, 0.3, 0.3, 1));

    draw_2d_->DrawText("Raise Factor:", 750, 22, vec4(1, 0.3, 0.3, 1.0));
    draw_2d_->DrawText(boost::lexical_cast<string>(configs->raise_factor), 870, 22, vec4(1, 0.3, 0.3, 1));
  }

  // Draw messages.
  double cur_time = glfwGetTime();
  int num_msgs = configs->messages.size();
  for (int i = num_msgs-1, y = 0; i >= num_msgs - 5 && i >= 0; i--, y += 20) {
    const string& msg = get<0>(configs->messages[i]);
    const float time_remaining = (get<1>(configs->messages[i]) - cur_time) / 10.0f;
    draw_2d_->DrawText(msg, 25, 800 - 730 + y, 
      vec4(1, 1, 1, time_remaining), 1.0, false, "avenir_light_oblique");
  }

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
  mat4 rotation_matrix = rotate(
    mat4(1.0),
    camera_.rotation.y + 4.71f,
    vec3(0.0f, 1.0f, 0.0f)
  );
  rotation_matrix = rotate(
    rotation_matrix,
    camera_.rotation.x,
    vec3(0.0f, 0.0f, 1.0f)
  );
  obj->rotation_matrix = rotation_matrix;
  obj->position = camera_.position + camera_.direction * -5.0f;
  obj->position += vec3(0, -0.5, 0);

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
  Particle* particle_container = resources_->GetParticleContainer();
  for (int i = 0; i < kMaxParticles; i++) {
    Particle& p = particle_container[i];
    if (!p.type || p.life < 0) {
      p.camera_distance = -1;
      continue;
    }
    if (p.type->behavior == PARTICLE_FIXED) {
      p.camera_distance = 0;
      continue;
    }
    p.camera_distance = length2(p.pos - camera_.position);
  }
  std::sort(&particle_container[0], &particle_container[kMaxParticles]);

  for (auto& [name, prd] : particle_render_data_) {
    prd.count = 0;
  }

  for (int i = 0; i < kMaxParticles; i++) {
    Particle& p = particle_container[i];
    if (p.life < 0 || !p.type) continue;

    vec2 uv;
    int index = p.frame + p.type->first_frame;
    float tile_size = 1.0f / float(p.type->grid_size);
    uv.x = int(index % p.type->grid_size) * tile_size + tile_size / 2;
    uv.y = (p.type->grid_size - int(index / p.type->grid_size) - 1) * tile_size 
      + tile_size / 2;

    ParticleRenderData& prd = particle_render_data_[p.type->name];
    prd.particle_positions[prd.count] = vec4(p.pos, p.size);
    prd.particle_colors[prd.count] = p.color;
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

