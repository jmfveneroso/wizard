#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec2 vertexUV;
layout(location = 2) in vec3 vertexNormal_modelspace;
layout(location = 3) in mat4 M;
layout(location = 8) in vec3 vertexTangent_modelspace;
layout(location = 9) in vec3 vertexBitangent_modelspace;

out VertexData {
  vec3 position;
  vec2 UV;
  vec3 light_dir_tangentspace;
  vec3 eye_dir_tangentspace;
  mat3 TBN;
} out_data;

uniform mat4 VP; 
uniform mat4 V; 
uniform mat4 P; 
uniform vec3 player_pos;
uniform vec3 light_direction;
uniform mat4 shadow_matrix0;

void main(){
  out_data.UV = vertexUV;
  vec4 position = vec4(vertexPosition_modelspace, 1.0);

  mat4 MVP = P * V * M;
  gl_Position = MVP * position;

  mat3 MV3x3 = mat3(V * M);
  vec3 tangent_cameraspace = normalize(MV3x3 * normalize(vertexTangent_modelspace));
  vec3 bitangent_cameraspace = normalize(MV3x3 * normalize(vertexBitangent_modelspace));
  vec3 normal_cameraspace = normalize(MV3x3 * normalize(vertexNormal_modelspace));

  out_data.TBN = transpose(mat3(
    tangent_cameraspace,
    bitangent_cameraspace,
    normal_cameraspace
  ));

  vec3 vertex_pos_worldspace = (M * position).xyz;
  vec3 vertex_pos_cameraspace = (V * vec4(vertex_pos_worldspace, 1)).xyz;
  vec3 eye_dir_cameraspace = normalize(-vertex_pos_cameraspace);

  vec3 light_pos_worldspace = player_pos;
  // vec3 light_pos_worldspace = vertex_pos_worldspace + light_direction;
  vec3 light_pos_cameraspace = (V * vec4(light_pos_worldspace, 1)).xyz;
  vec3 light_dir_cameraspace = normalize(light_pos_cameraspace - vertex_pos_cameraspace);

  out_data.eye_dir_tangentspace =  normalize(out_data.TBN * eye_dir_cameraspace);
  out_data.light_dir_tangentspace = normalize(out_data.TBN * light_dir_cameraspace);

  out_data.position = vec3(M * position);
}
