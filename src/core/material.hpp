#pragma once
#include "math_types.hpp"

enum class RenderMode {
    Solid,
    Wireframe,
    Points
};

struct Material {
    // Generic neutral high-contrast color for technical surfaces
    Vec3f diffuseColor  = {0.80f, 0.80f, 0.80f}; 
    float opacity       = 1.0f;
    float specular      = 0.5f;
    float shininess     = 32.0f;
    
    RenderMode renderMode = RenderMode::Solid;
    
    // Technical display features
    bool  showEdges     = true; 
    float edgeThreshold = 35.0f; // Angle in degrees to show the edge
};
