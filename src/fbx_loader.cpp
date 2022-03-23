#include <fbxsdk.h>
#include <iostream>
#include "fbx_loader.hpp"

using namespace std;
using namespace glm;

mutex gMutex;

vec2 Get2DVector(FbxVector2 pValue) {
  return vec2(pValue[0], pValue[1]);
}

vec3 Get3DVector(FbxVector4 pValue) {
  return vec3(pValue[0], pValue[2], pValue[1]);
}

vec4 Get4DVector(FbxVector4 pValue) {
  return vec4(pValue[0], pValue[2], pValue[1], pValue[3]);
}

mat4 GetMatrix(const FbxMatrix& lMatrix) {
  mat4 result;
  for (int k = 0; k < 4; k++) {
    result[k] = Get4DVector(lMatrix.GetRow(k));
  }
  return result;
}

// Pretty much equivalent to the sample.
FbxScene* ImportScene(const std::string& filename) {
  FbxManager* sdk_manager = FbxManager::Create();
  FbxIOSettings* ios = FbxIOSettings::Create(sdk_manager, IOSROOT);
  sdk_manager->SetIOSettings(ios);

  FbxImporter* importer = FbxImporter::Create(sdk_manager, "");
  if (!importer->Initialize(filename.c_str(), -1, sdk_manager->GetIOSettings())) { 
    throw runtime_error(importer->GetStatus().GetErrorString()); 
  }

  FbxScene* scene = FbxScene::Create(sdk_manager, "scene");
  importer->Import(scene);
  importer->Destroy();
  return scene;
}

// FBX Utility.
FbxAMatrix GetGeometryTransformation(FbxNode* node) { 
  if (!node) throw; 
  const FbxVector4 t = node->GetGeometricTranslation(FbxNode::eSourcePivot); 
  const FbxVector4 r = node->GetGeometricRotation(FbxNode::eSourcePivot); 
  const FbxVector4 s = node->GetGeometricScaling(FbxNode::eSourcePivot); 
  return FbxAMatrix(t, r, s); 
} 

// FBX Utility.
FbxNode* GetNodeWithAttribute(FbxNode* node, FbxNodeAttribute::EType attr) {
  if (!node) return nullptr;

  if (node->GetNodeAttribute() != NULL && 
      node->GetNodeAttribute()->GetAttributeType() == attr) {
    return node;
  }

  for (int i = 0; i < node->GetChildCount(); i++) {
    FbxNode* res = GetNodeWithAttribute(node->GetChild(i), attr);
    if (res) return res;
  }
  return nullptr;
}

shared_ptr<SkeletonJoint> BuildSkeleton(FbxNode* node) {
  if (!node) return nullptr;

  shared_ptr<SkeletonJoint> joint = make_shared<SkeletonJoint>();
  if (node->GetNodeAttribute() != NULL && 
      node->GetNodeAttribute()->GetAttributeType() == 
        FbxNodeAttribute::eSkeleton) {
    FbxSkeleton* lSkeleton = (FbxSkeleton*) node->GetNodeAttribute();
    joint->name = (char*) lSkeleton->GetName();
    joint->node = node;
  }

  for (int i = 0; i < node->GetChildCount(); i++) {
    shared_ptr<SkeletonJoint> child = BuildSkeleton(node->GetChild(i));
    if (!child) continue;
    joint->children.push_back(child);
  }
  return joint;
}

void ExtractSkeleton(FbxScene* scene, FbxData* data) {
  FbxNode* skeleton_node = GetNodeWithAttribute(scene->GetRootNode(), 
    FbxNodeAttribute::eSkeleton);
  if (!skeleton_node) return;

  data->skeleton = BuildSkeleton(skeleton_node);

  queue<FbxNode*> nodes;
  nodes.push(scene->GetRootNode());
  while (!nodes.empty()) {
    FbxNode* node = nodes.front();
    nodes.pop();

    if (node->GetNodeAttribute() != NULL && 
        node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton) {
      FbxSkeleton* lSkeleton = (FbxSkeleton*) node->GetNodeAttribute();
      shared_ptr<SkeletonJoint> joint = make_shared<SkeletonJoint>();
      joint->name = (char*) lSkeleton->GetName();
      joint->node = node;
      data->joint_map[joint->name] = joint;

      FbxTime time; 
      time.SetFrame(0, FbxTime::eFrames24); // TODO: 60 frames.
      FbxAMatrix transform_offset = node->EvaluateGlobalTransform(time) * 
        GetGeometryTransformation(scene->GetRootNode());
      FbxAMatrix mGlobalTransform = transform_offset.Inverse() *
        joint->node->EvaluateGlobalTransform(time); 
      joint->global_bindpose = GetMatrix(mGlobalTransform);
    }

    for (int i = 0; i < node->GetChildCount(); i++) {
      nodes.push(node->GetChild(i));
    }
  }
}

void ExtractPolygon(FbxMesh* mesh, int i, FbxData* data, int& vertex_id) {
  vector<unsigned int> vertices;
  vector<vec2> uvs;
  vector<vec3> normals;
  vector<vec3> tangents;
  vector<vec3> binormals;

  int polygon_size = mesh->GetPolygonSize(i);
  for (int j = 0; j < polygon_size; j++, vertex_id++) {
    int vertex_index = mesh->GetPolygonVertex(i, j);
    if (vertex_index < 0) {
      throw runtime_error("Invalid vertex index.");
    } else {
      vertices.push_back(vertex_index);
    }

    int element_uv_count = mesh->GetElementUVCount();
    if (element_uv_count == 0) continue;

    for (int k = 0; k < 1; ++k) {
      FbxGeometryElementUV* uv = mesh->GetElementUV(k);
      switch (uv->GetMappingMode()) {
        default:
          break;
        case FbxGeometryElement::eByControlPoint:
          switch (uv->GetReferenceMode()) {
            case FbxGeometryElement::eDirect:
              uvs.push_back(Get2DVector(uv->GetDirectArray()
                .GetAt(vertex_index)));
              break;
            case FbxGeometryElement::eIndexToDirect: {
              int id = uv->GetIndexArray().GetAt(vertex_index);
              uvs.push_back(
                Get2DVector(uv->GetDirectArray().GetAt(id)));
            }
            break;
            default:
              break;
          }
          break;
        case FbxGeometryElement::eByPolygonVertex: {
          int lTextureUVIndex = mesh->GetTextureUVIndex(i, j);
          switch (uv->GetReferenceMode()) {
            case FbxGeometryElement::eDirect:
            case FbxGeometryElement::eIndexToDirect: {
              vec2 uv_vec = Get2DVector(
                uv->GetDirectArray().GetAt(lTextureUVIndex));
              uvs.push_back(uv_vec);
            }
            break;
            default:
              break;
          }
        }
        break;
      }
    }

    if (data->name == "resources/models_fbx/mausoleum_stone.fbx") {
      // cout << "NC: " << mesh->GetElementNormalCount() << endl;
    }

    for (int k = 0; k < mesh->GetElementNormalCount(); ++k) {
      FbxGeometryElementNormal* normal = mesh->GetElementNormal(k);
      if (normal->GetMappingMode() == 
        FbxGeometryElement::eByPolygonVertex) {
        switch (normal->GetReferenceMode()) {
          case FbxGeometryElement::eDirect:
            normals.push_back(Get3DVector(normal->GetDirectArray()
              .GetAt(vertex_id)));
            break;
          case FbxGeometryElement::eIndexToDirect: {
            int id = normal->GetIndexArray().GetAt(vertex_id);
            normals.push_back(Get3DVector(normal->GetDirectArray()
              .GetAt(id)));
          }
          break;
          default:
            break;
        }
      }
    }

    if (data->name == "resources/models_fbx/mausoleum_stone.fbx") {
      // cout << "TC: " << mesh->GetElementTangentCount() << endl;
    }

    for (int k = 0; k < mesh->GetElementTangentCount(); ++k) {
      FbxGeometryElementTangent* tangent = mesh->GetElementTangent(k);
      if (tangent->GetMappingMode() == 
        FbxGeometryElement::eByPolygonVertex) {
        switch (tangent->GetReferenceMode()) {
          case FbxGeometryElement::eDirect:
            tangents.push_back(Get3DVector(tangent->GetDirectArray()
              .GetAt(vertex_id)));
            break;
          case FbxGeometryElement::eIndexToDirect: {
            int id = tangent->GetIndexArray().GetAt(vertex_id);
            tangents.push_back(Get3DVector(tangent->GetDirectArray()
              .GetAt(id)));
          }
          break;
          default:
            break;
        }
      }
    }

    if (data->name == "resources/models_fbx/mausoleum_stone.fbx") {
      // cout << "BC: " << mesh->GetElementBinormalCount() << endl;
    }

    for (int k = 0; k < mesh->GetElementBinormalCount(); ++k) {
      FbxGeometryElementBinormal* binormal = mesh->GetElementBinormal(k);
      if (binormal->GetMappingMode() == 
        FbxGeometryElement::eByPolygonVertex) {
        switch (binormal->GetReferenceMode()) {
          case FbxGeometryElement::eDirect:
            binormals.push_back(Get3DVector(binormal->GetDirectArray()
              .GetAt(vertex_id)));
            break;
          case FbxGeometryElement::eIndexToDirect: {
            int id = binormal->GetIndexArray().GetAt(vertex_id);
            binormals.push_back(Get3DVector(binormal->GetDirectArray()
              .GetAt(id)));
          }
          break;
          default:
            break;
        }
      }
    }
  }

  // Triangulate the polygon with the triangle fan triangulation method.
  for (int j = 1; j < polygon_size - 1; j++) {
    data->raw_mesh.indices.push_back(vertices[0]);
    data->raw_mesh.uvs.push_back(uvs[0]);
    data->raw_mesh.normals.push_back(normals[0]);
    data->raw_mesh.indices.push_back(vertices[j]);
    data->raw_mesh.uvs.push_back(uvs[j]);
    data->raw_mesh.normals.push_back(normals[j]);
    data->raw_mesh.indices.push_back(vertices[j+1]);
    data->raw_mesh.uvs.push_back(uvs[j+1]);
    data->raw_mesh.normals.push_back(normals[j+1]);

    if (!tangents.empty()) {
      data->raw_mesh.tangents.push_back(tangents[0]);
      data->raw_mesh.tangents.push_back(tangents[j]);
      data->raw_mesh.tangents.push_back(tangents[j+1]);
      data->raw_mesh.binormals.push_back(binormals[0]);
      data->raw_mesh.binormals.push_back(binormals[j]);
      data->raw_mesh.binormals.push_back(binormals[j+1]);
    }

    Polygon polygon;
    polygon.vertices.push_back(data->raw_mesh.vertices[vertices[0]]);
    polygon.vertices.push_back(data->raw_mesh.vertices[vertices[j]]);
    polygon.vertices.push_back(data->raw_mesh.vertices[vertices[j+1]]);
    polygon.normal = normals[0];
    data->raw_mesh.polygons.push_back(polygon);
  }
  // cout << "indices: " << data->indices.size() << endl;
  // cout << "uvs: " << data->uvs.size() << endl;
  // cout << "normals: " << data->normals.size() << endl;
  // cout << "polygons: " << data->polygons.size() << endl;
}

// Extract polygon data.
void ExtractMesh(FbxScene* scene, FbxData* data) {
  FbxNode* node = 
    GetNodeWithAttribute(scene->GetRootNode(), FbxNodeAttribute::eMesh);
  FbxMesh* mesh = (FbxMesh*) node->GetNodeAttribute();

  FbxVector4* vertices = mesh->GetControlPoints();
  int num_vertices = mesh->GetControlPointsCount();
  for (int i = 0; i < num_vertices; i++) {
    data->raw_mesh.vertices.push_back(Get3DVector(vertices[i]));
  }

  int vertex_id = 0;
  for (int i = 0; i < mesh->GetPolygonCount(); i++) {
    ExtractPolygon(mesh, i, data, vertex_id);
  }
}

// Comparison function to sort the vector elements 
// by second element of tuples 
bool SortBySec(const tuple<int, float>& a, const tuple<int, float>& b) { 
  return (get<1>(a) < get<1>(b)); 
} 

// Reference: https://www.gamedev.net/articles/programming/graphics/how-to-work-with-fbx-sdk-r3582/
void ExtractSkin(FbxScene* scene, FbxData* data) {
  FbxNode* node = 
    GetNodeWithAttribute(scene->GetRootNode(), FbxNodeAttribute::eMesh);

  FbxMesh* mesh = node->GetMesh(); 
  if (mesh->GetDeformerCount() == 0) {
    return;
  } else if (mesh->GetDeformerCount() > 1) {
    throw runtime_error("Wrong number of deformers.");
  }

  // unordered_map<string, shared_ptr<SkeletonJoint>> joint_map;
  // stack<shared_ptr<SkeletonJoint>> joint_stack({ data->skeleton });
  // while (!joint_stack.empty()) {
  //   shared_ptr<SkeletonJoint> joint = joint_stack.top();
  //   joint_stack.pop();
  //   for (auto& c : joint->children) joint_stack.push(c);
  //   joint_map[joint->name] = joint;

  //   cout << "Adding joint " << joint->name << endl;

  //   // Old way.
  //   // joint->global_bindpose = transform_link_matrix * inverse(transform_matrix) * 
  //   //   geometry_transform;

  //   FbxTime time; 
  //   time.SetFrame(0, FbxTime::eFrames24); 
  //   FbxAMatrix transform_offset = node->EvaluateGlobalTransform(time) * 
  //     GetGeometryTransformation(scene->GetRootNode());
  //   FbxAMatrix mGlobalTransform = transform_offset.Inverse() *
  //     joint->node->EvaluateGlobalTransform(time); 
  //   joint->global_bindpose = GetMatrix(mGlobalTransform);
  // }

  unordered_map<string, shared_ptr<SkeletonJoint>>& joint_map = data->joint_map;

  vector<vector<tuple<int, float>>> bone_weights(data->raw_mesh.vertices.size());
  data->joints.resize(joint_map.size());

  mat4 geometry_transform = 
    GetMatrix(GetGeometryTransformation(scene->GetRootNode()));

  FbxSkin* skin = (FbxSkin*) mesh->GetDeformer(0, FbxDeformer::eSkin); 
  if (!skin) return;

  int num_clusters = skin->GetClusterCount(); 
  for (int i = 0; i < num_clusters; ++i) {
    FbxCluster* cluster = skin->GetCluster(i);
    if (!cluster) continue;

    string name = (char*) cluster->GetLink()->GetName();
    if (joint_map.find(name) == joint_map.end()) {
      // throw runtime_error(string("Joint ") + name + " not found.");
      cout << "Joint " << name << " not found" << endl;
      continue;
    }

    shared_ptr<SkeletonJoint> joint = joint_map[name];
    joint->cluster = cluster;
    data->joints[i] = joint;

    int index_count = cluster->GetControlPointIndicesCount();
    int* indices = cluster->GetControlPointIndices();
    double* weights = cluster->GetControlPointWeights();
    for (int j = 0; j < index_count; j++) {
      bone_weights[indices[j]].push_back(make_tuple(i, weights[j]));
    }

    FbxAMatrix m;
    cluster->GetTransformMatrix(m); 
    mat4 transform_matrix = GetMatrix(m);
    cluster->GetTransformLinkMatrix(m);
    mat4 transform_link_matrix = GetMatrix(m);

    joint->global_bindpose_inverse = inverse(transform_link_matrix) * 
      transform_matrix * geometry_transform;
  }

  for (int i = 0; i < bone_weights.size(); i++) {
    ivec3 ids(-1, -1, -1);
    vec3 weights(0, 0, 0);
    sort(bone_weights[i].begin(), bone_weights[i].end(), SortBySec);

    for (int j = 0; j < 3; j++) {
      if (j >= bone_weights[i].size()) break;
      ids[j] = get<0>(bone_weights[i][j]);
      weights[j] = get<1>(bone_weights[i][j]);
    }

    weights = weights * (1.0f / (weights[0] + weights[1] + weights[2]));
    data->raw_mesh.bone_ids.push_back(ids);
    data->raw_mesh.bone_weights.push_back(weights);
  }
}

void ExtractAnimations(FbxScene* scene, FbxData* data) {
  FbxNode* node = 
    GetNodeWithAttribute(scene->GetRootNode(), FbxNodeAttribute::eMesh);

  FbxAMatrix geometry_transform = 
    GetGeometryTransformation(scene->GetRootNode());

  for (int i = 0; i < scene->GetSrcObjectCount<FbxAnimStack>(); i++) {
    FbxAnimStack* anim_stack = scene->GetSrcObject<FbxAnimStack>(i);
    if (!anim_stack) {
      cout << "Anim stack does not exist" << endl;
      continue;
    }

    Animation animation;
    FbxString anim_stack_name = anim_stack->GetName(); 
    animation.name = anim_stack_name.Buffer();

    scene->SetCurrentAnimationStack(anim_stack);

    FbxTakeInfo* take_info = scene->GetTakeInfo(anim_stack_name); 
    FbxTime start = take_info->mLocalTimeSpan.GetStart(); 
    FbxTime end = take_info->mLocalTimeSpan.GetStop(); 
    FbxLongLong frame_count = end.GetFrameCount(FbxTime::eFrames24);
    for (FbxLongLong i = start.GetFrameCount(FbxTime::eFrames24); 
      i <= frame_count; ++i) { 
      FbxTime time; 
      time.SetFrame(i, FbxTime::eFrames24); 

      Keyframe keyframe;
      keyframe.time = i;
      keyframe.transforms.resize(data->joints.size());
      for (int j = 0; j < data->joints.size(); j++) {
        auto& joint = data->joints[j];
        if (!joint) continue;

        FbxAMatrix transform_offset = node->EvaluateGlobalTransform(time) * 
          geometry_transform;
        FbxAMatrix mGlobalTransform = transform_offset.Inverse() * 
          joint->cluster->GetLink()->EvaluateGlobalTransform(time); 
        mat4 m = GetMatrix(mGlobalTransform);
        keyframe.transforms[j] = m * joint->global_bindpose_inverse;
      }
      animation.keyframes.push_back(keyframe);
    }
    // cout << "Extracted animation " << animation.name << endl;
    data->raw_mesh.animations.push_back(animation);
  }
}

void FbxLoad(const std::string& filename, FbxData& data) {
  FbxScene* scene = ImportScene(filename);

  // cout << "Extracting FBX: " << filename << endl;
  ExtractMesh(scene, &data);

  // Animation.
  ExtractSkeleton(scene, &data);
  ExtractSkin(scene, &data);
  ExtractAnimations(scene, &data);
}

void LoadFbxData(const std::string& filename, Mesh& m, FbxData& data, bool calculate_bs) {
  data.name = filename;
  if (filename == "resources/models_fbx/mausoleum_stone.fbx") {
    cout << "-------->>>>>>>>>-------- " << filename << endl;
    cout << "-------->>>>>>>>>-------- " << filename << endl;
    cout << "-------->>>>>>>>>-------- " << filename << endl;
    cout << "-------->>>>>>>>>-------- " << filename << endl;
    cout << "-------->>>>>>>>>-------- " << filename << endl;
    cout << "-------->>>>>>>>>-------- " << filename << endl;
    cout << "-------->>>>>>>>>-------- " << filename << endl;
    cout << "-------->>>>>>>>>-------- " << filename << endl;
    cout << "-------->>>>>>>>>-------- " << filename << endl;
    cout << "-------->>>>>>>>>-------- " << filename << endl;
    cout << "-------->>>>>>>>>-------- " << filename << endl;
  } else {
    cout << "------------------ " << filename << endl;
  }
  FbxLoad(filename, data);

  m.polygons = data.raw_mesh.polygons;

  GLuint buffers[8];
  glGenBuffers(8, buffers);

  vector<vec3> vertices;
  vector<unsigned int> indices;
  for (int i = 0; i < data.raw_mesh.indices.size(); i++) {
    vertices.push_back(data.raw_mesh.vertices[data.raw_mesh.indices[i]]);
    indices.push_back(i);
  }
  m.num_indices = indices.size();

  glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vec3), 
    &vertices[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
  glBufferData(GL_ARRAY_BUFFER, data.raw_mesh.uvs.size() * sizeof(vec2), 
    &data.raw_mesh.uvs[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, buffers[2]);
  glBufferData(GL_ARRAY_BUFFER, data.raw_mesh.normals.size() * sizeof(vec3), 
    &data.raw_mesh.normals[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[3]); 
  glBufferData(
    GL_ELEMENT_ARRAY_BUFFER, 
    indices.size() * sizeof(unsigned int), 
    &indices[0], 
    GL_STATIC_DRAW
  );

  glGenVertexArrays(1, &m.vao_);
  glBindVertexArray(m.vao_);

  int num_slots = 6;
  BindBuffer(buffers[0], 0, 3);
  BindBuffer(buffers[1], 1, 2);
  BindBuffer(buffers[2], 2, 3);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[3]);

  cout << "Raw Vertices: " << data.raw_mesh.vertices.size() << endl;
  cout << "Vertices: " << vertices.size() << endl;
  cout << "UVs: " << data.raw_mesh.uvs.size() << endl;
  cout << "Normals: " << data.raw_mesh.normals.size() << endl;
  cout << "Tangents: " << data.raw_mesh.tangents.size() << endl;
  cout << "Binormals: " << data.raw_mesh.binormals.size() << endl;

  // Compute tangents and bitangents.
  vector<vec3> tangents(vertices.size());
  vector<vec3> bitangents(vertices.size());
  for (int i = 0; i < vertices.size(); i += 3) {
    vec3 delta_pos1 = vertices[i+1] - vertices[i+0];
    vec3 delta_pos2 = vertices[i+2] - vertices[i+0];
    vec2 delta_uv1 = data.raw_mesh.uvs[i+1] - data.raw_mesh.uvs[i+0];
    vec2 delta_uv2 = data.raw_mesh.uvs[i+2] - data.raw_mesh.uvs[i+0];

    float r = 1.0f / (delta_uv1.x * delta_uv2.y - delta_uv1.y * delta_uv2.x);
    vec3 tangent = normalize((delta_pos1 * delta_uv2.y - delta_pos2 * delta_uv1.y) * r);
    vec3 bitangent = normalize((delta_pos2 * delta_uv1.x - delta_pos1 * delta_uv2.x) * r);

    vec3 normal = (data.raw_mesh.normals[i] + data.raw_mesh.normals[i+1] + data.raw_mesh.normals[i+2]) / 3.0f;
    // vec3 normal = data.raw_mesh.normals[i];
    vec3 face_normal = normalize(cross(tangent, bitangent));
    if (dot(normal, face_normal) < 0) face_normal = -face_normal;

    // bitangent = cross(normal, tangent);
    // cout << "Actual face normal: " << face_normal << endl;
    // cout << "Normal: " << data.raw_mesh.normals[i] << endl;
    // cout << "Normal 1: " << data.raw_mesh.normals[i+1] << endl;
    // cout << "Normal 2: " << data.raw_mesh.normals[i+2] << endl;
    // cout << "Normalized n: " << normal << endl;

    quat target_rotation = RotationBetweenVectors(face_normal, normal);
    mat4 rotation_matrix = mat4_cast(target_rotation);
    // cout << "tangent: " << tangent << endl;
    // cout << "bitangent: " << bitangent << endl;
    tangent = vec3(rotation_matrix * vec4(tangent, 1.0));
    bitangent = vec3(rotation_matrix * vec4(bitangent, 1.0));
    // cout << "tangent A: " << tangent << endl;
    // cout << "bitangent A: " << bitangent << endl;

    tangents[i+0] = tangent;
    tangents[i+1] = tangent;
    tangents[i+2] = tangent;
    bitangents[i+0] = bitangent;
    bitangents[i+1] = bitangent;
    bitangents[i+2] = bitangent;
  }

  if (!data.raw_mesh.tangents.empty()) {
    cout << "ADDING>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> " << filename << endl;
    cout << "ADDING>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> " << filename << endl;
    cout << "ADDING>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> " << filename << endl;
    glBindBuffer(GL_ARRAY_BUFFER, buffers[4]);
    glBufferData(GL_ARRAY_BUFFER, data.raw_mesh.tangents.size() * sizeof(vec3), 
      &data.raw_mesh.tangents[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[5]);
    glBufferData(GL_ARRAY_BUFFER, data.raw_mesh.binormals.size() * sizeof(vec3), 
      &data.raw_mesh.binormals[0], GL_STATIC_DRAW);
  } else {
    glBindBuffer(GL_ARRAY_BUFFER, buffers[4]);
    glBufferData(GL_ARRAY_BUFFER, tangents.size() * sizeof(vec3), 
      &tangents[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[5]);
    glBufferData(GL_ARRAY_BUFFER, bitangents.size() * sizeof(vec3), 
      &bitangents[0], GL_STATIC_DRAW);
  }

  BindBuffer(buffers[4], 3, 3);
  BindBuffer(buffers[5], 4, 3);

  vector<ivec3> bone_ids;
  vector<vec3> bone_weights;
  if (!data.raw_mesh.bone_ids.empty()) {
    for (int i = 0; i < data.raw_mesh.indices.size(); i++) {
      bone_ids.push_back(data.raw_mesh.bone_ids[data.raw_mesh.indices[i]]);
      bone_weights.push_back(data.raw_mesh.bone_weights[data.raw_mesh.indices[i]]);
    }

    glBindBuffer(GL_ARRAY_BUFFER, buffers[6]);
    glBufferData(GL_ARRAY_BUFFER, bone_ids.size() * sizeof(glm::ivec3), 
      &bone_ids[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[7]);
    glBufferData(GL_ARRAY_BUFFER, bone_weights.size() * sizeof(glm::vec3), 
      &bone_weights[0], GL_STATIC_DRAW);

    num_slots += 2;
    BindBuffer(buffers[6], 5, 3);
    BindBuffer(buffers[7], 6, 3);
  }

  glBindVertexArray(0);
  for (int slot = 0; slot < num_slots; slot++) {
    glDisableVertexAttribArray(slot);
  }

  m.bounding_sphere = GetAssetBoundingSphere(m.polygons);

  // Animations.
  if (data.raw_mesh.animations.empty()) {
    return;
  }

  for (const Animation& animation : data.raw_mesh.animations) {
    m.animations[animation.name] = animation;

    // Calculate BS.
    // if (!calculate_bs) continue;
    // for (int frame = 0; frame < m.animations[animation.name].keyframes.size(); frame++) {
    //   vector<vec3> vertices;

    //   for (int j = 0; j < m.polygons.size(); j++) {
    //     Polygon& p = m.polygons[j];
    //     for (int k = 0; k < 3; k++) {
    //       vec3 pos = p.vertices[k];
    //       vec3 vertex = vec3(0);
    //       unsigned int index = p.indices[k];
    //       for (int n = 0; n < 3; n++) {
    //         int bone_id = bone_ids[index][n];
    //         if (bone_id == -1) continue;
    //         float weight = bone_weights[index][n];

    //         mat4 joint_transform = animation.keyframes[frame].transforms[bone_id];
    //         vertex += vec3(joint_transform * vec4(pos, 1.0)) * weight;
    //       }
    //       vertices.push_back(vertex);
    //     }
    //   }
    //   m.animations[animation.name].keyframes[frame].bounding_sphere =
    //     GetBoundingSphereFromVertices(vertices);
    // }
    // cout << "Extracted animation 2: " << animation.name << endl;
  }

  for (int i = 0; i < data.joints.size(); i++) {
    auto& joint = data.joints[i];
    if (!joint) continue;
    m.bones_to_ids[data.joints[i]->name] = i;
  }
}
