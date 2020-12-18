#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertex_position_modelspace;
layout(location = 1) in vec2 uv;

out FragData {
  vec3 position;
  vec2 uv;
  vec3 to_camera_vector;
  vec3 from_light_vector;
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
  out_data.uv = uv ;
  vec3 position_worldspace = (M * vec4(out_data.position, 1)).xyz;

  gl_Position = MVP * vec4(out_data.position, 1);

  out_data.to_camera_vector = camera_position - position_worldspace;
  out_data.from_light_vector = position_worldspace - light_position_worldspace;
}
