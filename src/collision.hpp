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
#include <vector>
#include "util.hpp"

using namespace std;
using namespace glm;

struct AABB {
  vec3 point;
  vec3 dimensions;
};

ostream& operator<<(ostream& os, const AABB& v);

void ExtractFrustumPlanes(const mat4 MVP, vec4 planes[6]);

bool CollideAABBFrustum(const AABB& aabb, const vec4 planes[6], 
  const vec3& player_pos);
