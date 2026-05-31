#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo/ImGuizmo.h"

#include "core/scene_object.hpp"
#include "core/camera.hpp"
#include "core/settings_io.hpp"
#include "core/scene_manager.hpp"
#include "core/math_utils.hpp"
#include "render/renderer.hpp"
#include "ui/ui_manager.hpp"

#include <iostream>
#include <vector>
#include <memory>
#include <filesystem>

// --- Global State for Callbacks ---
Camera*       g_camera  = nullptr;
UIManager*    g_ui      = nullptr;
SceneManager* g_scene   = nullptr;
bool   g_leftMouseDown  = false;
bool   g_rightMouseDown = false;
double g_lastMouseX     = 0.0;
double g_lastMouseY     = 0.0;

// Callback declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_button_callback    (GLFWwindow* window, int button, int action, int mods);
void cursor_position_callback (GLFWwindow* window, double xpos, double ypos);
void scroll_callback          (GLFWwindow* window, double xoffset, double yoffset);
void drop_callback            (GLFWwindow* window, int count, const char** paths);

int main() {
    // 1. Initialise GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // OpenGL 4.5 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Antialiasing MSAAx4 (Phase 6 Polish)
    glfwWindowHint(GLFW_SAMPLES, 4);

    // 2. Create Window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Metrix3D", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable V-Sync (Zero tearing)

    // Set GLFW Callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback    (window, mouse_button_callback);
    glfwSetCursorPosCallback      (window, cursor_position_callback);
    glfwSetScrollCallback         (window, scroll_callback);
    glfwSetDropCallback           (window, drop_callback); // Drag & Drop

    // 3. GLAD Load
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
    // Enable MSAA in OpenGL state
    glEnable(GL_MULTISAMPLE);
    
    // Print technical stack verification
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    // 4. Initialise ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    io.ConfigWindowsMoveFromTitleBarOnly = true;                // Prevent dragging floating windows by clicking on empty space / viewport

    // Setup ImGui Backend
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450 core");

    // Load Fonts - Merge FontAwesome 5 Solid
    {
        ImFontConfig config;
        config.MergeMode = true; 
        config.PixelSnapH = true;
        config.GlyphMinAdvanceX = 13.0f; // Use if icons feel too close
        
        static const ImWchar icon_ranges[] = { 0xf000, 0xf9ff, 0 }; // FontAwesome 5 Solid range
        
        io.Fonts->AddFontDefault(); // Load default first
        
        // Try to load the font file
        if (std::filesystem::exists("assets/fa-solid-900.ttf")) {
            io.Fonts->AddFontFromFileTTF("assets/fa-solid-900.ttf", 13.0f, &config, icon_ranges);
        } else {
            std::cerr << "Warning: fa-solid-900.ttf not found in assets/" << std::endl;
        }
        
        // Build atlas
        unsigned int atlasWidth, atlasHeight;
        unsigned char* pixels;
        io.Fonts->GetTexDataAsRGBA32(&pixels, (int*)&atlasWidth, (int*)&atlasHeight);
    }

    // 5. Initialise Metrix3D subsystems
    SceneManager scene;
    UIManager uiManager;   // Applies dark theme in constructor
    Renderer renderer;
    Camera camera;

    g_camera = &camera;
    g_ui     = &uiManager;
    g_scene  = &scene;

    // TODO: Just to test the renderer, we could generate a triangle or cube manually,
    // but we will wait for the user to select an STL to import via ImGui.

    // -----------------------------------------------------------------------
    // Initialize Settings
    // -----------------------------------------------------------------------
    std::string settingsPath = "settings.json";
    if (const char* env_p = std::getenv("APPDATA")) {
        std::error_code ec;
        std::filesystem::path dir = std::filesystem::path(env_p) / "Metrix3D";
        std::filesystem::create_directories(dir, ec);
        settingsPath = (dir / "settings.json").string();
    }
    SettingsIO::Load(settingsPath);

    // 6. Main Loop
    while (!glfwWindowShouldClose(window)) {
        // --- INPUT ---
        glfwPollEvents();

        // --- UPDATE ---
        ImVec2 vpSize = uiManager.GetViewportSize();
        camera.Update((int)vpSize.x, (int)vpSize.y);
        renderer.ResizeFBO((int)vpSize.x, (int)vpSize.y);

        // --- RENDER 3D (to FBO) ---
        renderer.Render(scene, camera);

        // --- RENDER UI (to Screen) ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        
        uiManager.DrawUI(scene, camera, renderer.GetColorTextureID());

        ImGui::Render();
        
        // Clear background of the main OS window
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render ImGui draw data
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows (Multi-Viewport feature)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        // --- SWAP BUFFERS ---
        glfwSwapBuffers(window);
    }

    std::cout << "[Shutdown] Saving settings..." << std::endl;
    // Save settings before exit
    SettingsIO::Save(settingsPath);

    std::cout << "[Shutdown] ImGui/OpenGL Cleanup..." << std::endl;
    // 7. Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    std::cout << "[Shutdown] GLFW Destroy Window..." << std::endl;
    glfwDestroyWindow(window);
    
    std::cout << "[Shutdown] GLFW Terminate..." << std::endl;
    glfwTerminate();

    std::cout << "[Shutdown] Reached end of main(). Forcing std::exit(0)." << std::endl;
    std::exit(0);

    return 0;
}


// --- Callbacks Implementation ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// --- Helper: Raycast under cursor ---
bool GetRaycastHitUnderCursor(GLFWwindow* window, Vec3& outHitPt) {
    if (!g_ui || !g_camera || !g_scene) return false;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    ImVec2 vpPos  = g_ui->GetViewportPos();
    ImVec2 vpSize = g_ui->GetViewportSize();

    float lx = (float)mx - vpPos.x;
    float ly = (float)my - vpPos.y;

    if (lx < 0 || ly < 0 || lx >= vpSize.x || ly >= vpSize.y) return false;

    Ray ray = MathUtils::ScreenToWorldRay(
        glm::vec2(lx, ly),
        glm::vec2(vpSize.x, vpSize.y),
        g_camera->viewMatrix, g_camera->projMatrix);

    float minT = 1e9f;
    bool  hit  = false;

    for (auto& obj : g_scene->GetObjects()) {
        if (!obj->isVisible) continue;
        RaycastHit h;
        if (obj->RaycastHitTriangle(ray, h) && (float)h.distance < minT) {
            minT     = (float)h.distance;
            outHitPt = h.point;
            hit      = true;
        }
    }
    return hit;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        g_leftMouseDown = (action == GLFW_PRESS);
        if (g_leftMouseDown && g_ui && !g_ui->IsInteracting() && g_camera) {
            Vec3 hitPt;
            if (GetRaycastHitUnderCursor(window, hitPt)) {
                g_camera->SetOrbitPivot(hitPt);
            } else {
                // g_camera->SetOrbitPivot(hitPt);
            }
        } else if (!g_leftMouseDown && g_camera) {
            // g_camera->ClearOrbitPivot();
        }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        g_rightMouseDown = (action == GLFW_PRESS);
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    double deltaX = xpos - g_lastMouseX;
    double deltaY = ypos - g_lastMouseY;
    g_lastMouseX = xpos;
    g_lastMouseY = ypos;

    // Only move camera if UI is NOT blocking mouse events (so we don't rotate while dragging sliders)
    /*
    if (g_ui && !g_ui->IsInteracting() && g_camera) {
        if (g_leftMouseDown) {
            g_camera->Orbit(deltaX, deltaY);
        } else if (g_rightMouseDown) {
            g_camera->Pan(deltaX, deltaY);
        }
    }
    */
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (!g_ui || !g_camera) return;
    if (g_ui->IsInteracting()) return;

    // --- Zoom-to-cursor: raycast under the mouse pointer ---
    Vec3 hitPt;
    /*
    if (GetRaycastHitUnderCursor(window, hitPt)) {
        g_camera->ZoomTowardPoint(hitPt, yoffset);
        return;
    }
    g_camera->Zoom(yoffset);
    */
}

void drop_callback(GLFWwindow* window, int count, const char** paths) {
    if (!g_ui || !g_scene || !g_camera) return;
    for (int i = 0; i < count; ++i) {
        std::string p(paths[i]);
        std::string ext = std::filesystem::path(p).extension().string();
        for (auto& c : ext) c = (char)tolower(c);
        if (ext == ".stl" || ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".ply") {
            g_ui->ImportFromPath(*g_scene, *g_camera, p);
        }
    }
}
