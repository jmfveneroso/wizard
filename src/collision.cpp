#include "collision.hpp"

// (1) Find the 6 frustum plane equations.

// (2) For an AABB box

//     (2.1) For a plane 

//           (2.1.1) Find the P and N vertices (diagonal)

//                   (2.1.2) Project the plane normal on to the box axes

//                   (2.2.2) 

ostream& operator<<(ostream& os, const AABB& aabb) {
  os << "Point: " << aabb.point << endl;
  os << "Dimensions: " << aabb.dimensions << endl;
  return os;
}

void NormalizePlane(vec4& plane) {
 float mag = sqrt(plane[0] * plane[0] + plane[1] * plane[1] + 
   plane[2] * plane[2]);
 plane[0] = plane[0] / mag;
 plane[1] = plane[1] / mag;
 plane[2] = plane[2] / mag;
 plane[3] = plane[3] / mag;
}

// Taken from here:
// https://stackoverflow.com/questions/12836967/extracting-view-frustum-planes-hartmann-gribbs-method
// Hartmann & Gribbs method:
// http://www.cs.otago.ac.nz/postgrads/alexis/planeExtraction.pdf
void ExtractFrustumPlanes(const mat4 MVP, vec4 planes[6]) {
  for (int i = 4; i--;) planes[0][i] = MVP[i][3] + MVP[i][0];
  for (int i = 4; i--;) planes[1][i] = MVP[i][3] - MVP[i][0]; 
  for (int i = 4; i--;) planes[2][i] = MVP[i][3] + MVP[i][1];
  for (int i = 4; i--;) planes[3][i] = MVP[i][3] - MVP[i][1];
  for (int i = 4; i--;) planes[4][i] = MVP[i][3] + MVP[i][2];
  for (int i = 4; i--;) planes[5][i] = MVP[i][3] - MVP[i][2];
  for (int i = 0; i < 6; i++) {
    NormalizePlane(planes[i]);
  }
}

bool CollideAABBFrustum(const AABB& aabb, const vec4 planes[6], 
  const vec3& player_pos) {
  vec3 points[8] = {
    vec3(aabb.point),                                                 // x_min, y_min, z_min
    vec3(aabb.point + vec3(aabb.dimensions.x, 0, 0)),                 // x_max, y_min, z_min
    vec3(aabb.point + vec3(0, aabb.dimensions.y, 0)),                 // x_min, y_max, z_min
    vec3(aabb.point + vec3(aabb.dimensions.x, aabb.dimensions.y, 0)), // x_max, y_max, z_min
    vec3(aabb.point + vec3(0, 0, aabb.dimensions.z)),                 // x_min, y_min, z_max
    vec3(aabb.point + vec3(aabb.dimensions.x, 0, aabb.dimensions.z)), // x_max, y_min, z_max
    vec3(aabb.point + vec3(0, aabb.dimensions.y, aabb.dimensions.z)), // x_min, y_max, z_max
    vec3(aabb.point + aabb.dimensions)                                // x_max, y_max, z_max
  };

  static int pn_vertex[16][2] = {
    // Diagonals.
    { 7, 0 },
    { 3, 4 },
    { 5, 2 },
    { 1, 6 },
    // Top.
    { 4, 5 },
    { 5, 7 },
    { 7, 6 },
    { 6, 4 },
    // Heights.
    { 0, 4 },
    { 1, 5 },
    { 2, 6 },
    { 3, 7 },
    // Bottom.
    { 0, 1 },
    { 1, 3 },
    { 3, 2 },
    { 2, 0 },
  };

  for (int i = 0; i < 16; i++) {
    vec3 p_vertex = points[pn_vertex[i][0]] - player_pos;
    vec3 n_vertex = points[pn_vertex[i][1]] - player_pos;
  
    bool inside = true;
    for (int i = 0; i < 6; i++ ) {
      // TODO: apparently its working.
      // if (i == 2) continue; // Bot culling is not working properly.
      // if (i == 3) continue; // Top culling is not working properly.
      const vec4& plane = planes[i];
      float p_result = dot(p_vertex, vec3(plane.x, plane.y, plane.z)) + plane.w;
      float n_result = dot(n_vertex, vec3(plane.x, plane.y, plane.z)) + plane.w;
      if (p_result < 0 && n_result < 0) { // Completely outside.
        inside = false;
        break;
      }
    }
    if (inside) return true;
  }
  return false;
}
