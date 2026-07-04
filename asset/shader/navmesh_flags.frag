#version 460

in VS_OUT {
    vec4 vertexColor;
} fs_in;

out vec4 oPixelColor;

uniform float uAlpha = 1.0;

void main() {
  oPixelColor = fs_in.vertexColor;
  oPixelColor.a *= uAlpha;
}
