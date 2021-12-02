#include "4d.hpp"

Project4D::Project4D(shared_ptr<Resources> asset_catalog) 
  : resources_(asset_catalog) {
}

void RotateAroundPlane(const mat4& m, vector<vec4>& vertices) {
  for (auto& v : vertices) {
    v = m * v;
  }
}

void RotateAroundXY(vector<vec4>& vertices, float alpha) {
  float s = sin(alpha);
  float c = cos(alpha);
  const mat4 m {
    c, -s, 0, 0,
    s, c,  0, 0,
    0, 0,  1, 0,
    0, 0,  0, 1
  };
  RotateAroundPlane(m, vertices);
}

void RotateAroundXZ(vector<vec4>& vertices, float alpha) {
  float s = sin(alpha);
  float c = cos(alpha);
  const mat4 m {
    c, 0, -s, 0,
    0, 1,  0, 0,
    s, 0,  c, 0,
    0, 0,  0, 1
  };
  RotateAroundPlane(m, vertices);
}

void RotateAroundXW(vector<vec4>& vertices, float alpha) {
  float s = sin(alpha);
  float c = cos(alpha);
  const mat4 m {
    c, 0, 0, -s,
    0, 1, 0, 0,
    0, 0, 1, 0,
    s, 0, 0, c
  };
  RotateAroundPlane(m, vertices);
}

void RotateAroundYZ(vector<vec4>& vertices, float alpha) {
  float s = sin(alpha);
  float c = cos(alpha);
  const mat4 m {
    1, 0,  0,  0,
    0, c,  -s, 0,
    0, s,  c,  0,
    0, 0,  0,  1
  };
  RotateAroundPlane(m, vertices);
}

void RotateAroundYW(vector<vec4>& vertices, float alpha) {
  float s = sin(alpha);
  float c = cos(alpha);
  const mat4 m {
    1, 0, 0, 0,
    0, c, 0, -s,
    0, 0, 1, 0,
    0, s, 0, c
  };
  RotateAroundPlane(m, vertices);
}

void RotateAroundZW(vector<vec4>& vertices, float alpha) {
  float s = sin(alpha);
  float c = cos(alpha);
  const mat4 m {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, c, -s,
    0, 0, s, c
  };
  RotateAroundPlane(m, vertices);
}

void Project4D::BuildCube(int i, const vector<vec3>& v, const vec3& pos) {
  vector<vec3> vertices = {
    v[0], v[4], v[1], v[1], v[4], v[5], // Real top.
    v[1], v[3], v[0], v[0], v[3], v[2], // Real back.
    v[0], v[2], v[4], v[4], v[2], v[6], // Real left.
    v[5], v[7], v[1], v[1], v[7], v[3], // Real right.
    v[4], v[6], v[5], v[5], v[6], v[7], // Real front.
    v[6], v[2], v[7], v[7], v[2], v[3]  // Real bottom.
  };

  vector<vec2> u = {
    vec2(0, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1), // Top.
    vec2(0, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1), // Back.
    vec2(0, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1)  // Left.
  };

  vector<vec2> uvs {
    u[0], u[1], u[2],  u[2],  u[1], u[3],  // Top.
    u[4], u[5], u[6],  u[6],  u[5], u[7],  // Back.
    u[8], u[9], u[10], u[10], u[9], u[11], // Left.
    u[8], u[9], u[10], u[10], u[9], u[11], // Right.
    u[4], u[5], u[6],  u[6],  u[5], u[7],  // Front.
    u[0], u[1], u[2],  u[2],  u[1], u[3]   // Bottom.
  };

  vector<vec3> normals = {
    vec3(0, 1, 0), vec3(0, 1, 0), 
    vec3(0, -1, 0), vec3(0, -1, 0), 
    vec3(-1, 0, 0), vec3(-1, 0, 0), 
    vec3(1, 0, 0), vec3(1, 0, 0), 
    vec3(0, 1, 0), vec3(0, 1, 0), 
    vec3(0, -1, 0), vec3(0, -1, 0),
  };

  vector<unsigned int> indices(36);
  for (int i = 0; i < 36; i++) { indices[i] = i; }

  vector<Polygon> polygons;
  for (int i = 0; i < 12; i++) { 
    Polygon p;
    p.vertices.push_back(vertices[i]);
    p.vertices.push_back(vertices[i+1]);
    p.vertices.push_back(vertices[i+2]);
    p.normal = normals[i];
    polygons.push_back(p);
  }

  if (cubes_[i]) {
    const string mesh_name = cubes_[i]->GetAsset()->lod_meshes[0];
    shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
  
    UpdateMesh(*mesh, vertices, uvs, indices);
    mesh->polygons = polygons;
  } else {
    Mesh m = CreateMesh(0, vertices, uvs, indices);
    cubes_[i] = CreateGameObjFromMesh(resources_.get(), m, "hypercube", pos, 
      polygons);
    cubes_[i]->draw = false;
  }
}

vector<vec3> Project4D::StereographicalProjection(
  const vector<vec4>& vertices) {
  vector<vec3> proj_vertices;
  for (const auto& v : vertices) {
    vec3 proj_v = vec3(v.x, v.y, v.z) / (5 - v.w);
    proj_vertices.push_back(proj_v);
  }
  return proj_vertices;
}

vector<vec3> Project4D::OrthogonalProjection(const vector<vec4>& vertices) {
  vector<vec3> proj_vertices;
  for (const auto& v : vertices) {
    vec3 proj_v = vec3(v.x, v.y, v.z);
    proj_vertices.push_back(proj_v);
  }
  return proj_vertices;
}

void Project4D::CreateHypercube(const vec3 position, 
  const vector<float>& rotations) {

  float s = 2.5; 
  vector<vec4> v_hyper = {
    { -s, s, -s, -s }, // Back face.
    { s, s, -s, -s },
    { -s, -s, -s, -s },
    { s, -s, -s, -s },
    { -s, s, s, -s }, // Front face.
    { s, s, s, -s },
    { -s, -s, s, -s },
    { s, -s, s, -s },
    { -s, s, -s, s }, // Hyper back face.
    { s, s, -s, s },
    { -s, -s, -s, s },
    { s, -s, -s, s },
    { -s, s, s, s }, // Hyper front face.
    { s, s, s, s },
    { -s, -s, s, s },
    { s, -s, s, s }
  };

  RotateAroundXY(v_hyper, rotations[0]);
  RotateAroundXZ(v_hyper, rotations[1]);
  RotateAroundXW(v_hyper, rotations[2]);
  RotateAroundYZ(v_hyper, rotations[3]);
  RotateAroundYW(v_hyper, rotations[4]);
  RotateAroundZW(v_hyper, rotations[5]);

  vector<vec3> v = StereographicalProjection(v_hyper);

  // Real.
  // Hyper.
  // Top.
  // Back.
  // Left.
  // Right.
  // Front.
  // Bottom.
  vector<vector<vec3>> cubes {
    { v[0 ], v[1 ], v[2 ], v[3 ], v[4 ], v[5 ], v[6 ], v[ 7] },
    { v[8 ], v[9 ], v[10], v[11], v[12], v[13], v[14], v[15] },
    { v[8 ], v[9 ], v[0 ], v[1 ], v[12], v[13], v[4 ], v[5 ] },
    { v[8 ], v[9 ], v[10], v[11], v[0 ], v[1 ], v[2 ], v[3 ] },
    { v[8 ], v[0 ], v[10], v[2 ], v[12], v[4 ], v[14], v[6 ] },
    { v[1 ], v[9 ], v[3 ], v[11], v[5 ], v[13], v[7 ], v[15] },
    { v[4 ], v[5 ], v[6 ], v[7 ], v[12], v[13], v[14], v[15] },
    { v[2 ], v[3 ], v[10], v[11], v[6 ], v[7 ], v[14], v[15] }
  };

  for (int i = 0; i < 8; i++) {
    BuildCube(i, cubes[i], position);
  }
}
