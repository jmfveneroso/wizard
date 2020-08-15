#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertex_position_modelspace;

out FragData {
  vec2 uv;
  vec3 to_camera_vector;
  vec3 from_light_vector;
  float visibility;
} out_data;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;
uniform mat4 V;
uniform mat4 M;
uniform mat3 MV3x3;

// uniform sampler2DRect NormalsSampler;
// uniform sampler2DRect HeightMapSampler;
uniform int TILE_SIZE;

uniform vec3 camera_position;
uniform float move_factor;

const vec3 light_position_worldspace = vec3(1000, 1000, 0);
 
void main(){
  // Output position of the vertex, in clip space : MVP * position
  vec3 position = vertex_position_modelspace;
  position.x *= TILE_SIZE;
  position.z *= TILE_SIZE;

  // Waves.
  vec2 pos_world = (M * vec4(position, 1)).xz;
  position.y = -200 + 2 * (sin(0.1 * pos_world.x + 0.5 * move_factor * 10) + sin(0.1 * pos_world.y));

  gl_Position = MVP * vec4(position, 1);
  
  vec3 position_worldspace = (M * vec4(position, 1)).xyz;
  // out_data.uv = position_worldspace.xz / 32;
  out_data.uv = pos_world / 32;

  out_data.to_camera_vector = camera_position - position_worldspace;
  out_data.from_light_vector = position_worldspace - light_position_worldspace;

  vec3 position_cameraspace = (V * M * vec4(position, 1)).xyz;
  float distance = length(position_cameraspace);
  out_data.visibility = clamp(distance / 2000, 0.0, 1.0);
}
