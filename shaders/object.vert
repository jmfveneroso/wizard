#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec2 vertexUV;
layout(location = 2) in vec3 vertexNormal_modelspace;
layout(location = 3) in vec3 vertexTangent_modelspace;
layout(location = 4) in vec3 vertexBitangent_modelspace;

out VertexData {
  vec3 position;
  vec2 UV;
  vec3 normal;
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

void main(){
  out_data.UV = vertexUV;
  vec4 position = vec4(vertexPosition_modelspace, 1.0);
  vec4 normal = vec4(vertexNormal_modelspace, 0.0);
  vec3 position_worldspace = (M * position).xyz;
  out_data.position = (M * position).xyz;
  gl_Position = MVP * position;
  out_data.normal = (V * M * normal).xyz; 

  vec3 normal_cameraspace = MV3x3 * normalize(vertexNormal_modelspace);
  vec3 tangent_cameraspace = MV3x3 * normalize(vertexTangent_modelspace);
  vec3 bitangent_cameraspace = MV3x3 * normalize(vertexBitangent_modelspace);

  mat3 TBN = transpose(mat3(
    tangent_cameraspace,
    bitangent_cameraspace,
    normal_cameraspace
  ));

  vec3 light_pos_worldspace = light_direction * 200;

  vec3 vertex_pos_cameraspace = (V * M * vec4(vertexPosition_modelspace, 1)).xyz;
  vec3 eye_dir_cameraspace = vec3(0, 0, 0) - vertex_pos_cameraspace;
  vec3 light_pos_cameraspace = (V * vec4(light_pos_worldspace, 1)).xyz;
  vec3 light_dir_cameraspace = light_pos_cameraspace + eye_dir_cameraspace;

  out_data.light_dir_tangentspace = normalize(TBN * light_dir_cameraspace);
  out_data.eye_dir_tangentspace =  normalize(TBN * eye_dir_cameraspace);
}

