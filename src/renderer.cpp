#include "renderer.hpp"
#include "boost/filesystem.hpp"
#include <boost/algorithm/string/predicate.hpp>

// TODO: create file type for 3d Models.

// TODO: terrain rendering should be another lib.
//   The terrain lib should essentially create a mesh.

void Renderer::Init(const string& shader_dir) {
  if (!glfwInit()) throw "Failed to initialize GLFW";

  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // To make MacOS happy; should not be needed.
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); 

  // window_ = glfwCreateWindow(window_width_, window_height_, APP_NAME, NULL, NULL);
  window_ = glfwCreateWindow(window_width_, window_height_, APP_NAME, 
    glfwGetPrimaryMonitor(), NULL);
  if (window_ == NULL) {
    glfwTerminate();
    throw "Failed to open GLFW window";
  }

  // We would expect width and height to be 1024 and 768
  // But on MacOS X with a retina screen it'll be 1024*2 and 768*2, 
  // so we get the actual framebuffer size:
  glfwGetFramebufferSize(window_, &window_width_, &window_height_);
  glfwMakeContextCurrent(window_);

  // Needed for core profile.
  glewExperimental = true; 
  if (glewInit() != GLEW_OK) {
    glfwTerminate();
    throw "Failed to initialize GLEW";
  }

  // Hide the mouse and enable unlimited movement.
  glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwPollEvents();
  glfwSetCursorPos(window_, 0, 0);

  projection_matrix_ = glm::perspective(glm::radians(FIELD_OF_VIEW), 
    4.0f / 3.0f, NEAR_CLIPPING, FAR_CLIPPING);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS); 
  glEnable(GL_CULL_FACE);

  LoadShaders("shaders");
  fbos_["screen"] = CreateFramebuffer(window_width_, window_height_);

  // TODO: load texture with the object.
  texture_ = LoadPng("assets/textures_png/fish_uv.png");
  building_texture_ = LoadPng("assets/textures_png/first_floor_uv.png");
  granite_texture_ = LoadPng("assets/textures_png/granite.png");
  wood_texture_ = LoadPng("assets/textures_png/wood.png");

  terrain_ = make_shared<Terrain>(shaders_["terrain"], shaders_["water"]);

  Sector s;
  s.id = 0;
  s.stabbing_tree = make_shared<StabbingTreeNode>(0, 0, true);
  s.stabbing_tree->children.push_back(make_shared<StabbingTreeNode>(1, 0, false));
  sectors_[0] = s;
}

void Renderer::LoadShaders(const std::string& directory) {
  boost::filesystem::path p (directory);
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(p); itr != end_itr; ++itr) {
    if (!is_regular_file(itr->path())) continue;
    string current_file = itr->path().leaf().string();
    if (boost::ends_with(current_file, ".vert") ||
      boost::ends_with(current_file, ".frag") ||
      boost::ends_with(current_file, ".geom")) { 
      string prefix = current_file.substr(0, current_file.size() - 5);
      if (shaders_.find(prefix) == shaders_.end()) {
        shaders_[prefix] = LoadShader(prefix);
      }
    }
  }
}

FBO Renderer::CreateFramebuffer(int width, int height) {
  FBO fbo(width, height);

  glGenTextures(1, &fbo.texture);
  glBindTexture(GL_TEXTURE_2D, fbo.texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fbo.width, fbo.height, 0, GL_RGBA, 
    GL_UNSIGNED_BYTE, 0);

  glGenRenderbuffers(1, &fbo.depth_rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, fbo.depth_rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, fbo.width, 
    fbo.height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  glGenFramebuffers(1, &fbo.framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo.framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 
    fbo.texture, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 
    GL_RENDERBUFFER, fbo.depth_rbo);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    throw;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Framebuffer mesh. 
  vector<vec3> vertices {
    { -1, -1, 0.0 }, { -1,  1, 0.0 }, {  1, -1, 0.0 }, {  1, -1, 0.0 }, 
    { -1,  1, 0.0 }, {  1,  1, 0.0 }
  };

  vector<vec2> uvs = {
    { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 }
  };

  vector<unsigned int> indices { 0, 1, 2, 3, 4, 5 };

  GLuint vertex_buffer, uv_buffer, element_buffer;
  glGenBuffers(1, &vertex_buffer);
  glGenBuffers(1, &uv_buffer);
  glGenBuffers(1, &element_buffer);
  glGenVertexArrays(1, &fbo.vao);

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), 
    &vertices[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
  glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], 
    GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer); 
  glBufferData(
    GL_ELEMENT_ARRAY_BUFFER, 
    indices.size() * sizeof(unsigned int), 
    &indices[0], 
    GL_STATIC_DRAW
  );

  glBindVertexArray(fbo.vao);

  // TODO: cull faces.
  glDisable(GL_CULL_FACE);

  // Change to texture_sampler.
  BindTexture("texture_sampler", shaders_["screen"], fbo.texture);
  BindBuffer(vertex_buffer, 0, 3);
  BindBuffer(uv_buffer, 1, 2);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
  glBindVertexArray(0);

  for (int slot = 0; slot < 2; slot++) {
    glDisableVertexAttribArray(slot);
  }
  return fbo;
}

ConvexHull Renderer::CreateConvexHullFromOccluder(int occluder_id, 
  const vec3& player_pos) {
  ConvexHull convex_hull;

  unordered_map<string, Edge> edges;
  const vector<Polygon>& polygons = occluders_[occluder_id].polygons;

  // Find occlusion hull edges.
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
        edges[key] = Edge(vertices[i], vertices[j], indices[i], indices[j], normals[i], normals[j]);
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

int Renderer::GetPlayerSector(const vec3& player_pos) {
  for (int i = 1; i < sectors_.size(); i++) {
    if (IsInConvexHull(player_pos, sectors_[i].convex_hull)) {
      return i;
    }
  }
  return 0;
}

AABB GetObjectAABB(shared_ptr<GameObject> obj) {
  vector<vec3> vertices;
  for (auto& p : obj->polygons) {
    for (auto& v : p.vertices) {
      vertices.push_back(v + obj->position);
    }
  }
  return GetAABBFromVertices(vertices);
}

BoundingSphere GetObjectBoundingSphere(shared_ptr<GameObject> obj) {
  vector<vec3> vertices;
  for (auto& p : obj->polygons) {
    for (auto& v : p.vertices) {
      vertices.push_back(v + obj->position);
    }
  }
  return GetBoundingSphereFromVertices(vertices);
}

const vector<vec3> kOctreeNodeOffsets = {
  //    Z   Y   X
  vec3(-1, -1, -1), // 0 0 0
  vec3(-1, -1, +1), // 0 0 1
  vec3(-1, +1, -1), // 0 1 0
  vec3(-1, +1, +1), // 0 1 1
  vec3(+1, -1, -1), // 1 0 0
  vec3(+1, -1, +1), // 1 0 1
  vec3(+1, +1, -1), // 1 1 0
  vec3(+1, +1, +1), // 1 1 1
};

void Renderer::InsertObjectInOctree(shared_ptr<OctreeNode> octree_node, 
  shared_ptr<GameObject> object, int depth) {
  int index = 0, straddle = 0;
  cout << "--- Octree depth: " << depth << endl;
  cout << "--- Octree center: " << octree_node->center << endl;
  cout << "--- Octree radius: " << octree_node->half_dimensions << endl;

  // Compute the octant number [0..7] the object sphere center is in
  // If straddling any of the dividing x, y, or z planes, exit directly
  for (int i = 0; i < 3; i++) {
    float delta = object->bounding_sphere.center[i] - octree_node->center[i];
    if (abs(delta) < object->bounding_sphere.radius) {
      straddle = 1;
      break;
    }

    if (delta > 0.0f) index |= (1 << i); // ZYX
  }

  if (!straddle && depth < 8) {
    // Fully contained in existing child node; insert in that subtree
    if (octree_node->children[index] == nullptr) {
      octree_node->children[index] = make_shared<OctreeNode>(
        octree_node->center + kOctreeNodeOffsets[index] * 
          octree_node->half_dimensions * 0.5f, 
        octree_node->half_dimensions * 0.5f);
    }
    InsertObjectInOctree(octree_node->children[index], object, depth + 1);
  } else {
    // Straddling, or no child node to descend into, so
    // link object into linked list at this node
    octree_node->objects.push_back(object);
    cout << "Object: " << object->name << " inserted in the Octree" << endl;
    cout << "Octree depth: " << depth << endl;
    cout << "Octree center: " << octree_node->center << endl;
    cout << "Octree radius: " << octree_node->half_dimensions << endl;
    cout << "object->bounding_sphere.center: " << object->bounding_sphere.center << endl;
    cout << "object->bounding_sphere.radius: " << object->bounding_sphere.radius << endl;
  }
}

void Renderer::BuildOctree() {
  octree_ = make_shared<OctreeNode>(vec3(2100, 0, 2100), 
    vec3(4000, 4000, 4000));
 
  vector<shared_ptr<GameObject>>& objects = sectors_[0].objects;
  for (auto& obj : objects) {
    InsertObjectInOctree(octree_, obj, 0);
  }
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
  // cout << "octree_node->center: " << octree_node->center << endl;
  // cout << "octree_node->half_dimensions: " << octree_node->half_dimensions << endl;
  // cout << "player_pos: " << player_pos << endl;
  if (!CollideAABBFrustum(aabb, frustum_planes_, player_pos)) {
    // cout << "not in frustum" << endl;
    return;
  }
  // cout << "in frustum" << endl;

  for (int i = 0; i < 8; i++) {
    GetPotentiallyVisibleObjects(player_pos, octree_node->children[i], objects);
  }

  objects.insert(objects.end(), octree_node->objects.begin(), 
    octree_node->objects.end());
}

void Renderer::GetPotentiallyCollidingObjects(const vec3& player_pos, 
  shared_ptr<OctreeNode> octree_node,
  vector<shared_ptr<GameObject>>& objects) {
  if (!octree_node) {
    return;
  }

  AABB aabb;
  aabb.point = octree_node->center - octree_node->half_dimensions;
  aabb.dimensions = octree_node->half_dimensions * 2.0f;

  BoundingSphere s = BoundingSphere(player_pos, 0.75f);
  if (!TestSphereAABBIntersection(s, aabb)) {
    return;
  }

  for (int i = 0; i < 8; i++) {
    GetPotentiallyCollidingObjects(player_pos, octree_node->children[i], 
      objects);
  }

  objects.insert(objects.end(), octree_node->objects.begin(), 
    octree_node->objects.end());
}


void Renderer::DrawSector(shared_ptr<StabbingTreeNode> stabbing_tree_node) {
  int sector_id = stabbing_tree_node->id;

  // Outside
  if (sector_id == 0) {
    glEnable(GL_CULL_FACE);
    terrain_->Draw(projection_matrix_, view_matrix_, camera_.position);
    glDisable(GL_CULL_FACE);

    // TODO: frustum cull objects according to their position in the Octree.
  }

  // Occlusion culling
  // https://www.gamasutra.com/view/feature/2979/rendering_the_great_outdoors_fast_.php?print=1
  vector<vector<Polygon>> occluder_convex_hulls;
  Sector& s = sectors_[sector_id];

  // Cull with Octree.
  vector<shared_ptr<GameObject>> objs;
  if (sector_id == 0) {
    GetPotentiallyVisibleObjects(camera_.position, octree_, objs);
  } else {
    objs = s.objects;
  }

  // Sort for closest to farthest.
  for (auto& obj : objs) {
    obj->distance = length(camera_.position - obj->position);
  }
  std::sort(objs.begin(), objs.end(), [] (const auto& lhs, const auto& rhs) {
    return lhs->distance < rhs->distance;
  });

  static int bla = 1;
  for (auto& obj : objs) {
    AABB aabb = GetObjectAABB(obj);
    if (!obj->polygons.empty()) {
      bool occlude = false;
      if (bla > 0 && obj->name == "stone_pillar.fbx") {
        // CreateMeshFromAABB(aabb);
        bla--;
      }
      for (auto& ch : occluder_convex_hulls) {
        if (IsInConvexHull(aabb, ch)) {
          occlude = true;
          // cout << "occlude: " << obj->name << endl;
          break;
        }
      }
      if (occlude) {
        continue;
      }
    }

    if (obj->occluder_id != -1) {
      ConvexHull convex_hull = CreateConvexHullFromOccluder(obj->occluder_id, 
        camera_.position);
      if (!convex_hull.empty()) {
        occluder_convex_hulls.push_back(convex_hull);
        if (bla > 0) {
          // CreateMeshFromConvexHull(convex_hull);
          // bla = false;
        }
      }
    }

    if (!obj->draw) {
      continue;
    }

    // Frustum cull.
    if (!CollideAABBFrustum(aabb, frustum_planes_, camera_.position)) {
      // cout << "Occluded by frustum" << obj->name << endl;
      continue;
    }

    Mesh mesh;
    int lod_level = glm::clamp(int(obj->distance / LOD_DISTANCE), 0, 4);
    for (int i = lod_level; i >= 0; i--) {
      if (obj->lods[i].vao_ == 0) {
        continue;
      }
      mesh = obj->lods[i];
      break;
    }

    GLuint program_id = mesh.shader;
    glUseProgram(program_id);

    glBindVertexArray(mesh.vao_);
    glm::mat4 ModelMatrix = glm::translate(glm::mat4(1.0), obj->position);
    glm::mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
    glm::mat4 MVP = projection_matrix_ * ModelViewMatrix;

    glUniformMatrix4fv(GetUniformId(program_id, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(GetUniformId(program_id, "M"), 1, GL_FALSE, &ModelMatrix[0][0]);
    glUniformMatrix4fv(GetUniformId(program_id, "V"), 1, GL_FALSE, &view_matrix_[0][0]);

    if (program_id == shaders_["animated_object"]) {
      obj->frame++;
      if (obj->frame >= obj->joint_transforms[0].size()) {
        obj->frame = 0;
      }

      // TODO: max 10 joints. Enforce limit here.
      vector<mat4> joint_transforms;
      for (int i = 0; i < obj->joint_transforms.size(); i++) {
        joint_transforms.push_back(obj->joint_transforms[i][obj->frame]);
      }
      glUniformMatrix4fv(GetUniformId(program_id, "joint_transforms"), 
        joint_transforms.size(), GL_FALSE, &joint_transforms[0][0][0]);

      BindTexture("texture_sampler", program_id, texture_);
      glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
    } else if (program_id == shaders_["object"]) {
      if (obj->name == "assets/models_fbx/wooden_box.fbx") {
        BindTexture("texture_sampler", program_id, wood_texture_);
      } else if (obj->name == "assets/models_fbx/tower_outer_wall.fbx") {
        BindTexture("texture_sampler", program_id, building_texture_);
      } else if (obj->name == "assets/models_fbx/tower_inner_wall.fbx") {
        BindTexture("texture_sampler", program_id, building_texture_);
      } else if (obj->name == "assets/models_fbx/stone_pillar.fbx") {
        BindTexture("texture_sampler", program_id, granite_texture_);
      }
      glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
    } else {
      glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
  }

  mat4 ModelMatrix = translate(mat4(1.0), camera_.position);
  mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
  mat3 ModelView3x3Matrix = mat3(ModelViewMatrix);
  mat4 MVP = projection_matrix_ * view_matrix_ * ModelMatrix;

  // Portal culling 
  // http://di.ubi.pt/~agomes/tjv/teoricas/07-culling.pdf
  for (auto& node : stabbing_tree_node->children) {
    const Portal& p = portals_[node->portal_id];

    BoundingSphere sphere = BoundingSphere(camera_.position, 0.75f);
    bool in_frustum = false; 
    for (auto& poly : p.polygons) {
      if (TestSphereTriangleIntersection(sphere, poly.vertices)) {
        in_frustum = true;
        break;
      }

      if (CollideTriangleFrustum(poly.vertices, frustum_planes_,
        camera_.position)) {
        in_frustum = true;
        break;
      }
    }
    

    if (in_frustum) {
      DrawSector(node);
    }
  }
}

void Renderer::DrawObjects() {
  int current_sector_id = GetPlayerSector(camera_.position);
  DrawSector(sectors_[current_sector_id].stabbing_tree);
}

void Renderer::DrawFBO(const FBO& fbo) {
  GLuint program_id = shaders_["screen"];
  glBindVertexArray(fbo.vao);
  BindTexture("texture_sampler", program_id, fbo.texture);
  glUseProgram(program_id);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, fbo.width, fbo.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*) 0);
  glBindVertexArray(0);
}

void Renderer::Run(const function<void()>& process_frame) {
  GLint major_version, minor_version;
  glGetIntegerv(GL_MAJOR_VERSION, &major_version); 
  glGetIntegerv(GL_MINOR_VERSION, &minor_version);
  cout << "Open GL version is " << major_version << "." << 
    minor_version << endl;

  double last_time = glfwGetTime();
  int frames = 0;
  do {
    double current_time = glfwGetTime();
    frames++;

    // If last printf() was more than 1 second ago.
    if (current_time - last_time >= 1.0) { 
      cout << 1000.0 / double(frames) << " ms/frame" << endl;
      frames = 0;
      last_time += 1.0;
    }

    process_frame();

   // View matrix:
   // https://www.3dgep.com/understanding-the-view-matrix/#:~:text=The%20view%20matrix%20is%20used,things%20are%20the%20same%20thing!&text=The%20View%20Matrix%3A%20This%20matrix,of%20the%20camera's%20transformation%20matrix.
    view_matrix_ = glm::lookAt(
      camera_.position,                     // Camera is here
      camera_.position + camera_.direction, // and looks here : at the same position, plus "direction"
      camera_.up                            // Head is up (set to 0,-1,0 to look upside-down)
    );

    mat4 ModelMatrix = translate(mat4(1.0), camera_.position);
    mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
    mat3 ModelView3x3Matrix = mat3(ModelViewMatrix);
    mat4 MVP = projection_matrix_ * view_matrix_ * ModelMatrix;
    ExtractFrustumPlanes(MVP, frustum_planes_);

    glBindFramebuffer(GL_FRAMEBUFFER, fbos_["screen"].framebuffer);
    glViewport(0, 0, fbos_["screen"].width, fbos_["screen"].height);
    glClearColor(50, 50, 50, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    DrawObjects();
    DrawFBO(fbos_["screen"]);

    glfwSwapBuffers(window_);
    glfwPollEvents();
  } while (glfwWindowShouldClose(window_) == 0);

  // Cleanup VBO and shader.
  for (auto it : shaders_) {
    glDeleteProgram(it.second);
  }
  glfwTerminate();
}


















// ==============================================
//  OBJECT CREATION METHODS
// ==============================================

// TODO: transfer all mesh related code to another file.
shared_ptr<GameObject> Renderer::CreateCube(vec3 dimensions, vec3 position) {
  float w = dimensions.x;
  float h = dimensions.y;
  float l = dimensions.z;
 
  vector<vec3> v {
    vec3(0, h, 0), vec3(w, h, 0), vec3(0, 0, 0), vec3(w, 0, 0), // Back face.
    vec3(0, h, l), vec3(w, h, l), vec3(0, 0, l), vec3(w, 0, l), // Front face.
  };

  vector<vec3> vertices = {
    v[0], v[4], v[1], v[1], v[4], v[5], // Top.
    v[1], v[3], v[0], v[0], v[3], v[2], // Back.
    v[0], v[2], v[4], v[4], v[2], v[6], // Left.
    v[5], v[7], v[1], v[1], v[7], v[3], // Right.
    v[4], v[6], v[5], v[5], v[6], v[7], // Front.
    v[6], v[2], v[7], v[7], v[2], v[3]  // Bottom.
  };

  vector<vec2> u = {
    vec2(0, 0), vec2(0, l), vec2(w, 0), vec2(w, l), // Top.
    vec2(0, 0), vec2(0, h), vec2(w, 0), vec2(w, h), // Back.
    vec2(0, 0), vec2(0, h), vec2(l, 0), vec2(l, h)  // Left.
  };

  vector<glm::vec2> uvs {
    u[0], u[1], u[2],  u[2],  u[1], u[3],  // Top.
    u[4], u[5], u[6],  u[6],  u[5], u[7],  // Back.
    u[8], u[9], u[10], u[10], u[9], u[11], // Left.
    u[8], u[9], u[10], u[10], u[9], u[11], // Right.
    u[4], u[5], u[6],  u[6],  u[5], u[7],  // Front.
    u[0], u[1], u[2],  u[2],  u[1], u[3]   // Bottom.
  };

  vector<unsigned int> indices(36);
  for (int i = 0; i < 36; i++) { indices[i] = i; }

  Mesh mesh = CreateMesh(shaders_["solid"], vertices, uvs, indices);
  vector<Polygon> polygons;
  for (int i = 0; i < 12; i++) {
    Polygon p;
    p.vertices.push_back(vertices[i*3]);
    p.vertices.push_back(vertices[i*3+1]);
    p.vertices.push_back(vertices[i*3+2]);
    polygons.push_back(p);
  }

  shared_ptr<GameObject> obj = make_shared<GameObject>(mesh, position);
  obj->name = "cube";
  obj->polygons = polygons;
  obj->aabb = GetObjectAABB(obj);
  obj->bounding_sphere = GetObjectBoundingSphere(obj);
  sectors_[0].objects.push_back(obj);
  return obj; 
}

shared_ptr<GameObject> Renderer::CreateMeshFromConvexHull(const ConvexHull& ch) {
  int count = 0; 
  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<vec3> normals;
  vector<unsigned int> indices;
  for (auto& p : ch) {
    int polygon_size = p.vertices.size();
    for (int j = 1; j < polygon_size - 1; j++) {
      vertices.push_back(p.vertices[0]);
      uvs.push_back(vec2(0, 0));
      indices.push_back(count++);

      vertices.push_back(p.vertices[j]);
      uvs.push_back(vec2(0, 0));
      indices.push_back(count++);

      vertices.push_back(p.vertices[j+1]);
      uvs.push_back(vec2(0, 0));
      indices.push_back(count++);
    }
  }

  Mesh mesh = CreateMesh(shaders_["solid"], vertices, uvs, indices);
  shared_ptr<GameObject> obj = make_shared<GameObject>(mesh, vec3(0, 0, 0));
  obj->name = "convex_hull";
  sectors_[0].objects.push_back(obj);
  return nullptr; 
}

shared_ptr<GameObject> Renderer::CreateMeshFromAABB(const AABB& aabb) {
  return CreateCube(aabb.dimensions, aabb.point);
}

shared_ptr<GameObject> Renderer::CreatePlane(vec3 p1, vec3 p2, vec3 normal) {
  vec3 t = normalize(p2 - p1);
  vec3 b = cross(t, normal);

  float D = 10; 
  vector<vec3> vertices {
    p1 - t * D - b * D, // (-1, -1)
    p1 + t * D - b * D, // (+1, -1)
    p1 + t * D + b * D, // (+1, +1)
    p1 - t * D - b * D, // (-1, -1)
    p1 + t * D + b * D, // (+1, +1)
    p1 - t * D + b * D  // (-1, +1)
  };

  vector<vec2> uvs = {
    vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 0), vec2(1, 1), vec2(0, 1)
  };

  vector<unsigned int> indices(6);
  for (int i = 0; i < 6; i++) { indices[i] = i; }

  Mesh mesh = CreateMesh(shaders_["solid"], vertices, uvs, indices);
  shared_ptr<GameObject> obj = make_shared<GameObject>(mesh, vec3(0, 0, 0));
  obj->name = "plane";
  sectors_[0].objects.push_back(obj);
  return obj; 
}

shared_ptr<GameObject> Renderer::CreateJoint(vec3 start, vec3 end) {
  float h = length(end - start);
  float h1 = h * 0.05f;
  float w = h * 0.075f;
 
  vector<vec3> fixed_v {
    vec3(0, 0, 0), // Bottom.
    vec3(-w, h1, -w), vec3(w, h1, -w), vec3(w, h1, w), vec3(-w, h1, w), // Mid.
    vec3(0, h, 0) // Top.
  };

  vec3 target_axis = normalize(end - start);
  vec3 rotation_axis = cross(target_axis, vec3(0, 1, 0));
  float rotation_angle = acos(dot(target_axis, vec3(0, 1, 0)));
  quat my_quat = angleAxis(rotation_angle, rotation_axis);
  mat4 rotation_matrix = toMat4(my_quat);

  vector<vec3> v;
  for (auto& fv : fixed_v) {
    v.push_back(vec3(rotation_matrix * vec4(fv, 1)));
  }
  
  vector<vec3> vertices = {
    // Bottom.
    v[0], v[1], v[2], v[0], v[2], v[3],  v[0], v[3], v[4], v[0], v[4], v[1], 
    // Top.
    v[5], v[1], v[2], v[5], v[2], v[3],  v[5], v[3], v[4], v[5], v[4], v[1], 
  };

  vec2 u = vec2(0, 0);
  vector<glm::vec2> uvs {
    u, u, u, u, u, u, 
    u, u, u, u, u, u, 
    u, u, u, u, u, u, 
    u, u, u, u, u, u 
  };

  vector<unsigned int> indices(24);
  for (int i = 0; i < 24; i++) { indices[i] = i; }

  // Mesh mesh;
  Mesh mesh = CreateMesh(shaders_["solid"], vertices, uvs, indices);

  shared_ptr<GameObject> obj = make_shared<GameObject>(mesh, start);
  obj->name = "joint";
  sectors_[0].objects.push_back(obj);
  return obj; 
}

// ============================================
// FBX
// ============================================

// TODO: convert FBX to game model outside this file.
void Renderer::LoadFbx(const std::string& filename, vec3 position) {
  Mesh m;
  FbxData data = LoadFbxData(filename, m);
  m.shader = shaders_["animated_object"];

  // vec3 position = vec3(1990, 16, 2000);
  shared_ptr<GameObject> obj = make_shared<GameObject>(m, position);
  obj->name = filename;
  obj->polygons = m.polygons;
  obj->bounding_sphere = GetObjectBoundingSphere(obj);
  sectors_[0].objects.push_back(obj);
  position += vec3(0, 0, 5);

  // TODO: an animated object will have animations, if an animation is selected, 
  // a counter will count where in the animation is the model and update the
  // loaded joint transforms accordingly.
  if (data.animations.size() == 0) {
    cout << "No animations." << endl;
    return;
  }

  obj->joint_transforms.resize(data.joints.size());

  // TODO: load all animations.
  // TODO: load these joints transforms in the Fbx function.
  const Animation& animation = data.animations[1];
  for (auto& kf : animation.keyframes) {
    for (int i = 0; i < kf.transforms.size(); i++) {
      auto& joint = data.joints[i];
      if (!joint) continue;
      
      mat4 joint_transform = kf.transforms[i] * joint->global_bindpose_inverse;
      obj->joint_transforms[i].push_back(joint_transform);
    }
  }
}

int Renderer::LoadStaticFbx(const std::string& filename, vec3 position, int sector_id, int occluder_id) {
  Mesh m;
  LoadFbxData(filename, m);
  m.shader = shaders_["object"];

  shared_ptr<GameObject> obj = make_shared<GameObject>(m, position);
  obj->name = filename;
  obj->polygons = m.polygons;
  obj->occluder_id = occluder_id;
  obj->collide = true;

  obj->aabb = GetObjectAABB(obj);
  obj->bounding_sphere = GetObjectBoundingSphere(obj);
  obj->id = id_counter++;

  sectors_[sector_id].objects.push_back(obj);
  position += vec3(0, 0, 5);
  return obj->id;
}

void Renderer::LoadLOD(const std::string& filename, int id, int sector_id, int lod_level) {
  Mesh m;
  LoadFbxData(filename, m);
  m.shader = shaders_["object"];
 
  for (auto& obj : sectors_[sector_id].objects) {
    if (obj->id == id) {
      obj->lods[lod_level] = m;
    }
  }
}

// Given point p, return the point q on or in AABB b that is closest to p
vec3 ClosestPtPointAABB(vec3 p, vec3 aabb_min, vec3 aabb_max) {
  vec3 q;
  // For each coordinate axis, if the point coordinate value is
  // outside box, clamp it to the box, else keep it as is
  for (int i = 0; i < 3; i++) {
    float v = p[i];
    v = std::max(v, aabb_min[i]);
    v = std::min(v, aabb_max[i]);
    q[i] = v;
  }
  return q;
}





// =================================================
// Load visibility structures (occluders, sectors)
// =================================================

void Renderer::LoadSector(const std::string& filename, int id, vec3 position) {
  Mesh m;
  LoadFbxData(filename, m);

  Sector s;
  s.id = id;

  // TODO: additionally, calculate sphere bounding box and OBB for convex hull. 
  // TODO: polygons are being loaded as triangles. Correct that.
  s.convex_hull = m.polygons;
  for (auto& poly : s.convex_hull) {
    for (auto& v : poly.vertices) {
      v += position;
    }
  }

  s.stabbing_tree = make_shared<StabbingTreeNode>(1, 0, false);
  s.stabbing_tree->children.push_back(make_shared<StabbingTreeNode>(0, 0, true));
  sectors_[id] = s;
}

void Renderer::LoadPortal(const std::string& filename, int id, vec3 position) {
  Mesh m;
  LoadFbxData(filename, m);

  Portal p;
  p.polygons = m.polygons;
  for (auto& poly : p.polygons) {
    for (auto& v : poly.vertices) {
      v += position;
    }
  }
  portals_[id] = p;
}

void Renderer::LoadOccluder(const std::string& filename, int id, 
  vec3 position) {
  Mesh m;
  LoadFbxData(filename, m);

  Occluder o;
  o.polygons = m.polygons;
  for (auto& poly : o.polygons) {
    for (auto& v : poly.vertices) {
      v += position;
    }
  }
  occluders_[id] = o;
}
















// ============================================
// Collision Code
// ============================================
// TODO: move to another file

bool IntersectWall(const Polygon& polygon, vec3* player_pos, vec3 old_player_pos, float* magnitude, 
  const vec3& object_pos) {
  const vec3& normal = polygon.normals[0];

  vec3 aabb_min = *player_pos + vec3(-1, -1, -1);
  vec3 aabb_max = *player_pos + vec3(1, 1, 1);

  vec3 wall_aabb_min = vec3(9999999, 9999999, 9999999);
  vec3 wall_aabb_max = vec3(-999999, -999999, -999999);
  for (int i = 0; i < polygon.vertices.size(); i++) {
    const vec3& p = polygon.vertices[i] + object_pos;
    wall_aabb_min.x = std::min(wall_aabb_min.x, p.x);
    wall_aabb_min.y = std::min(wall_aabb_min.y, p.y);
    wall_aabb_min.z = std::min(wall_aabb_min.z, p.z);
    wall_aabb_max.x = std::max(wall_aabb_max.x, p.x);
    wall_aabb_max.y = std::max(wall_aabb_max.y, p.y);
    wall_aabb_max.z = std::max(wall_aabb_max.z, p.z);
  }

  vec3 closest_point = ClosestPtPointAABB(*player_pos, wall_aabb_min, wall_aabb_max);
  if (length(*player_pos - closest_point) > 1.5f) return false;

  // if (aabb_min.x > wall_aabb_max.x || aabb_min.y > wall_aabb_max.y || aabb_min.z > wall_aabb_max.z ||
  //     aabb_max.x < wall_aabb_min.x || aabb_max.y < wall_aabb_min.y || aabb_max.z < wall_aabb_min.z) {
  //   return false;
  // }

  // AABB positive extents.
  vec3 aabb_center = (aabb_min + aabb_max) * 0.5f;
  vec3 e = aabb_max - aabb_center;

  // Projection of the AABB box in the plane normal.
  float r = e.x * abs(normal.x) + e.y * abs(normal.y) + e.z * abs(normal.z);
  r = 1.5f;

  vec3 point_in_plane = polygon.vertices[0] + object_pos;
  float d = dot(point_in_plane, normal);

  // float s = dot(normal, aabb_center) - d;
  float s = dot(normal, *player_pos) - d;

  if (s < 0 || s > r) {
    return false;
  }

  vec3 p1 = polygon.vertices[0] + object_pos;
  vec3 p2 = polygon.vertices[2] + object_pos;
  vec3 player_p = *player_pos;
  p1.y = 0;
  p2.y = 0;
  player_p.y = 0;

  vec3 p1_ = p1 - player_p;
  vec3 p2_ = p2 - player_p;
  vec3 tangent1 = normalize(p1 - p2);
  vec3 tangent2 = normalize(p2 - p1);
  if (dot(p1_, tangent1) < 0) {
    if (length(p1_) < 1.5f) {
      cout << "Collision with point 1: " << p1 << endl;
      float tan_proj = abs(dot(p1_, tangent1));
      float normal_proj = abs(dot(p1_, normal));
      float mag = sqrt(1.5f * 1.5f - tan_proj * tan_proj) - normal_proj;
      *magnitude = mag;
      *player_pos = *player_pos + mag * normal;
      return true;
    }
    return false;
  } else if (dot(p2_, tangent2) < 0) {
    if (length(p2_) < 1.5f) {
      cout << "Collision with point 2: " << p2 << endl;
      float tan_proj = abs(dot(p2_, tangent2));
      float normal_proj = abs(dot(p2_, normal));
      float mag = sqrt(1.5f * 1.5f - tan_proj * tan_proj) - normal_proj;
      *magnitude = mag;
      *player_pos = *player_pos + mag * normal;
      return true;
    }
    return false;
  }

  cout << "Collision with center" << endl;

  // Resolve collision.
  float mag = abs(r - s) + 0.001f;
  *player_pos = *player_pos + mag * normal;

  float s2 = dot(normal, old_player_pos) - d;
  *magnitude = abs(r - s2);
  return true;
}

bool IntersectWithTriangle(const Polygon& polygon, vec3* player_pos, vec3 old_player_pos, 
  float* magnitude, const vec3& object_pos) {
  const vec3& normal = polygon.normals[0];

  vec3 a = polygon.vertices[0] + object_pos;
  vec3 b = polygon.vertices[1] + object_pos;
  vec3 c = polygon.vertices[2] + object_pos;

  bool inside;
  vec3 closest_point = ClosestPtPointTriangle(*player_pos, a, b, c, &inside);
  
  float r = 1.5f;

  const vec3& v = closest_point - *player_pos;
  if (length(v) > r || dot(*player_pos - a, normal) < 0) {
    return false;
  }

  if (!inside) {
    // cout << "not inside" << endl;
    vec3 closest_ab = ClosestPtPointSegment(*player_pos, a, b);
    vec3 closest_bc = ClosestPtPointSegment(*player_pos, b, c);
    vec3 closest_ca = ClosestPtPointSegment(*player_pos, c, a);
    vector<vec3> segments { a-b, b-c, c-a } ;
    vector<vec3> points { closest_ab, closest_bc, closest_ca } ;

    float min_distance = 9999.0f;
    float min_index = -1;
    for (int i = 0; i < 3; i++) {
      float distance = length(points[i] - *player_pos);
      if (distance < min_distance) {
        min_index = i;
        min_distance = distance;
      }
    }

    // Collide with segment.
    if (min_distance < r) {
      vec3 player_to_p = points[min_index] - *player_pos;
      vec3 tangent = normalize(cross(segments[min_index], normal));
      float proj_tan = abs(dot(player_to_p, tangent));
      float proj_normal = abs(dot(player_to_p, normal));
      float mag = sqrt(r * r - proj_tan * proj_tan) - proj_normal + 0.001f;
      *magnitude = mag;
      *player_pos = *player_pos + mag * normal;
      return true; 
    }
  }
  // cout << "inside" << endl;

  float mag = dot(v, normal);
  *magnitude = r + mag + 0.001f;
  *player_pos = *player_pos + *magnitude * normal;

  return true;
}

void Renderer::Collide(vec3* player_pos, vec3 old_player_pos, vec3* player_speed, bool* can_jump) {
  int current_sector_id = GetPlayerSector(camera_.position);

  // Collide with Octree.
  vector<shared_ptr<GameObject>> objs;
  if (current_sector_id == 0) {
    GetPotentiallyCollidingObjects(*player_pos, octree_, objs);
  } else {
    objs = sectors_[current_sector_id].objects;
  }

  bool collision = true;
  for (int i = 0; i < 5 && collision; i++) {
    vector<vec3> collision_resolutions;
    vector<float> magnitudes;
    collision = false;

    for (auto& obj : objs) {
      if (!obj->collide) {
        continue;
      }
      for (auto& pol : obj->polygons) {
        vec3 collision_resolution = *player_pos;
        float magnitude;
        if (IntersectWithTriangle(pol, &collision_resolution, old_player_pos, &magnitude, obj->position)) {
        // if (IntersectWall(pol, &collision_resolution, old_player_pos, &magnitude, obj->position)) {
          // cout << "Intersect" << endl;
          collision = true;
          collision_resolutions.push_back(collision_resolution);
          magnitudes.push_back(magnitude);
        }
      }
    }

    // Select the collision resolution for which the collision displacement along
    // the collision normal is minimal.
    int min_index = -1;
    float min_magnitude = 999999999.0f;
    for (int i = 0; i < collision_resolutions.size(); i++) {
      const vec3& collision_resolution = collision_resolutions[i];
      const float magnitude = magnitudes[i];
      if (magnitude < min_magnitude) {
        min_index = i;
        min_magnitude = magnitude;
      }
    }

    if (min_index != -1) {
      // cout << min_index << " - choice: " << collision_resolutions[min_index] << endl;
      vec3 collision_vector = normalize(collision_resolutions[min_index] - *player_pos);
      *player_speed += abs(dot(*player_speed, collision_vector)) * collision_vector;
      *player_pos = collision_resolutions[min_index];

      if (dot(collision_vector, vec3(0, 1, 0)) > 0.5f) {
        *can_jump = true;
      }
    }
  }
  // cout << "=========" << endl;
}
