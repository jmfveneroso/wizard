#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec2 vertexUV;

out VertexData {
  vec3 position;
  vec2 UV;
  vec3 color;
} out_data;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;
uniform mat4 V;
uniform mat4 M;
uniform mat3 MV3x3;

void main(){
  // Output position of the vertex, in clip space : MVP * position
  out_data.UV = vertexUV;
  out_data.position = (M * vec4(vertexPosition_modelspace, 1)).xyz;
  gl_Position = MVP * vec4(vertexPosition_modelspace, 1);
  out_data.color = vertexPosition_modelspace / 100000;
}
