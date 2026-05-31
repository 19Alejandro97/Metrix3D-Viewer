#pragma once

static const char* CAP_VERT_GLSL = R"GLSL(
#version 450 core
layout(location = 0) in vec3 a_position;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 v_worldPos;

void main() {
    vec4 wp    = u_model * vec4(a_position, 1.0);
    v_worldPos = wp.xyz;
    gl_Position = u_projection * u_view * wp;
}
)GLSL";

static const char* CAP_FRAG_GLSL = R"GLSL(
#version 450 core
in vec3 v_worldPos;

uniform vec4  u_color;
uniform vec4  u_planeColor; // Added for Axis-Tinting (R/G/B)
uniform vec3  u_normal;
uniform vec3  u_cameraPos;
uniform int   u_unlit;
uniform int   u_numClipPlanes;
uniform vec4  u_clipPlanes[6];
uniform int   u_clipPlanesActive[6];
uniform int   u_applyClipping;

out vec4 frag_color;

void main() {
    // 1. Clipping
    if (u_applyClipping != 0) {
        for (int i = 0; i < u_numClipPlanes; ++i) {
            if (u_clipPlanesActive[i] != 0) {
                float d = dot(vec4(v_worldPos, 1.0), u_clipPlanes[i]);
                if (d < 0.0) discard;
            }
        }
    }

    // 2. Axis-Tinting Logic (CAD Standard)
    // Mix 85% original piece color with 15% plane-axis color
    vec3 mixedColor = mix(u_color.rgb, u_planeColor.rgb, 0.15);

    // 3. Simple Capping Lighting
    vec3 litColor;
    if (u_unlit != 0) {
        litColor = mixedColor;
    } else {
        vec3 N = normalize(u_normal);
        vec3 V = normalize(u_cameraPos - v_worldPos);
        float diff = max(dot(N, V), 0.2); 
        litColor = mixedColor * diff;
    }

    frag_color = vec4(litColor, u_color.a);
}
)GLSL";
