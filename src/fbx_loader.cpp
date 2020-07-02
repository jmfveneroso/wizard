#include <fbxsdk.h>
#include <iostream>
#include "fbx_loader.hpp"

using namespace std;
using namespace glm;

ostream& operator<<(ostream& os, const vec2& v) {
  os << v.x << " " << v.y;
  return os;
}

ostream& operator<<(ostream& os, const vec3& v) {
  os << v.x << " " << v.y << " " << v.z;
  return os;
}

ostream& operator<<(ostream& os, const vec4& v) {
  os << v.x << " " << v.y << " " << v.z << " " << v.w;
  return os;
}

ostream& operator<<(ostream& os, const mat4& m) {
  os << m[0][0] << " " << m[0][1] << " " << m[0][2] << " " << m[0][3] << endl;
  os << m[1][0] << " " << m[1][1] << " " << m[1][2] << " " << m[1][3] << endl;
  os << m[2][0] << " " << m[2][1] << " " << m[2][2] << " " << m[2][3] << endl;
  os << m[3][0] << " " << m[3][1] << " " << m[3][2] << " " << m[3][3] << endl;
  return os;
}

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

void ExtractPolygons(FbxMesh* pMesh, FbxData* data) {
  int lControlPointsCount = pMesh->GetControlPointsCount();
  FbxVector4* lControlPoints = pMesh->GetControlPoints();
  for (int i = 0; i < lControlPointsCount; i++) {
    data->vertices.push_back(Get3DVector(lControlPoints[i]));
  }

  int vertexId = 0;
  for (int i = 0; i < pMesh->GetPolygonCount(); i++) {
    for (int j = 0; j < pMesh->GetPolygonSize(i); j++, vertexId++) {
      int lControlPointIndex = pMesh->GetPolygonVertex(i, j);
      if (lControlPointIndex < 0) {
        throw;
      } else {
        data->indices.push_back(lControlPointIndex);
      }

      for (int l = 0; l < pMesh->GetElementUVCount(); ++l) {
        FbxGeometryElementUV* leUV = pMesh->GetElementUV(l);
        switch (leUV->GetMappingMode()) {
          default:
            break;
          case FbxGeometryElement::eByControlPoint:
            switch (leUV->GetReferenceMode()) {
              case FbxGeometryElement::eDirect:
                data->uvs.push_back(Get2DVector(leUV->GetDirectArray().GetAt(lControlPointIndex)));
                break;
              case FbxGeometryElement::eIndexToDirect: {
                int id = leUV->GetIndexArray().GetAt(lControlPointIndex);
                data->uvs.push_back(Get2DVector(leUV->GetDirectArray().GetAt(id)));
              }
              break;
              default:
                break; // other reference modes not shown here!
            }
            break;
          case FbxGeometryElement::eByPolygonVertex: {
            int lTextureUVIndex = pMesh->GetTextureUVIndex(i, j);
            switch (leUV->GetReferenceMode()) {
              case FbxGeometryElement::eDirect:
              case FbxGeometryElement::eIndexToDirect: {
                data->uvs.push_back(Get2DVector(leUV->GetDirectArray().GetAt(lTextureUVIndex)));
              }
              break;
              default:
                break; // other reference modes not shown here!
            }
          }
          break;
        }
      }

      for (int l = 0; l < pMesh->GetElementNormalCount(); ++l) {
        FbxGeometryElementNormal* leNormal = pMesh->GetElementNormal( l);
        if(leNormal->GetMappingMode() == FbxGeometryElement::eByPolygonVertex) {
          switch (leNormal->GetReferenceMode()) {
            case FbxGeometryElement::eDirect:
              data->normals.push_back(Get3DVector(leNormal->GetDirectArray().GetAt(vertexId)));
              break;
            case FbxGeometryElement::eIndexToDirect: {
              int id = leNormal->GetIndexArray().GetAt(vertexId);
              data->normals.push_back(Get3DVector(leNormal->GetDirectArray().GetAt(id)));
            }
            break;
            default:
              break; // other reference modes not shown here!
          }
        }
      }
    }
  }
}

FbxAMatrix GetGeometryTransformation(FbxNode* inNode) { 
  if (!inNode) { 
    throw;
  } 
  
  const FbxVector4 lT = inNode->GetGeometricTranslation(FbxNode::eSourcePivot); 
  const FbxVector4 lR = inNode->GetGeometricRotation(FbxNode::eSourcePivot); 
  const FbxVector4 lS = inNode->GetGeometricScaling(FbxNode::eSourcePivot); 
  return FbxAMatrix(lT, lR, lS); 
} 

// Describes how bones deform the geometry.
void ExtractSkin(FbxGeometry* pGeometry, FbxData* data) {
  data->joint_vector.resize(data->joint_map.size());
  int lSkinCount = pGeometry->GetDeformerCount(FbxDeformer::eSkin);
  for (int i = 0; i != lSkinCount; ++i) {
    int lClusterCount = ((FbxSkin*) pGeometry->GetDeformer(i, FbxDeformer::eSkin))->GetClusterCount();
    for (int j = 0; j != lClusterCount; ++j) {
      FbxCluster* lCluster = ((FbxSkin *) pGeometry->GetDeformer(i, FbxDeformer::eSkin))->GetCluster(j);
      if (lCluster->GetLink() == NULL) continue;
      string name = (char *) lCluster->GetLink()->GetName();
      cout << name << endl;

      cout << data->joint_map.size() << endl;
      if (data->joint_map.find(name) == data->joint_map.end()) {
        throw;
      }

      shared_ptr<SkeletonJoint> joint = data->joint_map[name];
      data->joint_vector[j] = joint;

      int lIndexCount = lCluster->GetControlPointIndicesCount();
      int* lIndices = lCluster->GetControlPointIndices();
      double* lWeights = lCluster->GetControlPointWeights();
      for (int k = 0; k < lIndexCount; k++) {
        joint->indices.push_back(lIndices[k]);
        joint->weights.push_back((double) lWeights[k]);
      }

      FbxAMatrix transformMatrix;
      lCluster->GetTransformMatrix(transformMatrix); 

      mat4 m2 = GetMatrix(transformMatrix);
      cout << "transformMatrix start" << endl;
      cout << m2;
      cout << "transformMatrix end" << endl;

      vec4 v = vec4(0, 0, 0, 1);
      v = m2 * v;
      cout << v << endl;
      cout << "transformMatrix end 2" << endl;
      

      FbxAMatrix transformLinkMatrix;
      lCluster->GetTransformLinkMatrix(transformLinkMatrix);

      // Reference: https://www.gamedev.net/articles/programming/graphics/how-to-work-with-fbx-sdk-r3582/
      FbxAMatrix globalBindposeInverseMatrix = transformLinkMatrix.Inverse() * transformMatrix; // geometryTransform should be the identity matrix.
      mat4 m = GetMatrix(globalBindposeInverseMatrix);
      cout << "globalBindposeInverseMatrix start" << endl;
      cout << m;
      cout << "globalBindposeInverseMatrix end" << endl;

      mat4 transform_link_matrix = GetMatrix(transformLinkMatrix);
      mat4 transform_matrix = GetMatrix(transformMatrix);

      // joint->global_bindpose = GetMatrix(transformLinkMatrix * transformMatrix);
      // joint->global_bindpose_inverse = GetMatrix(globalBindposeInverseMatrix);

      joint->global_bindpose = scale(vec3(0.01f)) * transform_link_matrix * transform_matrix;
      joint->global_bindpose_inverse = inverse(transform_link_matrix) * transform_matrix;

      joint->translation = Get3DVector(transformLinkMatrix.GetT());
      joint->rotation = Get3DVector(transformLinkMatrix.GetR());
    }
  }
}

FbxNode* GetNodeWithAttribute(FbxNode* lNode, FbxNodeAttribute::EType attr) {
  if (!lNode) return nullptr;

  if (lNode->GetNodeAttribute() != NULL && 
      lNode->GetNodeAttribute()->GetAttributeType() == attr) {
    return lNode;
  }

  for (int i = 0; i < lNode->GetChildCount(); i++) {
    FbxNode* res = GetNodeWithAttribute(lNode->GetChild(i), attr);
    if (res) return res;
  }
  return nullptr;
}

shared_ptr<SkeletonJoint> GetSkeleton(FbxNode* lNode) {
  if (!lNode) return nullptr;

  shared_ptr<SkeletonJoint> joint = make_shared<SkeletonJoint>();
  if (lNode->GetNodeAttribute() != NULL && 
      lNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton) {
    FbxSkeleton* lSkeleton = (FbxSkeleton*) lNode->GetNodeAttribute();
    joint->name = (char *) lSkeleton->GetName();
    if (lSkeleton->GetSkeletonType() == FbxSkeleton::eLimb) {
      joint->length = (double) lSkeleton->LimbLength.Get();
    } else if (lSkeleton->GetSkeletonType() == FbxSkeleton::eLimbNode) {
      joint->length = (double) lSkeleton->Size.Get();
    } else if (lSkeleton->GetSkeletonType() == FbxSkeleton::eRoot) {
      joint->length = (double) lSkeleton->Size.Get();
    }
  }

  for (int i = 0; i < lNode->GetChildCount(); i++) {
    shared_ptr<SkeletonJoint> child = GetSkeleton(lNode->GetChild(i));
    if (!child) continue;
    joint->children.push_back(child);
  }
  return joint;
}

vector<tuple<int, float>> GetValuesPerFrameForKey(FbxAnimCurve* pCurve) {
  vector<tuple<int, float>> ms_to_value;
  if (pCurve) {
    int lKeyCount = pCurve->KeyGetCount();
    for (int lCount = 0; lCount < lKeyCount; lCount++) {
      float lKeyValue = static_cast<float>(pCurve->KeyGetValue(lCount));
      FbxTime lKeyTime = pCurve->KeyGetTime(lCount);
      ms_to_value.push_back(make_tuple(lKeyTime.GetMilliSeconds(), lKeyValue));
    }
  }
  return ms_to_value;
}

// Reference: https://www.gamedev.net/articles/programming/graphics/how-to-work-with-fbx-sdk-r3582/.
void ExtractAnimationsAux(FbxAnimLayer* pAnimLayer, FbxNode* pNode, FbxData* data) {
  string name = pNode->GetName();
  if (data->joint_map.find(name) != data->joint_map.end()) {
    shared_ptr<SkeletonJoint> joint = data->joint_map[name];
    joint->tx = GetValuesPerFrameForKey(pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X));
    joint->ty = GetValuesPerFrameForKey(pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y));
    joint->tz = GetValuesPerFrameForKey(pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z));
    joint->rx = GetValuesPerFrameForKey(pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X));
    joint->ry = GetValuesPerFrameForKey(pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y));
    joint->rz = GetValuesPerFrameForKey(pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z));
  }

  for (int lModelCount = 0; lModelCount < pNode->GetChildCount(); lModelCount++) {
    ExtractAnimationsAux(pAnimLayer, pNode->GetChild(lModelCount), data);
  }
}

// Reference: https://www.gamedev.net/articles/programming/graphics/how-to-work-with-fbx-sdk-r3582/
void ExtractAnimations2(FbxScene* pScene, FbxNode* inNode, FbxData* data) {
  FbxMesh* currMesh = inNode->GetMesh(); 
  int num_deformers = currMesh->GetDeformerCount(); 
  cout << "num_deformers: " << num_deformers << endl;
  for (int i = 0; i < num_deformers; ++i) {
    cout << "bla" << endl;
    FbxSkin* currSkin = (FbxSkin*) currMesh->GetDeformer(i, FbxDeformer::eSkin); 
    if (!currSkin) { 
      continue; 
    } 
    
    int num_clusters = currSkin->GetClusterCount(); 
    cout << "num_clusters: " << num_clusters << endl;
    for (int j = 0; j < num_clusters; ++j) {
      FbxCluster* currCluster = currSkin->GetCluster(j);
      FbxAnimStack* pAnimStack = pScene->GetSrcObject<FbxAnimStack>(0);
      if (!currCluster || !pAnimStack) {
        continue;
      }

      string joint_name = (char *) currCluster->GetLink()->GetName();
      const auto& joint = data->joint_map[joint_name];

      FbxString animStackName = pAnimStack->GetName(); 
      FbxString mAnimationName = animStackName.Buffer(); 

      FbxTakeInfo* takeInfo = pScene->GetTakeInfo(animStackName); 
      FbxTime start = takeInfo->mLocalTimeSpan.GetStart(); 
      FbxTime end = takeInfo->mLocalTimeSpan.GetStop(); 

      int mAnimationLength = end.GetFrameCount(FbxTime::eFrames24) - start.GetFrameCount(FbxTime::eFrames24) + 1; 
      cout << "Animation Length: " << mAnimationLength  << endl;

      // For each frame.     
      FbxLongLong frame_count = end.GetFrameCount(FbxTime::eFrames24);
      for (FbxLongLong i = start.GetFrameCount(FbxTime::eFrames24); i <= frame_count; ++i) { 
        FbxTime currTime; 
        currTime.SetFrame(i, FbxTime::eFrames24); 
        cout << "Frame number: " << i << endl;
        FbxAMatrix currentTransformOffset = inNode->EvaluateGlobalTransform(currTime); // * geometryTransform.
        FbxAMatrix mGlobalTransform = currentTransformOffset.Inverse() * currCluster->GetLink()->EvaluateGlobalTransform(currTime); 
        mat4 m = GetMatrix(mGlobalTransform);
        joint->keyframes.push_back(Keyframe(i, m));
        // cout << m;
      }
      cout << "sat what";
    }
  }
}

void ExtractAnimations(FbxScene* pScene, FbxData* data) {
  for (int i = 0; i < pScene->GetSrcObjectCount<FbxAnimStack>(); i++) {
    FbxAnimStack* pAnimStack = pScene->GetSrcObject<FbxAnimStack>(i);
    FbxNode* pNode = pScene->GetRootNode();

    int nbAnimLayers = pAnimStack->GetMemberCount<FbxAnimLayer>();
    if (nbAnimLayers == 0) {
      continue;
    }

    FbxString animStackName = pAnimStack->GetName(); 
    FbxTakeInfo* takeInfo = pScene->GetTakeInfo(animStackName); 
    FbxTime start = takeInfo->mLocalTimeSpan.GetStart(); 
    FbxTime end = takeInfo->mLocalTimeSpan.GetStop(); 

    for (int l = 0; l < nbAnimLayers; l++) {
      FbxAnimLayer* pAnimLayer = pAnimStack->GetMember<FbxAnimLayer>(l);
      ExtractAnimationsAux(pAnimLayer, pNode, data);
    }
  }
}

void BuildSkeletonMap(shared_ptr<SkeletonJoint> joint, FbxData* data, int indents = 0) {
  if (!joint) return;
  // cout << string(indents, ' ') << joint->name << endl;
  data->joint_map[joint->name] = joint;
  for (auto& c : joint->children) BuildSkeletonMap(c, data, indents + 1);
}

void SampleKeyframes(FbxData* data) {
  int sample_rate = 20; // Per second.
  for (auto& it : data->joint_map) {
    cout << it.first << endl;
    shared_ptr<SkeletonJoint> joint = it.second;

    vector<vector<tuple<int, float>>> arr { joint->tx, joint->ty, joint->tz, 
      joint->rx, joint->ry, joint->rz };
    
    int max_time = 0;
    for (auto& vec : arr) {
      if (vec.empty()) continue;
      max_time = std::max(std::get<0>(vec[vec.size() - 1]), max_time);
    }

    if (!max_time) continue;
    cout << "Max time: " << max_time << endl;

    for (int i = 0; i < 6; i++) {
      if (!arr[i].size()) throw;
      if (std::get<0>(arr[i][0]) != 0) throw;
    }

    vector<int> cur { 0, 0, 0, 0, 0, 0 };
    for (int ms = 0; ms <= max_time; ms++) {
      for (int i = 0; i < 6; i++) {
        if (cur[i] == arr[i].size()) continue;

        if (std::get<0>(arr[i][cur[i]+1]) <= ms) {
          cur[i]++;
        }
      }
 
      if (ms % sample_rate == 0) {
        float tx = std::get<1>(arr[0][cur[0]]);
        float ty = std::get<1>(arr[1][cur[1]]);
        float tz = std::get<1>(arr[2][cur[2]]);
        vec3 translation(tx, ty, tz);
        float rx = std::get<1>(arr[3][cur[3]]);
        float ry = std::get<1>(arr[4][cur[4]]);
        float rz = std::get<1>(arr[5][cur[5]]);
        vec3 rotation(rx, ry, rz);
        joint->keyframes.push_back(Keyframe(ms, translation, rotation));
      }
    }
  }
}

void BuildBoneLinks(FbxData* data) {
  data->bone_ids.resize(data->vertices.size());
  data->bone_weights.resize(data->vertices.size());

  for (int i = 0; i < data->joint_vector.size(); i++) {
    shared_ptr<SkeletonJoint> joint = data->joint_vector[i];
    if (!joint) continue;

    for (int j = 0; j < joint->indices.size(); j++) {
      int vertex_id = joint->indices[j];
      data->bone_ids[vertex_id].push_back(i);
      data->bone_weights[vertex_id].push_back(joint->weights[j]);
    }
  }

  for (int i = 0; i < data->bone_ids.size(); i++) {
    cout << "Vertex " << i << " -> ";
    for (int j = 0; j < data->bone_ids[i].size(); j++) {
      cout << data->bone_ids[i][j] << " " << data->bone_weights[i][j] << " ";
    }
    cout << endl;
  }
}

FbxData FbxLoad(const std::string& filename) {
  // Initialize the SDK manager. This object handles memory management.
  FbxManager* lSdkManager = FbxManager::Create();

  // Create the IO settings object.
  FbxIOSettings *ios = FbxIOSettings::Create(lSdkManager, IOSROOT);
  lSdkManager->SetIOSettings(ios);

  // Create an importer using the SDK manager.
  FbxImporter* lImporter = FbxImporter::Create(lSdkManager,"");
  
  // Use the first argument as the filename for the importer.
  if(!lImporter->Initialize(filename.c_str(), -1, lSdkManager->GetIOSettings())) { 
      printf("Call to FbxImporter::Initialize() failed.\n"); 
      printf("Error returned: %s\n\n", lImporter->GetStatus().GetErrorString()); 
      exit(-1);
  }

  // Create a new scene so that it can be populated by the imported file.
  FbxScene* lScene = FbxScene::Create(lSdkManager,"myScene");

  // Import the contents of the file into the scene.
  lImporter->Import(lScene);

  // The file is imported, so get rid of the importer.
  lImporter->Destroy();

  FbxData data;

  // Skeleton.
  FbxNode* skeleton_node = GetNodeWithAttribute(lScene->GetRootNode(), 
    FbxNodeAttribute::eSkeleton);
  if (skeleton_node) {
    data.skeleton = GetSkeleton(skeleton_node);
    BuildSkeletonMap(data.skeleton, &data);
  }

  // FbxAMatrix geometryTransform = GetGeometryTransformation(lScene->GetRootNode());
  // cout << "geometryTransform start" << endl;
  // FbxString lMatrixValue;
  // for (int k = 0; k < 4; k++) {
  //   FbxVector4 lRow = geometryTransform.GetRow(k);
  //   char lRowValue[1024];
  //   FBXSDK_sprintf(lRowValue, 1024, "%9.4f %9.4f %9.4f %9.4f\n", lRow[0], lRow[1], lRow[2], lRow[3]);
  //   lMatrixValue += FbxString("        ") + FbxString(lRowValue);
  // }
  // DisplayString("", lMatrixValue.Buffer());
  // cout << "geometryTransform end" << endl;

  FbxNode* mesh_node = 
    GetNodeWithAttribute(lScene->GetRootNode(), FbxNodeAttribute::eMesh);
  if (mesh_node) {
    FbxMesh* lMesh = (FbxMesh*) mesh_node->GetNodeAttribute();
    ExtractPolygons(lMesh, &data);
    ExtractSkin(lMesh, &data);
  }

  // ExtractAnimations(lScene, &data);
  ExtractAnimations2(lScene, mesh_node, &data);
  // SampleKeyframes(&data);
  BuildBoneLinks(&data);

  // DisplayPose(lScene);
  // DisplayAnimation(lScene);
  // DisplayHierarchy(lScene);
  // cout << "Display Content" << endl;
  // DisplayContent(lScene);

  return data;
}
