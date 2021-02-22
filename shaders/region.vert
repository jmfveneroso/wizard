#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec2 vertexUV;
layout(location = 2) in vec3 vertexNormal_modelspace;

out FragData {
  vec3 position;
  vec2 UV;
  vec3 normal;
} out_data;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;
uniform mat4 M;
uniform mat4 V;

void main(){
  out_data.UV = vertexUV;
  out_data.position = (M * vec4(vertexPosition_modelspace, 1)).xyz;
  gl_Position = MVP * vec4(vertexPosition_modelspace, 1);
  out_data.normal = (V * M * vec4(vertexNormal_modelspace,0)).xyz; 
}
