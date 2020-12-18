#version 330 core
layout(location = 0) in vec3 vertex_position;

uniform mat4 projection;

void main(){
  gl_Position = projection * vec4(vertex_position, 1);
}
