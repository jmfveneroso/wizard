#include "asset.hpp"

// void LoadFbx(const std::string& filename, vec3 position) {
//   FbxData data = FbxLoad(filename);
//   auto& vertices = data.vertices;
//   auto& uvs = data.uvs;
//   auto& normals = data.normals;
//   auto& indices = data.indices;
//   auto& bone_ids = data.bone_ids;
//   auto& bone_weights = data.bone_weights;
// 
//   MeshData m;
//   m.shader = shaders_["animated_object"];
//   glGenBuffers(1, &m.vertex_buffer_);
//   glGenBuffers(1, &m.uv_buffer_);
//   glGenBuffers(1, &m.normal_buffer_);
//   glGenBuffers(1, &m.element_buffer_);
// 
//   vector<vec3> _vertices;
//   vector<vec2> _uvs;
//   vector<vec3> _normals;
//   vector<ivec3> _bone_ids;
//   vector<vec3> _bone_weights;
//   vector<unsigned int> _indices;
//   for (int i = 0; i < indices.size(); i++) {
//     _vertices.push_back(vertices[indices[i]]);
//     _uvs.push_back(uvs[i]);
//     _normals.push_back(normals[i]);
//     _bone_ids.push_back(bone_ids[indices[i]]);
//     _bone_weights.push_back(bone_weights[indices[i]]);
//     _indices.push_back(i);
//   }
// 
//   glBindBuffer(GL_ARRAY_BUFFER, m.vertex_buffer_);
//   glBufferData(GL_ARRAY_BUFFER, _vertices.size() * sizeof(glm::vec3), 
//     &_vertices[0], GL_STATIC_DRAW);
// 
//   glBindBuffer(GL_ARRAY_BUFFER, m.uv_buffer_);
//   glBufferData(GL_ARRAY_BUFFER, _uvs.size() * sizeof(glm::vec2), 
//     &_uvs[0], GL_STATIC_DRAW);
// 
//   glBindBuffer(GL_ARRAY_BUFFER, m.normal_buffer_);
//   glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), 
//     &_normals[0], GL_STATIC_DRAW);
// 
//   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_); 
//   glBufferData(
//     GL_ELEMENT_ARRAY_BUFFER, 
//     _indices.size() * sizeof(unsigned int), 
//     &_indices[0], 
//     GL_STATIC_DRAW
//   );
//   m.num_indices = _indices.size();
// 
//   GLuint bone_id_buffer;
//   glGenBuffers(1, &bone_id_buffer);
//   glBindBuffer(GL_ARRAY_BUFFER, bone_id_buffer);
//   glBufferData(GL_ARRAY_BUFFER, _bone_ids.size() * sizeof(glm::ivec3), 
//     &_bone_ids[0], GL_STATIC_DRAW);
// 
//   GLuint weight_buffer;
//   glGenBuffers(1, &weight_buffer);
//   glBindBuffer(GL_ARRAY_BUFFER, weight_buffer);
//   glBufferData(GL_ARRAY_BUFFER, _bone_weights.size() * sizeof(glm::vec3), 
//     &_bone_weights[0], GL_STATIC_DRAW);
// 
//   glGenVertexArrays(1, &m.vao_);
//   glBindVertexArray(m.vao_);
// 
//   BindBuffer(m.vertex_buffer_, 0, 3);
//   BindBuffer(m.uv_buffer_, 1, 2);
//   BindBuffer(m.normal_buffer_, 2, 3);
//   BindBuffer(bone_id_buffer, 3, 3);
//   BindBuffer(weight_buffer, 4, 3);
// 
//   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_);
// 
//   glBindVertexArray(0);
//   for (int slot = 0; slot < 5; slot++) {
//     glDisableVertexAttribArray(slot);
//   }
// 
//   //    // vec3 position = vec3(1990, 16, 2000);
//   //    shared_ptr<Object3D> obj = make_shared<Object3D>(m, position);
//   //    obj->name = filename;
//   //    m.polygons = data.polygons;
//   //    obj->polygons = m.polygons;
//   //    obj->bounding_sphere = GetObjectBoundingSphere(obj);
//   //    sectors_[0].objects.push_back(obj);
//   //    position += vec3(0, 0, 5);
// 
//   //    // CreateSkeletonAux(translate(position), data.skeleton);
// 
//   //    // TODO: an animated object will have animations, if an animation is selected, 
//   //    // a counter will count where in the animation is the model and update the
//   //    // loaded joint transforms accordingly.
//   //    if (data.animations.size() == 0) {
//   //      cout << "No animations." << endl;
//   //      return;
//   //    }
// 
//   //    joint_transforms_.resize(data.joints.size());
//   //    const Animation& animation = data.animations[1];
//   //    for (auto& kf : animation.keyframes) {
//   //      for (int i = 0; i < kf.transforms.size(); i++) {
//   //        auto& joint = data.joints[i];
//   //        if (!joint) continue;
//   //        
//   //        mat4 joint_transform = kf.transforms[i] * joint->global_bindpose_inverse;
//   //        joint_transforms_[i].push_back(joint_transform);
//   //      }
//   //    }
// }

// Mesh LoadFbxMesh(const std::string& filename) {
//   FbxData data = FbxLoad(filename);
//   auto& vertices = data.vertices;
//   auto& uvs = data.uvs;
//   auto& normals = data.normals;
//   auto& indices = data.indices;
// 
//   Mesh m;
//   m.shader = shaders_["object"];
//   glGenBuffers(1, &m.vertex_buffer_);
//   glGenBuffers(1, &m.uv_buffer_);
//   glGenBuffers(1, &m.normal_buffer_);
//   glGenBuffers(1, &m.element_buffer_);
// 
//   vector<vec3> _vertices;
//   vector<vec2> _uvs;
//   vector<vec3> _normals;
//   vector<unsigned int> _indices;
//   for (int i = 0; i < indices.size(); i++) {
//     _vertices.push_back(vertices[indices[i]]);
//     _uvs.push_back(uvs[i]);
//     _normals.push_back(normals[i]);
//     _indices.push_back(i);
//   }
// 
//   glBindBuffer(GL_ARRAY_BUFFER, m.vertex_buffer_);
//   glBufferData(GL_ARRAY_BUFFER, _vertices.size() * sizeof(glm::vec3), 
//     &_vertices[0], GL_STATIC_DRAW);
// 
//   glBindBuffer(GL_ARRAY_BUFFER, m.uv_buffer_);
//   glBufferData(GL_ARRAY_BUFFER, _uvs.size() * sizeof(glm::vec2), 
//     &_uvs[0], GL_STATIC_DRAW);
// 
//   glBindBuffer(GL_ARRAY_BUFFER, m.normal_buffer_);
//   glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), 
//     &_normals[0], GL_STATIC_DRAW);
// 
//   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_); 
//   glBufferData(
//     GL_ELEMENT_ARRAY_BUFFER, 
//     _indices.size() * sizeof(unsigned int), 
//     &_indices[0], 
//     GL_STATIC_DRAW
//   );
//   m.num_indices = _indices.size();
// 
//   glGenVertexArrays(1, &m.vao_);
//   glBindVertexArray(m.vao_);
// 
//   BindBuffer(m.vertex_buffer_, 0, 3);
//   BindBuffer(m.uv_buffer_, 1, 2);
//   BindBuffer(m.normal_buffer_, 2, 3);
// 
//   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_);
// 
//   glBindVertexArray(0);
//   for (int slot = 0; slot < 5; slot++) {
//     glDisableVertexAttribArray(slot);
//   }
//   m.polygons = data.polygons;
//   return m;
// }
// 
// int LoadStaticFbx(const std::string& filename, vec3 position, int sector_id, int occluder_id) {
//   Mesh m = LoadFbxMesh(filename);
// 
//   shared_ptr<Object3D> obj = make_shared<Object3D>(m, position);
//   obj->name = filename;
//   obj->polygons = m.polygons;
//   obj->occluder_id = occluder_id;
//   obj->collide = true;
// 
//   obj->aabb = GetObjectAABB(obj);
//   obj->bounding_sphere = GetObjectBoundingSphere(obj);
//   obj->id = id_counter++;
// 
//   sectors_[sector_id].objects.push_back(obj);
//   position += vec3(0, 0, 5);
//   return obj->id;
// }
// 
// void LoadLOD(const std::string& filename, int id, int sector_id, int lod_level) {
//   Mesh m = LoadFbxMesh(filename);
//  
//   for (auto& obj : sectors_[sector_id].objects) {
//     if (obj->id == id) {
//       obj->lods[lod_level] = m;
//     }
//   }
// }
