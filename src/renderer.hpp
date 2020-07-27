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
#include <png.h>
#include "fbx_loader.hpp"
#include "util.hpp"
#include "terrain.hpp"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800
#define APP_NAME "test"
#define NEAR_CLIPPING 0.05f
#define FAR_CLIPPING 2000.0f
#define FIELD_OF_VIEW 45.0f

using namespace std;
using namespace glm;

struct Camera {
  vec3 position; 
  vec3 up; 
  vec3 direction;

  Camera() {}
  Camera(vec3 position, vec3 direction, vec3 up) : position(position), 
    up(up), direction(direction) {}
};

struct Mesh {
  GLuint shader;
  GLuint vertex_buffer_;
  GLuint uv_buffer_;
  GLuint normal_buffer_;
  GLuint element_buffer_;
  GLuint vao_;
  GLuint num_indices;
  Mesh() {}
};

struct Object3D {
  Mesh mesh;
  vec3 position;
  vec3 rotation;
  Object3D() {}
  Object3D(Mesh mesh, vec3 position) : mesh(mesh), position(position) {}
  Object3D(Mesh mesh, vec3 position, vec3 rotation) : mesh(mesh), position(position), rotation(rotation) {}
};

struct FBO {
  GLuint framebuffer;
  GLuint texture;
  GLuint width;
  GLuint height;
  GLuint depth_rbo;
  GLuint vao;

  FBO() {}
  FBO(GLuint width, GLuint height) : width(width), height(height) {}
};

class Renderer {
  GLFWwindow* window_;
  int window_width_ = WINDOW_WIDTH;
  int window_height_ = WINDOW_HEIGHT;
  mat4 projection_matrix_;
  mat4 view_matrix_;
  Camera camera_;

  vector<vector<mat4>> joint_transforms_;
  GLuint texture_;
  shared_ptr<Terrain> terrain_;

  unordered_map<string, GLuint> shaders_;
  unordered_map<string, FBO> fbos_;
  unordered_map<string, Mesh> meshes_;
  std::vector<shared_ptr<Object3D>> objects_;

  void CreateSkeletonAux(mat4 parent_transform, shared_ptr<SkeletonJoint> node);
  void LoadShaders(const std::string& directory);
  FBO CreateFramebuffer(int width, int height);
  Mesh CreateMesh(
    GLuint shader_id,
    vector<vec3>& vertices, 
    vector<vec2>& uvs, 
    vector<unsigned int>& indices
  );
  
  void DrawCube(GLuint vao, glm::vec3 position, GLfloat rotation);
  void DrawFBO(const FBO& fbo);
  void DrawObjects();

 public:
  void Init(const string& shader_dir);  
  void Run(const function<void()>& process_frame);
  void SetCamera(const Camera& camera) { camera_ = camera; }
  shared_ptr<Object3D> CreateCube(vec3 dimensions);
  shared_ptr<Object3D> CreateJoint(vec3 start, vec3 end);
  void LoadFbx(const std::string& filename);
  
  GLFWwindow* window() { return window_; }
};
