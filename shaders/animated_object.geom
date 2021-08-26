#version 330
 
layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;
 
in VertexData {
  vec3 position;
  vec2 UV;
  vec3 normal;
  vec3 tangent;
  vec3 bitangent;
  vec3 light_dir_tangentspace;
  vec3 eye_dir_tangentspace;
} in_data[];
 
out FragData {
  vec3 position;
  vec2 UV;
  vec3 normal;
  vec3 barycentric;
  vec3 tangent;
  vec3 bitangent;
  vec3 light_dir_tangentspace;
  vec3 eye_dir_tangentspace;
} out_data;

void main() {
  for (int i = 0; i < 3; i++) {
    gl_Position = gl_in[i].gl_Position;

    out_data.position = in_data[i].position;
    out_data.UV = in_data[i].UV;
    out_data.normal = in_data[i].normal;
    out_data.barycentric = vec3(0.0);
    out_data.barycentric[i] = 1.0;
    out_data.tangent = in_data[i].tangent;
    out_data.bitangent = in_data[i].bitangent;
    out_data.light_dir_tangentspace = in_data[i].light_dir_tangentspace;
    out_data.eye_dir_tangentspace = in_data[i].eye_dir_tangentspace;

    EmitVertex();
  }
}

