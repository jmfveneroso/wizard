#version 330 core

layout(location = 0) in vec3 vertex_pos_modelspace;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 vertex_normal_modelspace;
layout(location = 3) in mat4 M;

// Output data ; will be interpolated for each fragment.
out VertexData {
  vec3 position;
  vec2 UV;
  vec3 normal;
} out_data;

uniform mat4 VP; 
uniform mat4 V; 
uniform mat4 P; 

void main() {
  vec4 position = vec4(vertex_pos_modelspace, 1.0);
  vec4 normal = vec4(vertex_normal_modelspace, 0.0);

  mat4 MVP = P * V * M;
  gl_Position = MVP * position;

  vec3 vertex_pos_worldspace = (M * position).xyz;
  out_data.position = vertex_pos_worldspace;
  out_data.UV = uv;
  out_data.normal = (V * M * normal).xyz; 
}
