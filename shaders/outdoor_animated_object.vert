#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec2 vertexUV;
layout(location = 2) in vec3 vertexNormal_modelspace;
layout(location = 3) in vec3 vertexTangent_modelspace;
layout(location = 4) in vec3 vertexBitangent_modelspace;
layout(location = 5) in ivec3 bone_ids;
layout(location = 6) in vec3 bone_weights;

out VertexData {
  vec3 position;
  vec2 UV;
  vec3 light_dir_tangentspace;
  vec3 light_dir_tangentspace_2;
  vec3 eye_dir_tangentspace;
  mat3 TBN;
} out_data;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;
uniform mat4 M;
uniform mat4 V;
uniform mat3 MV3x3;

uniform int enable_bump_map;
uniform vec3 light_direction;
uniform vec3 player_pos;

// Animation.
uniform mat4 joint_transforms[200];

void main(){
  out_data.UV = vertexUV;

  vec4 position = vec4(0.0);
  vec4 tangent = vec4(0.0);
  vec4 bitangent = vec4(0.0);
  vec4 normal = vec4(0.0);
  for (int i = 0; i < 3; i++) {
    int index = bone_ids[i];
    if (index == -1) continue;

    float weight = bone_weights[i];
   
    vec4 pos_ = joint_transforms[index] * vec4(vertexPosition_modelspace, 1.0);
    position += pos_ * weight;

    vec4 tangent_ = joint_transforms[index] * vec4(vertexTangent_modelspace, 0.0);
    tangent += tangent_ * weight;

    vec4 bitangent_ = joint_transforms[index] * vec4(vertexBitangent_modelspace, 0.0);
    bitangent += bitangent_ * weight;

    vec4 normal_ = joint_transforms[index] * vec4(vertexNormal_modelspace, 0.0);
    normal += normal_ * weight;
  }

  gl_Position = MVP * position;

  vec3 tangent_cameraspace = MV3x3 * vec3(tangent);
  vec3 bitangent_cameraspace = MV3x3 * vec3(bitangent);
  vec3 normal_cameraspace = MV3x3 * vec3(normal);

  out_data.TBN = transpose(mat3(
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

  vec3 light_pos_worldspace_2 = vertex_pos_worldspace + vec3(0, 1, 0);
  vec3 light_pos_cameraspace_2 = (V * vec4(light_pos_worldspace_2, 1)).xyz;
  vec3 light_dir_cameraspace_2 = normalize(light_pos_cameraspace_2 - vertex_pos_cameraspace);

  out_data.eye_dir_tangentspace =  normalize(out_data.TBN * eye_dir_cameraspace);
  out_data.light_dir_tangentspace = normalize(out_data.TBN * light_dir_cameraspace);
  out_data.light_dir_tangentspace_2 = normalize(out_data.TBN * light_dir_cameraspace_2);

  out_data.position = vec3(M * position);
}
