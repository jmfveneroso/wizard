#version 330
 
layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;
 
in VertexData {
  vec3 position;
  vec2 UV;
  vec3 normal_cameraspace;
  vec3 coarser_normal_cameraspace;
  float visibility;
  vec3 position_cameraspace;
  vec2 tileset;
  vec3 blending;
  vec3 coarser_blending;
  float alpha;
  vec4 shadow_coord; 
  vec4 shadow_coord1; 
  vec4 shadow_coord2; 
} in_data[];
 
out FragData {
  vec3 position;
  vec2 UV;
  vec3 normal_cameraspace;
  vec3 coarser_normal_cameraspace;
  float visibility;
  vec3 position_cameraspace;
  vec3 barycentric;
  vec2 tileset;
  vec3 blending;
  vec3 coarser_blending;
  float alpha;
  vec4 shadow_coord; 
  vec4 shadow_coord1; 
  vec4 shadow_coord2; 
} out_data;

void main() {
  for (int i = 0; i < 3; i++) {
    gl_Position = gl_in[i].gl_Position;
    out_data.position = in_data[i].position;
    out_data.UV = in_data[i].UV;
    out_data.normal_cameraspace = in_data[i].normal_cameraspace;
    out_data.coarser_normal_cameraspace = in_data[i].coarser_normal_cameraspace;
    out_data.visibility = in_data[i].visibility;
    out_data.position_cameraspace = in_data[i].position_cameraspace;
    out_data.barycentric = vec3(0.0);
    out_data.barycentric[i] = 1.0;
    out_data.blending = in_data[i].blending;
    out_data.shadow_coord = in_data[i].shadow_coord;
    out_data.shadow_coord1 = in_data[i].shadow_coord1;
    out_data.shadow_coord2 = in_data[i].shadow_coord2;
    out_data.coarser_blending = in_data[i].coarser_blending;
    // out_data.tileset = in_data[0].tileset;
    out_data.tileset = vec2(0, 0);
    out_data.alpha = in_data[i].alpha;
    EmitVertex();
  }
}
