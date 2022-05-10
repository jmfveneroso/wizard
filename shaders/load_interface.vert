#version 330 core
layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec2 uv;

uniform mat4 projection;

out FragData {
  vec2 uv;
} out_data;

void main(){
  out_data.uv = uv;
  gl_Position = projection * vec4(vertex_position, 1);
}
