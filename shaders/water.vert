#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertex_position_modelspace;

out FragData {
  vec3 position;
  vec2 uv;
  vec3 to_camera_vector;
  vec3 from_light_vector;
  float visibility;
  float shalowness;
} out_data;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;
uniform mat4 V;
uniform mat4 M;
uniform mat3 MV3x3;
uniform float MAX_HEIGHT;
uniform ivec2 buffer_top_left;
uniform int CLIPMAP_SIZE;

uniform sampler2DRect height_sampler;
uniform int TILE_SIZE;

uniform vec3 camera_position;
uniform float move_factor;

uniform vec3 light_position_worldspace;
 
void main(){
  // Output position of the vertex, in clip space : MVP * position
  out_data.position = vertex_position_modelspace;

  ivec2 toroidal_coords = ivec2(out_data.position.x, out_data.position.z);
  ivec2 buffer_coords = (toroidal_coords + buffer_top_left + CLIPMAP_SIZE + 1) % (CLIPMAP_SIZE + 1);
  float height = MAX_HEIGHT * texelFetch(height_sampler, buffer_coords).r;

  out_data.position.x *= TILE_SIZE;
  out_data.position.z *= TILE_SIZE;

  // Waves.
  vec2 pos_world = (M * vec4(out_data.position, 1)).xz;
  out_data.position.y = 5 + 0.5 * (sin(0.1 * pos_world.x + 0.5 * move_factor * 30) + sin(0.1 * pos_world.y));
  float shalowness = (out_data.position.y - height) / 5;
  out_data.shalowness = 1 - clamp(shalowness, 0, 1);

  gl_Position = MVP * vec4(out_data.position, 1);
  
  vec3 position_worldspace = (M * vec4(out_data.position, 1)).xyz;
  out_data.uv = pos_world / 32;

  out_data.to_camera_vector = camera_position - position_worldspace;
  out_data.from_light_vector = position_worldspace - light_position_worldspace;

  vec3 position_cameraspace = (V * M * vec4(out_data.position, 1)).xyz;
  float distance = length(position_cameraspace);
  out_data.visibility = clamp(distance / 2000, 0.0, 1.0);
}
