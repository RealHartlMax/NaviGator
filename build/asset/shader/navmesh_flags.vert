#version 460

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in uint aFlags;

out VS_OUT {
    vec4 vertexColor;
} vs_out;

layout (std140, binding=0) uniform uSharedData {
  mat4 mProj;
  mat4 mView;
  mat4 mModel;
};

void main() {
  gl_Position = mProj * mView * mModel * vec4(aPos.xyz, 1.0);
  
  // Flag-based coloring
  // These values should match the navmesh flag definitions
  if ((aFlags & 1u) != 0u) {  // PAVED
    vs_out.vertexColor = vec4(1.0, 0.2, 0.2, 0.7);  // Red
  } else if ((aFlags & 2u) != 0u) {  // WATER
    vs_out.vertexColor = vec4(0.2, 1.0, 1.0, 0.7);  // Cyan
  } else if ((aFlags & 4u) != 0u) {  // STEEP
    vs_out.vertexColor = vec4(1.0, 0.6, 0.2, 0.7);  // Orange
  } else if ((aFlags & 8u) != 0u) {  // TRAFFIC
    vs_out.vertexColor = vec4(0.8, 0.2, 1.0, 0.7);  // Purple
  } else if ((aFlags & 16u) != 0u) {  // SLIPPERY
    vs_out.vertexColor = vec4(1.0, 1.0, 0.2, 0.7);  // Yellow
  } else {
    vs_out.vertexColor = vec4(0.5, 0.5, 0.5, 0.7);  // Gray (unknown/default)
  }
}
