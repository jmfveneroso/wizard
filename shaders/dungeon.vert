#version 330 core

layout(location = 0) in vec3 vertex_pos_modelspace;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 vertex_normal_modelspace;
layout(location = 3) in mat4 M;
layout(location = 8) in vec3 vertex_tangent_modelspace;
layout(location = 9) in vec3 vertex_bitangent_modelspace;

// Output data ; will be interpolated for each fragment.
out VertexData {
  vec3 position;
  vec2 UV;
  vec3 normal;
  vec3 light_dir_tangentspace;
  vec3 eye_dir_tangentspace;
  vec4 shadow_coord; 
} out_data;

uniform mat4 VP; 
uniform mat4 V; 
uniform mat4 P; 
uniform vec3 player_pos;
uniform mat4 shadow_matrix0;

void main() {
  vec4 position = vec4(vertex_pos_modelspace, 1.0);
  vec4 normal = vec4(vertex_normal_modelspace, 0.0);

  mat4 MVP = P * V * M;
  gl_Position = MVP * position;

  vec3 vertex_pos_worldspace = (M * position).xyz;
  out_data.position = vertex_pos_worldspace;
  out_data.UV = uv;
  out_data.normal = (V * M * normal).xyz; 

  mat3 MV3x3 = mat3(V * M);
  vec3 normal_cameraspace = MV3x3 * normalize(vertex_normal_modelspace);
  vec3 tangent_cameraspace = MV3x3 * normalize(vertex_tangent_modelspace);
  vec3 bitangent_cameraspace = MV3x3 * normalize(vertex_bitangent_modelspace);

  mat3 TBN = transpose(mat3(
    tangent_cameraspace,
    bitangent_cameraspace,
    normal_cameraspace
  ));

  vec3 light_pos_worldspace = player_pos + vec3(0, 5, 0);

  vec3 vertex_pos_cameraspace = (V * M * vec4(vertex_pos_modelspace, 1)).xyz;
  vec3 eye_dir_cameraspace = vec3(0, 0, 0) - vertex_pos_cameraspace;

  vec3 light_pos_cameraspace = (V * vec4(light_pos_worldspace, 1)).xyz;
  vec3 light_dir_cameraspace = light_pos_cameraspace + eye_dir_cameraspace;

  out_data.light_dir_tangentspace = normalize(TBN * light_dir_cameraspace);
  out_data.eye_dir_tangentspace =  normalize(TBN * eye_dir_cameraspace);

  mat4 DepthMVP = shadow_matrix0 * M;
  out_data.shadow_coord = DepthMVP * position;
}
