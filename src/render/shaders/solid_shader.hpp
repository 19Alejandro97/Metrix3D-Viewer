#pragma once

static const char* SOLID_VERT_GLSL = R"GLSL(
#version 450 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat3 u_normalMatrix;

out vec3 v_fragPos;
out vec3 v_normal;

void main() {
    vec4 worldPos = u_model * vec4(a_position, 1.0);
    v_fragPos     = worldPos.xyz;
    v_normal      = normalize(u_normalMatrix * a_normal);
    gl_Position   = u_projection * u_view * worldPos;
}
)GLSL";

static const char* SOLID_FRAG_GLSL = R"GLSL(
#version 450 core
in vec3 v_fragPos;
in vec3 v_normal;

uniform vec3  u_cameraPos;
uniform vec3  u_diffuse;
uniform float u_opacity;
uniform float u_specular;
uniform float u_shininess;

uniform vec4  u_clipPlanes[6];
uniform vec4  u_clipPlaneColors[6]; // Added for multi-color feedback
uniform int   u_clipPlanesActive[6];
uniform int   u_numClipPlanes;
uniform int   u_applyClipping;

// Section View FIDELITY
uniform int   u_showSectionOutline;
uniform vec4  u_sectionOutlineColor; // This becomes a 'fallback' if needed
uniform float u_sectionOutlineThickness;

uniform int   u_useSmoothShading;
uniform float u_exposure;
uniform float u_gamma;

out vec4 frag_color;

const float PI = 3.14159265359;

void main() {
    float outlineAlpha = 0.0;
    vec3  calculatedOutlineColor = u_sectionOutlineColor.rgb;

    if (u_applyClipping != 0) {
        for (int i = 0; i < u_numClipPlanes; ++i) {
            if (u_clipPlanesActive[i] != 0) {
                float d = dot(vec4(v_fragPos, 1.0), u_clipPlanes[i]);
                if (d < 0.0) discard;
                
                if (u_showSectionOutline != 0) {
                    float pW = fwidth(d);
                    float a = 1.0 - smoothstep(0.0, u_sectionOutlineThickness * pW, d);
                    if (a > outlineAlpha) {
                        outlineAlpha = a;
                        calculatedOutlineColor = u_clipPlaneColors[i].rgb;
                    }
                }
            }
        }
    }

    // 1. Precise Normal Calculation
    vec3 N;
    if (u_useSmoothShading != 0) {
        N = normalize(v_normal);
    } else {
        // Flat shading via screen-space derivatives (normalized for stability)
        vec3 dpdx = dFdx(v_fragPos);
        vec3 dpdy = dFdy(v_fragPos);
        N = normalize(cross(dpdx, dpdy));
    }

    // 2. View Vector calculation
    vec3 V = normalize(u_cameraPos - v_fragPos);

    // 3. Two-Sided Lighting Robustness (Flipping N if looking at backface)
    if (dot(N, V) < 0.0) {
        N = -N;
    }

    // 4. Lighting Model: PBR-lite CAD Standard
    vec3 keyDir   = normalize(vec3(0.6, 0.8, 0.8));
    vec3 keyColor = vec3(1.0, 0.98, 0.95) * 0.80;
    
    vec3 fillDir   = normalize(vec3(-0.8, -0.3, 0.4));
    vec3 fillColor = vec3(0.85, 0.9, 1.0) * 0.35;
    
    vec3 backDir   = normalize(vec3(0.2, 0.5, -1.0));
    vec3 backColor = vec3(1.0, 1.0, 1.0) * 0.25;

    // 5. Ambient Environment (Sky/Ground mix)
    vec3 skyColor    = vec3(0.4, 0.45, 0.55);
    vec3 groundColor = vec3(0.12, 0.12, 0.15);
    float upWeight   = max(0.0, dot(N, vec3(0, 1, 0)));
    vec3 ambient     = mix(groundColor, skyColor, upWeight) * 0.55;

    // 6. Diffuse Contributions
    float keyDiff  = max(dot(N, keyDir), 0.0);
    float fillDiff = max(dot(N, fillDir), 0.0);
    float backDiff = max(dot(N, backDir), 0.0);
    
    // HEADLIGHT: Camera-relative light source for visibility in holes/sections
    float headDiff = max(dot(N, V), 0.0); 
    vec3 headColor = vec3(1.0, 1.0, 1.0) * 0.45;

    vec3 totalDiffuse = (keyColor * keyDiff + fillColor * fillDiff + backColor * backDiff + headColor * headDiff);
    vec3 diffuseComponent = u_diffuse * totalDiffuse;

    // 7. Specular Component (standard Blinn-Phong normalization)
    float rough = clamp(1.0 - sqrt(max(u_shininess, 1.0) / 100.0), 0.15, 1.0);
    float specExp = exp2(10.0 * (1.0 - rough));
    float normFactor = (specExp + 2.0) / (8.0 * PI);
    
    vec3 H = normalize(keyDir + V);
    float NdotH = max(dot(N, H), 0.0);
    vec3 specularComponent = keyColor * pow(NdotH, specExp) * normFactor * u_specular * 0.4;

    // 8. Visual Highlights (Fresnel/Edge)
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    vec3 edgeHighlight = u_diffuse * fresnel * 0.3;

    // 9. Composing Final Color
    vec3 baseColor = (ambient * u_diffuse) + diffuseComponent + specularComponent + edgeHighlight;

    // Section Outline Mix
    vec3 finalColor = mix(baseColor, calculatedOutlineColor, outlineAlpha * u_sectionOutlineColor.a);

    // 10. Post-Processing (Exposure & Tone Mapping)
    finalColor *= u_exposure;
    finalColor = finalColor / (finalColor + vec3(1.0)); // simple Reinhard
    finalColor = pow(finalColor, vec3(1.0 / u_gamma));

    frag_color = vec4(finalColor, u_opacity);
}
)GLSL";
