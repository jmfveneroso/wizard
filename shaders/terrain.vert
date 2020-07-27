#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 position_modelspace;

out VertexData {
  vec3 position;
  vec2 UV;
  vec3 normal_cameraspace;
  float visibility;
  vec3 position_cameraspace;
  vec3 blending;
  vec2 tileset;
} out_data;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;
uniform mat4 V;
uniform mat4 M;
uniform mat3 MV3x3;
uniform sampler2DRect height_sampler;
uniform sampler2DRect normal_sampler;
uniform sampler2DRect blending_sampler;
uniform sampler2DRect tile_type_sampler;
uniform int TILE_SIZE;
uniform int PURE_TILE_SIZE;
uniform int CLIPMAP_SIZE;
uniform int TILES_PER_TEXTURE;
uniform float MAX_HEIGHT;
uniform ivec2 buffer_top_left;

void main(){
  out_data.position = position_modelspace;

  ivec2 toroidal_coords = ivec2(out_data.position.x, out_data.position.z);
  ivec2 buffer_coords = (toroidal_coords + buffer_top_left + CLIPMAP_SIZE + 1) % (CLIPMAP_SIZE + 1);
  float height = MAX_HEIGHT * texelFetch(height_sampler, buffer_coords).r;
  vec3 normal = texelFetch(normal_sampler, buffer_coords).rgb * 2 - 1;
  vec3 blending = texelFetch(blending_sampler, buffer_coords).rgb;
  vec2 tileset = texelFetch(tile_type_sampler, buffer_coords).rg;

  out_data.position.x *= TILE_SIZE;
  out_data.position.z *= TILE_SIZE;
  out_data.position.y = height;
  gl_Position = MVP * vec4(out_data.position, 1);

  vec3 position_worldspace = (M * vec4(out_data.position, 1)).xyz;
  out_data.normal_cameraspace = (V * M * vec4(normal, 0)).xyz; 
  out_data.position_cameraspace = (V * M * vec4(out_data.position, 1)).xyz;
  out_data.tileset = tileset;
  out_data.blending = blending;
  
  vec2 uv = position_worldspace.xz / (TILES_PER_TEXTURE * PURE_TILE_SIZE);
  out_data.UV = uv;

  // vec2 uv = (ivec2(in_data.UV) / PURE_TILE_SIZE) * PURE_TILE_SIZE;
  // uv = (in_data.UV - uv) / PURE_TILE_SIZE;

  float distance = length(out_data.position_cameraspace.xyz);
  out_data.visibility = clamp(distance / 1000, 0.0, 1.0);
}
