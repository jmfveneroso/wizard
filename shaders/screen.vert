#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;

out FragData {
  vec3 position;
  vec2 uv;
} out_data;

void main(){
  out_data.position = position;
  out_data.uv = uv;
  gl_Position = vec4(position, 1.0);
}
