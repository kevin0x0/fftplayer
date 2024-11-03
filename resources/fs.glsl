#version 460 core

in float w;
out vec4 fragment;

void main() {
  fragment = vec4(1.0 * (1 - sqrt(w)), 1.0 * sqrt(w), 0.0, 1.0);
}
