#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertexPosition_modelspace;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;
uniform sampler2DRect height_sampler;
uniform int TILE_SIZE;
uniform int PURE_TILE_SIZE;
uniform int CLIPMAP_SIZE;
uniform int TILES_PER_TEXTURE;
uniform float MAX_HEIGHT;
uniform ivec2 buffer_top_left;
uniform vec3 player_pos;
uniform sampler2DShadow shadow_map;

void main(){
  vec3 position = vertexPosition_modelspace;

  ivec2 toroidal_coords = ivec2(position.x, position.z);
  ivec2 buffer_coords = (toroidal_coords + buffer_top_left + CLIPMAP_SIZE + 1) % (CLIPMAP_SIZE + 1);

  float height = MAX_HEIGHT * texelFetch(height_sampler, buffer_coords).r;

  position.x *= TILE_SIZE;
  position.z *= TILE_SIZE;
  position.y = height;
  gl_Position = MVP * vec4(position, 1);
}
