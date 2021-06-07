#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec2 vertexUV;
layout(location = 2) in vec3 vertexNormal_modelspace;
layout(location = 3) in ivec3 bone_ids;
layout(location = 4) in vec3 bone_weights;

// layout(location = 3) in vec3 vertexTangent_modelspace;
// layout(location = 4) in vec3 vertexBitangent_modelspace;
// layout(location = 5) in ivec3 bone_ids;
// layout(location = 6) in vec3 bone_weights;

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
uniform vec2 tile_pos;
uniform float tile_size;

// Animation.
uniform float enable_animation;
uniform mat4 joint_transforms[200];

void main(){
  out_data.UV = tile_pos + vertexUV * tile_size;
  // out_data.UV = vec2(0.5, 0.5) + vertexUV * 0.167;
  // out_data.UV = vertexUV;

  vec4 position = vec4(vertexPosition_modelspace, 1.0);
  vec4 normal = vec4(vertexNormal_modelspace, 0.0);

  if (enable_animation > 0) {
    position = vec4(0.0);
    normal = vec4(0.0);
    for (int i = 0; i < 3; i++) {
      int index = bone_ids[i];
      if (index == -1) continue;

      float weight = bone_weights[i];
     
      vec4 pos_ = joint_transforms[index] * vec4(vertexPosition_modelspace, 1.0);
      position += pos_ * weight;

      vec4 normal_ = joint_transforms[index] * vec4(vertexNormal_modelspace, 0.0);
      normal += normal_ * weight;
    }
  }

  vec3 position_worldspace = (M * position).xyz;
  out_data.position = (M * position).xyz;

  gl_Position = MVP * position;
  out_data.normal = (V * M * normal).xyz; 

  // vec3 normal_cameraspace = MV3x3 * normalize(vertexNormal_modelspace);
  // vec3 tangent_cameraspace = MV3x3 * normalize(vertexTangent_modelspace);
  // vec3 bitangent_cameraspace = MV3x3 * normalize(vertexBitangent_modelspace);

  // mat3 TBN = transpose(mat3(
  //   tangent_cameraspace,
  //   bitangent_cameraspace,
  //   normal_cameraspace
  // ));

  // vec3 light_pos_worldspace = light_direction * 200;

  // vec3 vertex_pos_cameraspace = (V * M * vec4(vertexPosition_modelspace, 1)).xyz;
  // vec3 eye_dir_cameraspace = vec3(0, 0, 0) - vertex_pos_cameraspace;
  // vec3 light_pos_cameraspace = (V * vec4(light_pos_worldspace, 1)).xyz;
  // vec3 light_dir_cameraspace = light_pos_cameraspace + eye_dir_cameraspace;

  // out_data.light_dir_tangentspace = normalize(TBN * light_dir_cameraspace);
  // out_data.eye_dir_tangentspace =  normalize(TBN * eye_dir_cameraspace);
}

