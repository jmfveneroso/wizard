#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 square_vertices;
layout(location = 1) in vec4 xyzs; // Position of the center of the particule and size of the square
layout(location = 2) in vec4 color;
layout(location = 3) in vec2 uv;

// Output data ; will be interpolated for each fragment.
out vec4 clip_space;
out vec2 UV;
out vec4 particle_color;

// Values that stay constant for the whole mesh.
uniform vec3 camera_right_worldspace;
uniform vec3 camera_up_worldspace;
uniform float tile_size;
uniform int is_fixed;

// Model-View-Projection matrix, but without the Model (the position is in BillboardPos; the orientation depends on the camera)
uniform mat4 VP; 

void main() {
  float particle_size = xyzs.w; // because we encoded it this way.
  vec3 particle_center_worldspace = xyzs.xyz;
  
  vec3 vertex_pos_worldspace = particle_center_worldspace 
    + camera_right_worldspace * square_vertices.x * particle_size
    + camera_up_worldspace * square_vertices.y * particle_size;
  
  clip_space = VP * vec4(vertex_pos_worldspace, 1.0f);

  if (is_fixed > 0) {
    clip_space = vec4(xyzs.xyz, 1.0f) + 
      vec4(square_vertices.x, square_vertices.y, 0, 0) * particle_size;
  }

  // Output position of the vertex
  gl_Position = clip_space;

  UV = uv + square_vertices.xy * tile_size;
  
  particle_color = color;
}
