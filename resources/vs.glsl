#version 460 core

layout (location = 0) in vec2 aPos;
// layout (location = 1) in vec3 aColor;

uniform float channel;
uniform float total_channel;

out float w;

void main() {
  vec2 pos;
  pos.y = (aPos.y + channel) / total_channel * 2 - 1;
  pos.x = aPos.x * 2 - 1;
  gl_Position = vec4(pos, 0.0, 1.0);
  w = aPos.x;
  // color = aColor;
}
