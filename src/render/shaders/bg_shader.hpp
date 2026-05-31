#pragma once

static const char* BG_VERT_GLSL = R"GLSL(
#version 450 core
layout(location = 0) in vec2 a_pos;
out vec2 v_uv;
void main() {
    v_uv = a_pos * 0.5 + 0.5;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)GLSL";

static const char* BG_FRAG_GLSL = R"GLSL(
#version 450 core
in vec2 v_uv;
uniform vec3 u_colorTop;
uniform vec3 u_colorBot;
out vec4 frag_color;
void main() {
    frag_color = vec4(mix(u_colorBot, u_colorTop, v_uv.y), 1.0);
}
)GLSL";
