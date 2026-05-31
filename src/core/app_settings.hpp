#pragma once
#include <glm/glm.hpp>
#include <vector>

// Global visual configuration for the application
// Persisted in imgui.ini implicitly through the Settings panel
struct AppSettings {
    static AppSettings& Instance() {
        static AppSettings inst;
        return inst;
    }

    void ResetToDefaults() {
        *this = AppSettings();
    }

    // Entity selection highlight colors (RGBA)
    glm::vec4 colorEntityA = {1.0f, 0.55f, 0.0f, 1.0f};  // Orange
    glm::vec4 colorEntityB = {0.0f, 0.85f, 1.0f, 1.0f};  // Cyan
    glm::vec4 colorNormal  = {0.2f, 1.0f,  0.4f, 1.0f};  // Green for normal arrows
    
    // Measurement Config
    glm::vec4 measurementColor = {1.0f, 0.5f, 0.0f, 1.0f}; // Intense Orange
    float     measurementThickness = 2.0f;

    // Background color for the 3D viewport
    bool      useGradient            = true;
    glm::vec3 viewportBackgroundBottom = {0.10f, 0.10f, 0.12f};
    glm::vec3 viewportBackgroundTop    = {0.25f, 0.28f, 0.35f};

    // Rendering Quality Settings
    enum class RenderQuality { Performance, Balanced, Quality, Ultra };
    RenderQuality qualityPreset = RenderQuality::Balanced;

    int       msaaSamples   = 4;      // 1, 2, 4, 8
    bool      useSmoothShading = true; // false = Flat shading via derivatives
    float     exposure      = 1.0f;
    float     gamma         = 2.2f;

    // Unit System
    enum class UnitSystem { MM, CM, Inch };
    UnitSystem currentUnit = UnitSystem::MM;

    const char* GetUnitString() const {
        switch (currentUnit) {
            case UnitSystem::MM: return "mm";
            case UnitSystem::CM: return "cm";
            case UnitSystem::Inch: return "in";
            default: return "??";
        }
    }

    bool showSettingsWindow = false;
    bool showConsole        = false;
    bool preserveCoordinatesOnImport = true;
    bool continuousMeasurement = false;
    bool instantViewJump       = false; // Default to smooth transition (SLERP)

    bool      showSectionOutline      = true;
    glm::vec4 sectionOutlineColor     = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f); // Cyan Emissive
    float     sectionOutlineThickness = 4.5f; // Pixels
    int       activeGizmoPlane = -1; // -1 = None, 0-5 = index of plane being manipulated

    // Projection Mode
    bool      isOrthographic = false;
    float     fovY           = 30.0f;

    // A curated "Premium" palette for CAD parts
    std::vector<glm::vec3> cadPalette = {
        {0.25f, 0.45f, 0.55f}, // CAD Teal
        {0.85f, 0.45f, 0.15f}, // Industrial Orange
        {0.35f, 0.35f, 0.55f}, // Blue Metal
        {0.45f, 0.55f, 0.25f}, // Sage Green
        {0.65f, 0.65f, 0.65f}, // Aluminium
        {0.75f, 0.25f, 0.25f}, // Dark Red
        {0.25f, 0.65f, 0.75f}, // Sky Blue
        {0.45f, 0.25f, 0.45f}  // Purple
    };

private:
    AppSettings() = default;
};
