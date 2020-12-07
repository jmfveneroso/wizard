#include <fbxsdk.h>
#include <iostream>
#include "fbx_loader.hpp"

using namespace std;
using namespace glm;

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
}

void ExtractPolygon(FbxMesh* mesh, int i, FbxData* data, int& vertex_id) {
  vector<unsigned int> vertices;
  vector<vec2> uvs;
  vector<vec3> normals;

  int polygon_size = mesh->GetPolygonSize(i);
  for (int j = 0; j < polygon_size; j++, vertex_id++) {
    int vertex_index = mesh->GetPolygonVertex(i, j);
    if (vertex_index < 0) {
      throw runtime_error("Invalid vertex index.");
    } else {
      vertices.push_back(vertex_index);
    }

    for (int k = 0; k < mesh->GetElementUVCount(); ++k) {
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
              uvs.push_back(Get2DVector(
                uv->GetDirectArray().GetAt(lTextureUVIndex)));
            }
            break;
            default:
              break;
          }
        }
        break;
      }
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
  }

  // Uncomment to produce non triangular polygons.
  // Polygon polygon;
  // for (int k = 0; k < polygon_size; k++) {
  //   polygon.vertices.push_back(data->vertices[vertices[k]]);
  //   polygon.normals.push_back(normals[k]);
  // }
  // data->polygons.push_back(polygon);

  // Triangulate the polygon with the triangle fan triangulation method.
  for (int j = 1; j < polygon_size - 1; j++) {
    data->indices.push_back(vertices[0]);
    data->uvs.push_back(uvs[0]);
    data->normals.push_back(normals[0]);
    data->indices.push_back(vertices[j]);
    data->uvs.push_back(uvs[j]);
    data->normals.push_back(normals[j]);
    data->indices.push_back(vertices[j+1]);
    data->uvs.push_back(uvs[j+1]);
    data->normals.push_back(normals[j+1]);

    Polygon polygon;
    polygon.vertices.push_back(data->vertices[vertices[0]]);
    polygon.normals.push_back(normals[0]);
    polygon.indices.push_back(vertices[0]);
    polygon.vertices.push_back(data->vertices[vertices[j]]);
    polygon.normals.push_back(normals[j+1]);
    polygon.indices.push_back(vertices[j]);
    polygon.vertices.push_back(data->vertices[vertices[j+1]]);
    polygon.normals.push_back(normals[j+1]);
    polygon.indices.push_back(vertices[j+1]);
    data->polygons.push_back(polygon);
  }
}

// Extract polygon data.
void ExtractMesh(FbxScene* scene, FbxData* data) {
  FbxNode* node = 
    GetNodeWithAttribute(scene->GetRootNode(), FbxNodeAttribute::eMesh);
  FbxMesh* mesh = (FbxMesh*) node->GetNodeAttribute();

  FbxVector4* vertices = mesh->GetControlPoints();
  int num_vertices = mesh->GetControlPointsCount();
  for (int i = 0; i < num_vertices; i++) {
    data->vertices.push_back(Get3DVector(vertices[i]));
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

  unordered_map<string, shared_ptr<SkeletonJoint>> joint_map;
  stack<shared_ptr<SkeletonJoint>> joint_stack({ data->skeleton });
  while (!joint_stack.empty()) {
    shared_ptr<SkeletonJoint> joint = joint_stack.top();
    joint_stack.pop();
    for (auto& c : joint->children) joint_stack.push(c);
    joint_map[joint->name] = joint;

    // Old way.
    // joint->global_bindpose = transform_link_matrix * inverse(transform_matrix) * 
    //   geometry_transform;

    FbxTime time; 
    time.SetFrame(0, FbxTime::eFrames24); 
    FbxAMatrix transform_offset = node->EvaluateGlobalTransform(time) * 
      GetGeometryTransformation(scene->GetRootNode());
    FbxAMatrix mGlobalTransform = transform_offset.Inverse() *
      joint->node->EvaluateGlobalTransform(time); 
    joint->global_bindpose = GetMatrix(mGlobalTransform);
  }

  vector<vector<tuple<int, float>>> bone_weights(data->vertices.size());
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
      throw runtime_error("Joint not found.");
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
    data->bone_ids.push_back(ids);
    data->bone_weights.push_back(weights);
  }
}

void ExtractAnimations(FbxScene* scene, FbxData* data) {
  FbxNode* node = 
    GetNodeWithAttribute(scene->GetRootNode(), FbxNodeAttribute::eMesh);

  FbxAMatrix geometry_transform = 
    GetGeometryTransformation(scene->GetRootNode());

  for (int i = 0; i < scene->GetSrcObjectCount<FbxAnimStack>(); i++) {
    FbxAnimStack* anim_stack = scene->GetSrcObject<FbxAnimStack>(i);
    if (!anim_stack) continue;

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
    data->animations.push_back(animation);
  }
}

FbxData FbxLoad(const std::string& filename) {
  FbxScene* scene = ImportScene(filename);

  FbxData data;

  // TODO: for each mesh, extract the data.
  ExtractMesh(scene, &data);

  // Animation.
  ExtractSkeleton(scene, &data);
  ExtractSkin(scene, &data);
  ExtractAnimations(scene, &data);

  // TODO: ExtractTextures.
  return data;
}

FbxData LoadFbxData(const std::string& filename, Mesh& m) {
  FbxData data = FbxLoad(filename);
  m.polygons = data.polygons;

  GLuint buffers[6];
  glGenBuffers(6, buffers);

  vector<vec3> vertices;
  vector<unsigned int> indices;
  for (int i = 0; i < data.indices.size(); i++) {
    vertices.push_back(data.vertices[data.indices[i]]);
    indices.push_back(i);
  }
  m.num_indices = indices.size();

  glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), 
    &vertices[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
  glBufferData(GL_ARRAY_BUFFER, data.uvs.size() * sizeof(glm::vec2), 
    &data.uvs[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, buffers[2]);
  glBufferData(GL_ARRAY_BUFFER, data.normals.size() * sizeof(glm::vec3), 
    &data.normals[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[3]); 
  glBufferData(
    GL_ELEMENT_ARRAY_BUFFER, 
    indices.size() * sizeof(unsigned int), 
    &indices[0], 
    GL_STATIC_DRAW
  );

  glGenVertexArrays(1, &m.vao_);
  glBindVertexArray(m.vao_);

  int num_slots = 4;
  BindBuffer(buffers[0], 0, 3);
  BindBuffer(buffers[1], 1, 2);
  BindBuffer(buffers[2], 2, 3);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[3]);

  if (!data.bone_ids.empty()) {
    vector<ivec3> bone_ids;
    vector<vec3> bone_weights;
    for (int i = 0; i < data.indices.size(); i++) {
      bone_ids.push_back(data.bone_ids[data.indices[i]]);
      bone_weights.push_back(data.bone_weights[data.indices[i]]);
    }

    glBindBuffer(GL_ARRAY_BUFFER, buffers[4]);
    glBufferData(GL_ARRAY_BUFFER, bone_ids.size() * sizeof(glm::ivec3), 
      &bone_ids[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[5]);
    glBufferData(GL_ARRAY_BUFFER, bone_weights.size() * sizeof(glm::vec3), 
      &bone_weights[0], GL_STATIC_DRAW);

    num_slots += 2;
    BindBuffer(buffers[4], 3, 3);
    BindBuffer(buffers[5], 4, 3);
  }

  glBindVertexArray(0);
  for (int slot = 0; slot < num_slots; slot++) {
    glDisableVertexAttribArray(slot);
  }

  // Animations.
  if (data.animations.empty()) {
    return data;
  }

  for (const Animation& animation : data.animations) {
    cout << "filename: " << filename << endl;
    cout << "animation name: " << animation.name << endl;
    m.animations[animation.name] = animation;
  }

  for (int i = 0; i < data.joints.size(); i++) {
    auto& joint = data.joints[i];
    if (!joint) continue;
    m.bones_to_ids[data.joints[i]->name] = i;
  }
  return data;
}
