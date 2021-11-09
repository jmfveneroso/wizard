#version 330 core

in FragData {
  vec3 position;
  vec2 UV;
  vec3 normal_cameraspace;
  vec3 coarser_normal_cameraspace;
  vec3 position_cameraspace;
  vec3 light_dir_tangentspace;
  float alpha;
} in_data;

// Output data
layout(location = 0) out vec3 color;

// Values that stay constant for the whole mesh.
uniform sampler2D texture_sampler;
uniform sampler2D texture1_sampler;
uniform sampler2D texture2_sampler;
uniform sampler2D texture3_sampler;
uniform sampler2D texture4_sampler;
uniform sampler2D texture5_sampler;
uniform sampler2D texture6_sampler;
uniform sampler2D texture7_sampler;
uniform sampler2DShadow shadow_sampler;
uniform sampler2DShadow shadow_sampler1;
uniform sampler2DShadow shadow_sampler2;
uniform mat4 MVP;
uniform mat4 V;
uniform mat4 M;
uniform mat3 MV3x3;
uniform int PURE_TILE_SIZE;
vec2 tile_offsets[4] = vec2[](vec2(0, 0), vec2(0.5, 0), vec2(0, 0.5), vec2(0.5, 0.5));

uniform int clip_against_plane;
uniform int show_grid;
uniform int draw_shadows;
uniform vec3 clipping_point;
uniform vec3 clipping_normal;
uniform vec3 camera_position;
uniform vec3 light_direction;

vec3 GetSky(vec3 position) {
  vec3 night_sky = vec3(0.07, 0.07, 0.25);
  vec3 sky_color = vec3(0.4, 0.7, 0.8);
  vec3 sunset_color = vec3(0.99, 0.54, 0.0);

  float sun_pos = 1.5 * (1 - dot(vec3(0, 1, 0), light_direction));
  sun_pos = clamp(sun_pos, 0.0, 1.0);

  float sky_pos = dot(vec3(0, 1, 0), position);

  vec3 color = mix(sky_color, sunset_color, sun_pos);
  color = mix(color, sky_color, sky_pos);

  float how_night = dot(vec3(0, -1, 0), light_direction);
  how_night = clamp(how_night, 0.0, 1.0);
  color = mix(color, night_sky, how_night);

  return color;
}

void main() {
  // Discard fragment if it is in front of the clipping plane.
  if (clip_against_plane > 0) {
    if (dot(in_data.position - clipping_point, clipping_normal) > 0.0) {
      discard;
    }
  }

  // vec2 uv = fract(in_data.UV);
  vec2 uv = in_data.UV;

  // Texture.
  vec3 diffuse_color;
  diffuse_color = vec3(0.8, 0.8, 0.8);

  float weight = 0.01f;

  vec3 normal_color = vec3(0, 1, 0);
  vec3 tex_normal_tangentspace = normalize(normal_color* 2.0 - 1.0);
  vec3 n = tex_normal_tangentspace;
  vec3 l = in_data.light_dir_tangentspace;

  // How to set normal strength: https://computergraphics.stackexchange.com/questions/5411/correct-way-to-set-normal-strength/5412
  n.xy *= 3.0;
  n = normalize(n);
  float cos_theta = max(dot(n, l), 0.0);

  // Ambient.
  vec3 ambient_color = vec3(0.5, 0.5, 0.5) * diffuse_color;
  vec3 out_color = ambient_color;

  // Light.
  vec3 light_color = vec3(1, 1, 0.75);
  float light_power = 0.85;

  // Sun light.
  float sun_intensity = (1.0 + dot(light_direction, vec3(0, 1, 0))) * 0.25;
  out_color += sun_intensity * (diffuse_color * light_color * light_power * cos_theta);

  float d = length(in_data.position - camera_position);
  float depth = clamp(d / 5000.0f, 0, 0.9);
  vec3 pos = normalize(in_data.position - (camera_position - vec3(0, 1000, 0)));
  vec3 fog_color = GetSky(pos);

  float h = clamp((40.0f + in_data.position.y) / 90.0f, 0.1, 1.0);
  vec3 depth_color = vec3(0.4, 0.4, 0.4);
  out_color = mix(out_color, depth_color, 1 - h);
  out_color = mix(out_color, fog_color, depth);
  color = out_color;
}
