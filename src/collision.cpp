#include "collision.hpp"

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
void ExtractFrustumPlanes(const mat4& MVP, vec4 planes[6]) {
  for (int i = 4; i--;) planes[0][i] = MVP[i][3] + MVP[i][0]; // Left.
  for (int i = 4; i--;) planes[1][i] = MVP[i][3] - MVP[i][0]; // Right.
  for (int i = 4; i--;) planes[2][i] = MVP[i][3] + MVP[i][1]; // Top.
  for (int i = 4; i--;) planes[3][i] = MVP[i][3] - MVP[i][1]; // Bottom.
  for (int i = 4; i--;) planes[4][i] = MVP[i][3] + MVP[i][2]; // Near.
  for (int i = 4; i--;) planes[5][i] = MVP[i][3] - MVP[i][2]; // Far.
  for (int i = 0; i < 6; i++) {
    NormalizePlane(planes[i]);
  }
}

bool IsAABBIntersectingAABB(const AABB& aabb1, const AABB& aabb2) {
  vec3 aabb1_min = aabb1.point;
  vec3 aabb1_max = aabb1.point + aabb1.dimensions;
  vec3 aabb2_min = aabb2.point;
  vec3 aabb2_max = aabb2.point + aabb2.dimensions;
  for (int i = 0; i < 3; i++) {
    if (aabb1_max[i] < aabb2_min[i]) return false;
    if (aabb1_min[i] > aabb2_max[i]) return false;
  }
  return true;
}

vec3 ClosestPtPointAABB(const vec3& p, const AABB& aabb) {
  vec3 aabb_min = aabb.point;
  vec3 aabb_max = aabb.point + aabb.dimensions;
  vec3 q;

  // For each coordinate axis, if the point coordinate value is
  // outside box, clamp it to the box, else keep it as is
  for (int i = 0; i < 3; i++) {
    float v = p[i];
    if (v < aabb_min[i]) v = aabb_min[i]; // v = max(v, b.min[i])
    if (v > aabb_max[i]) v = aabb_max[i]; // v = min(v, b.max[i])
    q[i] = v;
  }
  return q;
}

bool TestSphereAABBIntersection(const BoundingSphere& s, const AABB& aabb) {
  vec3 p = ClosestPtPointAABB(s.center, aabb);
  return length(p - s.center) < s.radius;
}

bool TestSphereTriangleIntersection(const BoundingSphere& s, const vector<vec3>& v) {
  bool inside;
  vec3 p = ClosestPtPointTriangle(s.center, v[0], v[1], v[2], &inside);
  return length(p - s.center) < s.radius;
}

bool IsPointInAABB(const vec3& p, const AABB& aabb) {
  vec3 aabb_max = aabb.point + aabb.dimensions;
  return p.x >= aabb.point.x && p.x <= aabb_max.x &&
      p.y >= aabb.point.y && p.y <= aabb_max.y &&
      p.z >= aabb.point.z && p.z <= aabb_max.z;
}

// More or less based on:
// https://old.cescg.org/CESCG-2002/DSykoraJJelinek/index.html#s6
bool CollideAABBFrustum(const AABB& aabb, const vec4 planes[], 
  const vec3& player_pos) {
  if (IsPointInAABB(player_pos, aabb)) {
    return true;
  }

  static vec3 offsets[8] = {
    vec3(0, 0, 0),
    vec3(1, 0, 0),
    vec3(0, 1, 0),
    vec3(1, 1, 0),
    vec3(0, 0, 1),
    vec3(1, 0, 1),
    vec3(0, 1, 1),
    vec3(1, 1, 1) 
  };

  // Diagonals near vertex for the diagonals.
  static int n_vertex_idx[8] = { 7, 3, 5, 1, 6, 2, 4, 0 };

  for (int i = 0; i < 6; i++ ) {
    const vec4& p = planes[i];
    int idx = ((p.x < 0) ? 4 : 0) + ((p.y < 0) ? 2 : 0) + ((p.z < 0) ? 1 : 0);
    vec3 n_vertex = aabb.point + aabb.dimensions * offsets[n_vertex_idx[idx]]
      - player_pos;
    
    // All planes except for the near and far planes pass through the player
    // position, so w should be 0 for them. In the near and far cases w is
    // the near and far clipping distance.
    if (dot(n_vertex, vec3(p.x, p.y, p.z)) + p.w < 0) { // Completely outside.
      return false;
    }
  }
  return true;
}

bool CollideTriangleFrustum(const vector<vec3>& v, const vec4 planes[],
  const vec3& player_pos) {
  for (int i = 0; i < 6; i++ ) {
    const vec4& p = planes[i];

    bool all_outside = true;
    for (int j = 0; j < 3; j++ ) {
      if (dot(v[j] - player_pos, vec3(p.x, p.y, p.z)) + p.w > 0) {
        all_outside = false;
        break;
      }
    }
    if (all_outside) {
      return false;
    }
  }
  return true;
}

vec3 ClosestPtPointTriangle(vec3 p, vec3 a, vec3 b, vec3 c, bool* inside) {
  vec3 ab = b - a;
  vec3 ac = c - a;
  vec3 bc = c - b;

  *inside = false;

  // Compute parametric position s for projection P’ of P on AB,
  // P’ = A + s*AB, s = snom/(snom+sdenom)
  float snom = dot(p - a, ab), sdenom = dot(p - b, a - b);

  // Compute parametric position t for projection P’ of P on AC,
  // P’ = A + t*AC, s = tnom/(tnom+tdenom)
  float tnom = dot(p - a, ac), tdenom = dot(p - c, a - c);

  if (snom <= 0.0f && tnom <= 0.0f) return a; // Vertex region early out

  // Compute parametric position u for projection P’ of P on BC,
  // P’ = B + u*BC, u = unom/(unom+udenom)
  float unom = dot(p - b, bc), udenom = dot(p - c, b - c);

  if (sdenom <= 0.0f && unom <= 0.0f) return b; // Vertex region early out
  if (tdenom <= 0.0f && udenom <= 0.0f) return c; // Vertex region early out

  // P is outside (or on) AB if the triple scalar product [N PA PB] <= 0
  vec3 n = cross(b - a, c - a);
  float vc = dot(n, cross(a - p, b - p));

  // If P outside AB and within feature region of AB,
  // return projection of P onto AB
  if (vc <= 0.0f && snom >= 0.0f && sdenom >= 0.0f)
    return a + snom / (snom + sdenom) * ab;

  // P is outside (or on) BC if the triple scalar product [N PB PC] <= 0
  float va = dot(n, cross(b - p, c - p));

  // If P outside BC and within feature region of BC,
  // return projection of P onto BC
  if (va <= 0.0f && unom >= 0.0f && udenom >= 0.0f)
    return b + unom / (unom + udenom) * bc;

  // P is outside (or on) CA if the triple scalar product [N PC PA] <= 0
  float vb = dot(n, cross(c - p, a - p));

  // If P outside CA and within feature region of CA,
  // return projection of P onto CA
  if (vb <= 0.0f && tnom >= 0.0f && tdenom >= 0.0f)
    return a + tnom / (tnom + tdenom) * ac;

  // P must project inside face region. Compute Q using barycentric coordinates
  *inside = true;
  float u = va / (va + vb + vc);
  float v = vb / (va + vb + vc);
  float w = 1.0f - u - v; // = vc / (va + vb + vc)
  return u * a + v * b + w * c;
}

vec3 ClosestPtPointSegment(vec3 c, vec3 a, vec3 b) {
  vec3 ab = b - a;

  // Project c onto ab, computing parameterized position d(t)=a+ t*(b – a)
  float t = dot(c - a, ab) / dot(ab, ab);

  // If outside segment, clamp t (and therefore d) to the closest endpoint
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  // Compute projected position from the clamped t
  return a + t * ab;
}

bool IsBehindPlane(const vec3& p, const vec3& plane_point, const vec3& normal) {
  float d = dot(plane_point, normal);
  return dot(p, normal) - d < 0;
}

bool IsInConvexHull(const vec3& p, vector<Polygon> polygons) {
  for (const Polygon& poly : polygons) {
    const vec3& plane_point = poly.vertices[0];
    const vec3& normal = poly.normals[0];
    if (!IsBehindPlane(p, plane_point, normal)) {
      return false;
    }
  }
  return true;
}

bool IsInConvexHull(const AABB& aabb, vector<Polygon> polygons) {
  vector<vec3> points {
    { aabb.point.x                    , aabb.point.y                    , aabb.point.z                     },
    { aabb.point.x + aabb.dimensions.x, aabb.point.y                    , aabb.point.z                     },
    { aabb.point.x                    , aabb.point.y + aabb.dimensions.y, aabb.point.z                     },
    { aabb.point.x + aabb.dimensions.x, aabb.point.y + aabb.dimensions.y, aabb.point.z                     },
    { aabb.point.x                    , aabb.point.y                    , aabb.point.z + aabb.dimensions.z },
    { aabb.point.x + aabb.dimensions.x, aabb.point.y                    , aabb.point.z + aabb.dimensions.z },
    { aabb.point.x                    , aabb.point.y + aabb.dimensions.y, aabb.point.z + aabb.dimensions.z },
    { aabb.point.x + aabb.dimensions.x, aabb.point.y + aabb.dimensions.y, aabb.point.z + aabb.dimensions.z }
  };

  for (auto& p : points) {
    if (!IsInConvexHull(p, polygons)) {
      return false;
    }
  }
  return true;
}

AABB GetAABBFromVertices(const vector<vec3>& vertices) {
  AABB aabb;

  vec3 min_v = vec3(999999.0f, 999999.0f, 999999.0f);
  vec3 max_v = vec3(-999999.0f, -999999.0f, -999999.0f);
  for (const vec3& v : vertices) {
    min_v.x = std::min(min_v.x, v.x);
    max_v.x = std::max(max_v.x, v.x);
    min_v.y = std::min(min_v.y, v.y);
    max_v.y = std::max(max_v.y, v.y);
    min_v.z = std::min(min_v.z, v.z);
    max_v.z = std::max(max_v.z, v.z);
  }

  aabb.point = min_v;
  aabb.dimensions = max_v - min_v;
  return aabb;
}

Polygon CreatePolygonFrom3Points(vec3 a, vec3 b, vec3 c, vec3 direction) {
  vec3 normal = normalize(cross(a - b, a - c));
  if (dot(normal, direction) < 0) {
    normal = -normal;  
  }

  Polygon poly;
  poly.vertices.push_back(a);
  poly.vertices.push_back(b);
  poly.vertices.push_back(c);
  poly.normals.push_back(normal);
  poly.normals.push_back(normal);
  poly.normals.push_back(normal);
  return poly;
}

BoundingSphere GetBoundingSphereFromVertices(
  const vector<vec3>& vertices) {
  BoundingSphere bounding_sphere;
  bounding_sphere.center = vec3(0, 0, 0);

  float num_vertices = vertices.size();
  for (const vec3& v : vertices) {
    bounding_sphere.center += v;
  }
  bounding_sphere.center *= 1.0f / num_vertices;
  
  bounding_sphere.radius = 0.0f;
  for (const vec3& v : vertices) {
    bounding_sphere.radius = std::max(bounding_sphere.radius, 
      length(v - bounding_sphere.center));
  }
  return bounding_sphere;
}

bool IntersectBoundingSphereWithTriangle(const BoundingSphere& bounding_sphere, 
  const Polygon& polygon, vec3* displacement_vector) {
  const vec3& pos = bounding_sphere.center;
  float r = bounding_sphere.radius;

  const vec3& normal = polygon.normals[0];
  const vec3& a = polygon.vertices[0];
  const vec3& b = polygon.vertices[1];
  const vec3& c = polygon.vertices[2];

  // TODO: test against polygon bounding sphere and exit early.

  bool inside;
  vec3 closest_point = ClosestPtPointTriangle(pos, a, b, c, &inside);

  const vec3& v = closest_point - bounding_sphere.center;
  if (length(v) > r || dot(pos - a, normal) < 0) {
    *displacement_vector = vec3(0, 0, 0);
    return false;
  }

  // // Collide with edge.
  // if (!inside) {
  //   vec3 closest_ab = ClosestPtPointSegment(pos, a, b);
  //   vec3 closest_bc = ClosestPtPointSegment(pos, b, c);
  //   vec3 closest_ca = ClosestPtPointSegment(pos, c, a);
  //   vector<vec3> segments { a-b, b-c, c-a } ;
  //   vector<vec3> points { closest_ab, closest_bc, closest_ca } ;

  //   float min_distance = 9999.0f;
  //   float min_index = -1;
  //   for (int i = 0; i < 3; i++) {
  //     float distance = length(points[i] - pos);
  //     if (distance < min_distance) {
  //       min_index = i;
  //       min_distance = distance;
  //     }
  //   }

  //   if (min_distance < r) {
  //     vec3 pos_to_p = points[min_index] - pos;
  //     vec3 tangent = normalize(cross(segments[min_index], normal));
  //     float proj_tan = abs(dot(pos_to_p, tangent));
  //     float proj_normal = abs(dot(pos_to_p, normal));
  //     float magnitude = sqrt(r * r - proj_tan * proj_tan) - proj_normal + 0.001f;
  //     *displacement_vector = magnitude * normal;
  //     return true; 
  //   }
  // }

  float magnitude = r + dot(v, normal) + 0.001f;
  *displacement_vector = magnitude * normal;
  return true;
}

bool IntersectWithTriangle(const Polygon& polygon, 
  vec3* player_pos, vec3 old_player_pos, float* magnitude, 
  const vec3& object_pos, float radius) {
  const vec3& normal = polygon.normals[0];

  vec3 a = polygon.vertices[0] + object_pos;
  vec3 b = polygon.vertices[1] + object_pos;
  vec3 c = polygon.vertices[2] + object_pos;

  bool inside;
  vec3 closest_point = ClosestPtPointTriangle(*player_pos, a, b, c, &inside);
  
  float r = radius;

  const vec3& v = closest_point - *player_pos;
  if (length(v) > r || dot(*player_pos - a, normal) < 0) {
    return false;
  }

  // Collide with edge.
  if (!inside) {
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

  float mag = dot(v, normal);
  *magnitude = r + mag + 0.001f;
  *player_pos = *player_pos + *magnitude * normal;

  return true;
}

