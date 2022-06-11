#version 330 core

layout(location = 0) in vec3 square_vertices;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 vertexNormal_modelspace;
layout(location = 3) in vec3 vertexTangent_modelspace;
layout(location = 4) in vec3 vertexBitangent_modelspace;

// Output data; will be interpolated for each fragment.
out VertexData {
  vec2 UV;
} out_data;

// Values that stay constant for the whole mesh.
uniform mat4 M;
uniform vec3 camera_right_worldspace;
uniform vec3 camera_up_worldspace;

// Model-View-Projection matrix, but without the Model (the position is in BillboardPos; the orientation depends on the camera)
uniform mat4 VP; 

void main() {
  vec3 particle_center_worldspace = (M * vec4(0, 0, 0, 1)).xyz;
  float particle_size = 1.0f;
  
  vec3 vertex_pos_worldspace = particle_center_worldspace 
    + camera_right_worldspace * square_vertices.x * particle_size
    + camera_up_worldspace * square_vertices.y * particle_size;
  
  vec4 clip_space = VP * vec4(vertex_pos_worldspace, 1.0f);

  // Output position of the vertex
  gl_Position = clip_space;

  out_data.UV = uv;
}
