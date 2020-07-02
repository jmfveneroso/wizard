#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec2 vertexUV;
layout(location = 2) in vec3 vertexNormal_modelspace;
layout(location = 3) in vec3 bone_weights;

out VertexData {
  vec3 position;
  vec2 UV;
  vec3 normal;
} out_data;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;
uniform mat4 M;
uniform mat4 V;

// Animation.
uniform mat4 joint_transforms[2];

void main(){
  out_data.UV = vertexUV;

  // vec4 position = vec4(vertexPosition_modelspace, 1.0);
  // vec4 normal = vec4(vertexNormal_modelspace, 0.0);

  vec4 position = vec4(0.0);
  vec4 normal = vec4(0.0);
  for (int i = 0; i < 2; i++) {
    float weight = bone_weights[i];
    // float weight = 0.5;
   
    vec4 pos_ = joint_transforms[i] * vec4(vertexPosition_modelspace.xzy, 1.0);
    position += pos_ * weight;

    vec4 normal_ = joint_transforms[i] * vec4(vertexNormal_modelspace.xzy, 0.0);
    normal += normal_ * weight;
  }

  out_data.position = (M * position).xyz;
  gl_Position = MVP * position;
  out_data.normal = (V * M * normal).xyz; 
}
