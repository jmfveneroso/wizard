#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 position_modelspace;
layout(location = 1) in vec2 vertex_uv;

out VertexData {
  vec3 position;
  vec2 UV;
  vec3 normal_cameraspace;
  vec3 coarser_normal_cameraspace;
  float visibility;
  vec3 position_cameraspace;
  vec3 blending;
  vec3 coarser_blending;
  vec2 tileset;
  float alpha;
} out_data;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;
uniform mat4 V;
uniform mat4 M;
uniform mat3 MV3x3;
uniform sampler2DRect height_sampler;
uniform sampler2DRect normal_sampler;
uniform sampler2DRect blending_sampler;
uniform sampler2DRect coarser_blending_sampler;
uniform sampler2DRect tile_type_sampler;
uniform int TILE_SIZE;
uniform int PURE_TILE_SIZE;
uniform int CLIPMAP_SIZE;
uniform int TILES_PER_TEXTURE;
uniform float MAX_HEIGHT;
uniform ivec2 buffer_top_left;
uniform vec3 player_pos;

void main(){
  out_data.position = position_modelspace;

  ivec2 toroidal_coords = ivec2(out_data.position.x, out_data.position.z);
  ivec2 buffer_coords = (toroidal_coords + buffer_top_left + CLIPMAP_SIZE + 1) % (CLIPMAP_SIZE + 1);

  float height = MAX_HEIGHT * texelFetch(height_sampler, buffer_coords).r;
  // vec4 normal = texelFetch(normal_sampler, buffer_coords).rgba * 2 - 1;
  vec4 normal = texelFetch(normal_sampler, buffer_coords).rgba;
  vec3 blending = texelFetch(blending_sampler, buffer_coords).rgb;
  vec3 coarser_blending = texelFetch(coarser_blending_sampler, buffer_coords).rgb;
  vec2 tileset = texelFetch(tile_type_sampler, buffer_coords).rg;

  out_data.position.x *= TILE_SIZE;
  out_data.position.z *= TILE_SIZE;
  out_data.position.y = height;
  gl_Position = MVP * vec4(out_data.position, 1);

  vec3 position_worldspace = (M * vec4(out_data.position, 1)).xyz;

  // TODO: pick only the x and y (which is actually z) components assuming a 
  // (0, 1, 0) y component and generate normal.
  vec3 normalized_normal = normalize(vec3(normal.x, 1, normal.y));
  vec3 normalized_coarser_normal = normalize(vec3(normal.z, 1, normal.w));

  out_data.normal_cameraspace = (V * M * vec4(normalized_normal, 0)).xyz; 
  out_data.coarser_normal_cameraspace = (V * M * vec4(normalized_coarser_normal, 0)).xyz; 
  out_data.position_cameraspace = (V * M * vec4(out_data.position, 1)).xyz;
  out_data.tileset = tileset;
  out_data.blending = blending;
  out_data.coarser_blending = coarser_blending;
  
  out_data.UV = position_worldspace.xz / TILES_PER_TEXTURE;

  float distance = length(out_data.position_cameraspace.xyz);
  out_data.visibility = clamp(distance / 4000, 0.0, 1.0);

  // Transition alpha.
  float w = 0.5;
  float half_clipmap_size = ((CLIPMAP_SIZE + 1) * TILE_SIZE) / 2;
  float alpha = ((clamp((abs(position_worldspace.x - player_pos.x)) / half_clipmap_size, w, 1.0) - w) / (1 - w));
  out_data.alpha = max(alpha, (clamp((abs(position_worldspace.z - player_pos.z)) / half_clipmap_size, w, 1.0) - w) / (1 - w));
}
