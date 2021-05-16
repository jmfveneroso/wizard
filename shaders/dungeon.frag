#version 330 core

uniform sampler2D texture_sampler;

in VertexData {
  vec3 position;
  vec2 UV;
  vec3 normal;
} in_data;

// Output data
layout(location = 0) out vec4 color;

uniform vec3 light_direction;
uniform vec3 lighting_color;
uniform vec3 player_pos;
uniform mat4 V; 
uniform float light_radius;

void main() {
  vec3 diffuse_color = texture(texture_sampler, in_data.UV).rgb;

  vec3 light_color = vec3(1.0, 1.0, 1.0);
  float light_power = 0.5;

  vec3 ambient_color = lighting_color * diffuse_color;

  vec3 out_color = ambient_color;

  // Sun light.
  vec3 light_cameraspace = (V * vec4(light_direction, 0.0)).xyz;
  vec3 n = normalize(in_data.normal);
  vec3 l = normalize(light_cameraspace);
  float brightness = clamp(dot(n, l), 0, 1);
  out_color += diffuse_color * light_color * light_power * brightness;

  float d = distance(player_pos, in_data.position);
  float depth = clamp(d / light_radius, 0, 1);
  vec3 fog_color = vec3(0, 0, 0);
  out_color = mix(out_color, fog_color, depth);

  color = vec4(out_color, 1.0);
  // color = vec4(in_data.UV.x, in_data.UV.y, 0.0, 1.0);
}
