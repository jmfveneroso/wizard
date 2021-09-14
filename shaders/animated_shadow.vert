#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 5) in ivec3 bone_ids;
layout(location = 6) in vec3 bone_weights;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;

// Animation.
uniform mat4 joint_transforms[200];

void main(){
  vec4 position = vec4(0.0);
  for (int i = 0; i < 3; i++) {
    int index = bone_ids[i];
    if (index == -1) continue;

    float weight = bone_weights[i];
   
    vec4 pos_ = joint_transforms[index] * vec4(vertexPosition_modelspace, 1.0);
    position += pos_ * weight;
  }

  gl_Position =  MVP * position;
}
