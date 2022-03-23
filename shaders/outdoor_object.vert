#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec2 vertexUV;
layout(location = 2) in vec3 vertexNormal_modelspace;
layout(location = 3) in vec3 vertexTangent_modelspace;
layout(location = 4) in vec3 vertexBitangent_modelspace;

out VertexData {
  vec2 UV;
  vec3 light_dir_tangentspace;
  vec3 eye_dir_tangentspace;
} out_data;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;
uniform mat4 M;
uniform mat4 V;
uniform mat3 MV3x3;

uniform int enable_bump_map;
uniform vec3 light_direction;
uniform vec3 player_pos;

void main(){
  out_data.UV = vertexUV;
  vec4 position = vec4(vertexPosition_modelspace, 1.0);
  gl_Position = MVP * position;

  vec3 tangent_cameraspace = normalize(MV3x3 * normalize(vertexTangent_modelspace));
  vec3 bitangent_cameraspace = normalize(MV3x3 * normalize(vertexBitangent_modelspace));
  vec3 normal_cameraspace = normalize(MV3x3 * normalize(vertexNormal_modelspace));

  mat3 TBN = transpose(mat3(
    tangent_cameraspace,
    bitangent_cameraspace,
    normal_cameraspace
  ));

  vec3 vertex_pos_worldspace = (M * position).xyz;
  vec3 vertex_pos_cameraspace = (V * vec4(vertex_pos_worldspace, 1)).xyz;
  vec3 eye_dir_cameraspace = normalize(-vertex_pos_cameraspace);

  vec3 light_pos_worldspace = vertex_pos_worldspace + light_direction;
  vec3 light_pos_cameraspace = (V * vec4(light_pos_worldspace, 1)).xyz;
  vec3 light_dir_cameraspace = normalize(light_pos_cameraspace - vertex_pos_cameraspace);

  out_data.eye_dir_tangentspace =  normalize(TBN * eye_dir_cameraspace);
  out_data.light_dir_tangentspace = normalize(TBN * light_dir_cameraspace);
}
