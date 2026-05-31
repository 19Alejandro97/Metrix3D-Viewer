#pragma once
#include "shader.hpp"
#include "../core/camera.hpp"
#include <memory>
#include <vector>
#include <glm/glm.hpp>

class SceneManager;
class SceneObject;
class AppSettings;

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Resizes the internal framebuffer if the ImGui Viewport panel changed size
    void ResizeFBO(int width, int height);

    // Renders the scene into the FBO
    void Render(SceneManager& scene, const Camera& camera);

    // Returns the texture drawn this frame (usable by ImGui::Image)
    unsigned int GetColorTextureID() const { return resolveFboTexture; }

private:
    void SetupFBO();
    void BuildShaders();

    // --- Internal Rendering Pipeline (Decomposed in renderer_passes.cpp) ---
    void RenderBackground(const AppSettings& settings);
    void RenderScene(SceneManager& scene, const Camera& camera, const AppSettings& settings);
    void RenderOverlays(SceneManager& scene, const Camera& camera, const AppSettings& settings);
    void RenderSectionCaps(SceneManager& scene, const Camera& camera, const AppSettings& settings);
    void RenderPlaneGizmos(SceneManager& scene, const Camera& camera, const AppSettings& settings);
    void ResolveFinalImage();

    // Helper to draw a single mesh with clipping/styling
    void DrawMeshObject(const std::shared_ptr<SceneObject>& obj, 
                       Shader& shader,
                       const Camera& camera,
                       bool applyClipping, 
                       bool isEdgeOverlay,
                       const AppSettings& settings);

    // Multisample Framebuffer (Render Target)
    int fboWidth = 800, fboHeight = 600;
    int currMsaaSamples = 4;
    unsigned int multiFbo = 0;
    unsigned int multiColorTex = 0;
    unsigned int multiRboDepthTemplate = 0;

    // Resolve Framebuffer (For ImGui texture)
    unsigned int resolveFbo = 0;
    unsigned int resolveFboTexture = 0;
    
    // Buffer handles for capping quad
    unsigned int capVao = 0;
    unsigned int capVbo = 0;
    void UpdateCapQuad(glm::vec3 n, float d);

    // Shaders
    std::unique_ptr<Shader> solidShader;
    std::unique_ptr<Shader> capShader;
    std::unique_ptr<Shader> backgroundShader;

    // Background Quad
    unsigned int bgVao = 0;
    unsigned int bgVbo = 0;
};
