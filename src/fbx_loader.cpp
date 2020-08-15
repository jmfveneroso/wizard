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

shared_ptr<SkeletonJoint> BuildSkeleton(FbxNode* lNode) {
  if (!lNode) return nullptr;

  shared_ptr<SkeletonJoint> joint = make_shared<SkeletonJoint>();
  if (lNode->GetNodeAttribute() != NULL && 
      lNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton) {
    FbxSkeleton* lSkeleton = (FbxSkeleton*) lNode->GetNodeAttribute();
    joint->name = (char*) lSkeleton->GetName();
  }

  for (int i = 0; i < lNode->GetChildCount(); i++) {
    shared_ptr<SkeletonJoint> child = BuildSkeleton(lNode->GetChild(i));
    if (!child) continue;
    joint->children.push_back(child);
  }
  return joint;
}

void BuildJointMap(shared_ptr<SkeletonJoint> joint, FbxData* data) {
  data->joint_map[joint->name] = joint;
  for (auto& c : joint->children) BuildJointMap(c, data);
}

void ExtractSkeleton(FbxScene* scene, FbxData* data) {
  FbxNode* skeleton_node = GetNodeWithAttribute(scene->GetRootNode(), 
    FbxNodeAttribute::eSkeleton);
  if (!skeleton_node) return;

  data->skeleton = BuildSkeleton(skeleton_node);
  BuildJointMap(data->skeleton, data);
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

  // For collision detection.
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
    polygon.vertices.push_back(data->vertices[vertices[j]]);
    polygon.normals.push_back(normals[j+1]);
    polygon.vertices.push_back(data->vertices[vertices[j+1]]);
    polygon.normals.push_back(normals[j+1]);
    data->polygons.push_back(polygon);
  }
}

// Extract polygon data.
void ExtractMesh(FbxScene* scene, FbxData* data) {
  FbxNode* node = 
    GetNodeWithAttribute(scene->GetRootNode(), FbxNodeAttribute::eMesh);
  FbxMesh* mesh = (FbxMesh*) node->GetNodeAttribute();

  int num_vertices = mesh->GetControlPointsCount();
  FbxVector4* vertices = mesh->GetControlPoints();
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

  vector<vector<tuple<int, float>>> bone_weights(data->vertices.size());
  data->joints.resize(data->joint_map.size());

  mat4 geometry_transform = 
    GetMatrix(GetGeometryTransformation(scene->GetRootNode()));

  FbxSkin* skin = (FbxSkin*) mesh->GetDeformer(0, FbxDeformer::eSkin); 
  if (!skin) return;

  int num_clusters = skin->GetClusterCount(); 
  for (int i = 0; i < num_clusters; ++i) {
    FbxCluster* cluster = skin->GetCluster(i);
    if (!cluster) continue;

    string name = (char*) cluster->GetLink()->GetName();
    if (data->joint_map.find(name) == data->joint_map.end()) {
      throw runtime_error("Joint not found.");
    }

    shared_ptr<SkeletonJoint> joint = data->joint_map[name];
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

    joint->global_bindpose = scale(vec3(0.01f)) * transform_link_matrix * 
      transform_matrix * geometry_transform;
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
        keyframe.transforms[j] = m;
      }
      animation.keyframes.push_back(keyframe);
    }
    data->animations.push_back(animation);
  }
}

FbxData FbxLoad(const std::string& filename) {
  FbxScene* scene = ImportScene(filename);

  FbxData data;
  ExtractSkeleton(scene, &data);
  ExtractMesh(scene, &data);
  ExtractSkin(scene, &data);
  ExtractAnimations(scene, &data);
  // TODO: ExtractTextures.
  return data;
}
