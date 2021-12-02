#ifndef __FBX_LOADER_HPP__
#define __FBX_LOADER_HPP__

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
#include <unordered_set>
#include "util.hpp"

using namespace std;
using namespace glm;

// Useful references:
// - OpenGL Skeletal Animation: https://www.youtube.com/watch?v=f3Cr8Yx3GGA
// - Github for yt above: https://github.com/TheThinMatrix/OpenGL-Animation/tree/master/Engine/openglObjects
// - How to Work with FBX SDK: https://www.gamedev.net/articles/programming/graphics/how-to-work-with-fbx-sdk-r3582/
// - Github for text above: https://github.com/lang1991/FBXExporter/tree/master/FBXExporter
// - Skinned Mesh Animation Using Matrices: https://www.gamedev.net/tutorials/graphics-programming-and-theory/skinned-mesh-animation-using-matrices-r3577/
// - FBX SDK docs: https://help.autodesk.com/view/FBX/2015/ENU/?guid=__files_GUID_9481A726_315C_4A58_A347_8AC95C2AF0F2_htm
// - Spatial transformation matrices: https://www.brainvoyager.com/bv/doc/UsersGuide/CoordsAndTransforms/SpatialTransformationMatrices.html
// - OpenGL Matrices: http://www.opengl-tutorial.org/beginners-tutorials/tutorial-3-matrices/#scaling-matrices

struct SkeletonJoint {
  string name;
  shared_ptr<SkeletonJoint> parent;
  vector<shared_ptr<SkeletonJoint>> children;
  mat4 global_bindpose_inverse; 
  mat4 global_bindpose; // Probably don't need this.
  FbxCluster* cluster; // Remove from here.
  FbxNode* node; // Remove from here.
};

struct FbxData {
  RawMesh raw_mesh;

  shared_ptr<SkeletonJoint> skeleton;
  vector<shared_ptr<SkeletonJoint>> joints;
  unordered_map<string, shared_ptr<SkeletonJoint>> joint_map;
};

void FbxLoad(const std::string& filename, FbxData& data);
void LoadFbxData(const std::string& filename, Mesh& m, FbxData& data, bool calculate_bs = false);

#endif // __FBX_LOADER_H__
