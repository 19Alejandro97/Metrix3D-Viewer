#include "renderer.hpp"
#include "../core/app_settings.hpp"
#include "shaders/solid_shader.hpp"
#include "shaders/cap_shader.hpp"
#include "shaders/bg_shader.hpp"
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

Renderer::Renderer() {
    SetupFBO();
    BuildShaders();

    // Gen Cap Buffer
    glGenVertexArrays(1, &capVao);
    glGenBuffers(1, &capVbo);

    // Gen Background Quad
    float bgVerts[] = { -1, -1,  1, -1,  1,  1, -1, -1,  1,  1, -1,  1 };
    glGenVertexArrays(1, &bgVao);
    glGenBuffers(1, &bgVbo);
    glBindVertexArray(bgVao);
    glBindBuffer(GL_ARRAY_BUFFER, bgVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bgVerts), bgVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

Renderer::~Renderer() {
    glDeleteFramebuffers(1, &multiFbo);
    glDeleteTextures(1, &multiColorTex);
    glDeleteRenderbuffers(1, &multiRboDepthTemplate);
    
    glDeleteFramebuffers(1, &resolveFbo);
    glDeleteTextures(1, &resolveFboTexture);
    
    glDeleteVertexArrays(1, &capVao);
    glDeleteBuffers(1, &capVbo);

    glDeleteVertexArrays(1, &bgVao);
    glDeleteBuffers(1, &bgVbo);
}

void Renderer::SetupFBO() {
    if (multiFbo) {
        glDeleteFramebuffers(1, &multiFbo);
        glDeleteTextures(1, &multiColorTex);
        glDeleteRenderbuffers(1, &multiRboDepthTemplate);
        multiFbo = 0;
    }
    if (resolveFbo) {
        glDeleteFramebuffers(1, &resolveFbo);
        glDeleteTextures(1, &resolveFboTexture);
        resolveFbo = 0;
    }

    glGenFramebuffers(1, &multiFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, multiFbo);

    glGenTextures(1, &multiColorTex);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, multiColorTex);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, currMsaaSamples, GL_RGBA8, fboWidth, fboHeight, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, multiColorTex, 0);

    glGenRenderbuffers(1, &multiRboDepthTemplate);
    glBindRenderbuffer(GL_RENDERBUFFER, multiRboDepthTemplate);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, currMsaaSamples, GL_DEPTH24_STENCIL8, fboWidth, fboHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, multiRboDepthTemplate);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR::FRAMEBUFFER:: Multi FBO is not complete!" << std::endl;
    }

    glGenFramebuffers(1, &resolveFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFbo);

    glGenTextures(1, &resolveFboTexture);
    glBindTexture(GL_TEXTURE_2D, resolveFboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fboWidth, fboHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolveFboTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR::FRAMEBUFFER:: Resolve FBO is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::ResizeFBO(int width, int height) {
    if (width != fboWidth || height != fboHeight) {
        fboWidth = width;
        fboHeight = height;
        SetupFBO();
    }
}

void Renderer::BuildShaders() {
    solidShader = std::make_unique<Shader>(SOLID_VERT_GLSL, SOLID_FRAG_GLSL);
    capShader   = std::make_unique<Shader>(CAP_VERT_GLSL, CAP_FRAG_GLSL);
    backgroundShader = std::make_unique<Shader>(BG_VERT_GLSL, BG_FRAG_GLSL);
}

void Renderer::Render(SceneManager& scene, const Camera& camera) {
    if (fboWidth <= 0 || fboHeight <= 0) return;
    auto& settings = AppSettings::Instance();

    if (settings.msaaSamples != currMsaaSamples) {
        currMsaaSamples = settings.msaaSamples;
        SetupFBO();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, multiFbo);
    glViewport(0, 0, fboWidth, fboHeight);

    // Pass 1: Background
    RenderBackground(settings);

    // Pass 2: Main Scene (Opaque & Transparent)
    RenderScene(scene, camera, settings);

    // Pass 3: Section Caps (Solid Taponado)
    RenderSectionCaps(scene, camera, settings);

    // Pass 4: Overlays (Highlights, Measurements, UCS)
    RenderOverlays(scene, camera, settings);

    // Pass 5: UI Interaction Gizmos (Ghost Planes)
    RenderPlaneGizmos(scene, camera, settings);

    // Resolve & MSAA Blit
    ResolveFinalImage();
}

void Renderer::ResolveFinalImage() {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, multiFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo);
    glBlitFramebuffer(0, 0, fboWidth, fboHeight, 
                      0, 0, fboWidth, fboHeight, 
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
