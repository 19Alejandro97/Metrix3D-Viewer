#include "renderer.hpp"
#include "../core/scene_manager.hpp"
#include "../core/scene_object.hpp"
#include "../core/app_settings.hpp"
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

void Renderer::RenderBackground(const AppSettings& settings) {
    glClearColor(settings.viewportBackgroundBottom.r, settings.viewportBackgroundBottom.g, settings.viewportBackgroundBottom.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if (settings.useGradient) {
        glDisable(GL_DEPTH_TEST);
        backgroundShader->Use();
        backgroundShader->SetVec3("u_colorTop", settings.viewportBackgroundTop);
        backgroundShader->SetVec3("u_colorBot", settings.viewportBackgroundBottom);
        glBindVertexArray(bgVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
}

void Renderer::RenderScene(SceneManager& scene, const Camera& camera, const AppSettings& settings) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Core State Guard: CAD standard backface culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    bool anyClipEnabled = scene.AnyClipEnabled();
    auto& objects = scene.GetObjects();
    
    // Categorize
    std::vector<std::shared_ptr<SceneObject>> opaqueObjects;
    std::vector<std::shared_ptr<SceneObject>> transparentObjects;
    for (const auto& obj : objects) {
        if (!obj->isVisible || !obj->mesh) continue;
        if (obj->material.opacity >= 0.99f) opaqueObjects.push_back(obj);
        else                                transparentObjects.push_back(obj);
    }

    // Sort Transparent
    Vec3 camPos = camera.GetPosition();
    std::sort(transparentObjects.begin(), transparentObjects.end(), [&](const auto& a, const auto& b) {
        return glm::distance2(a->transform.position, camPos) > glm::distance2(b->transform.position, camPos);
    });

    // Pass: Opaque
    glDepthMask(GL_TRUE);
    for (const auto& obj : opaqueObjects) {
        DrawMeshObject(obj, *solidShader, camera, anyClipEnabled && obj->isActive, false, settings);
    }

    // Pass: Transparent
    glDepthMask(GL_FALSE);
    for (const auto& obj : transparentObjects) {
        DrawMeshObject(obj, *solidShader, camera, anyClipEnabled && obj->isActive, false, settings);
    }
    glDepthMask(GL_TRUE);

    // Pass: Edges (Avoid Z-fighting)
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-2.0f, -2.0f);
    glDepthFunc(GL_LEQUAL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    for (const auto& obj : objects) {
        if (obj->isVisible && obj->mesh && obj->material.showEdges && obj->mesh->m_gpu.edgeVao != 0) {
            DrawMeshObject(obj, *solidShader, camera, anyClipEnabled && obj->isActive, true, settings);
        }
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDepthFunc(GL_LESS);
}

void Renderer::DrawMeshObject(const std::shared_ptr<SceneObject>& obj, Shader& shader, const Camera& camera, bool applyClipping, bool isEdgeOverlay, const AppSettings& settings) {
    shader.Use();

    // 1. Lighting & Exposure Persistence
    shader.SetMat4("u_view", camera.viewMatrix);
    shader.SetMat4("u_projection", camera.projMatrix);
    shader.SetVec3("u_cameraPos", camera.GetPosition());
    shader.SetFloat("u_exposure", settings.exposure);
    shader.SetFloat("u_gamma",    settings.gamma);

    // 2. Clipping Synchronization
    shader.SetInt("u_applyClipping", applyClipping ? 1 : 0);
    shader.SetInt("u_numClipPlanes", 6);
    
    int activeMask[6] = {0,0,0,0,0,0};
    glm::vec4 equations[6] = {glm::vec4(0), glm::vec4(0), glm::vec4(0), glm::vec4(0), glm::vec4(0), glm::vec4(0)};
    glm::vec4 colors[6] = {glm::vec4(0), glm::vec4(0), glm::vec4(0), glm::vec4(0), glm::vec4(0), glm::vec4(0)};

    if (applyClipping && obj->scene) {
        auto selectedObj = obj->scene->GetSelectedObject(); 
        Mat4f ucsModel = selectedObj ? Mat4f(selectedObj->transform.worldMatrix) : Mat4f(1.0f);
        glm::mat3 ucsNormalMat = glm::transpose(glm::inverse(glm::mat3(ucsModel)));
        
        auto& clipPlanes = obj->scene->GetClipPlanes();
        for (int i = 0; i < 6; ++i) {
            activeMask[i] = clipPlanes[i].active ? 1 : 0;
            colors[i] = glm::vec4(clipPlanes[i].color, 1.0f);
            if (clipPlanes[i].active) {
                glm::vec4 lEq = clipPlanes[i].GetPlaneEquation();
                glm::vec3 nW = glm::normalize(ucsNormalMat * glm::vec3(lEq));
                glm::vec3 oW = glm::vec3(ucsModel * glm::vec4(-glm::vec3(lEq) * lEq.w, 1.0f));
                equations[i] = glm::vec4(nW, -glm::dot(nW, oW));
            }
        }
        shader.SetInt("u_showSectionOutline", settings.showSectionOutline ? 1 : 0);
        shader.SetVec4("u_sectionOutlineColor", settings.sectionOutlineColor);
        shader.SetFloat("u_sectionOutlineThickness", settings.sectionOutlineThickness);
    }
    glUniform4fv(glGetUniformLocation(shader.id, "u_clipPlanes"), 6, glm::value_ptr(equations[0]));
    glUniform4fv(glGetUniformLocation(shader.id, "u_clipPlaneColors"), 6, glm::value_ptr(colors[0]));
    glUniform1iv(glGetUniformLocation(shader.id, "u_clipPlanesActive"), 6, activeMask);

    // 3. Normal Matrix & Transformations
    shader.SetInt("u_useSmoothShading", (isEdgeOverlay ? 1 : (settings.useSmoothShading ? 1 : 0)));
    Mat4f model = Mat4f(obj->transform.worldMatrix);
    shader.SetMat4("u_model", model);
    shader.SetMat3("u_normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));

    if (isEdgeOverlay) {
        shader.SetVec3("u_diffuse", glm::vec3(0.0f));
        shader.SetFloat("u_opacity", obj->material.opacity);
        glBindVertexArray(obj->mesh->m_gpu.edgeVao);
        glDrawElements(GL_LINES, (GLsizei)obj->mesh->edgeIndices.size(), GL_UNSIGNED_INT, 0);
    } else {
        shader.SetVec3("u_diffuse", obj->material.diffuseColor);
        shader.SetFloat("u_opacity", obj->material.opacity);
        shader.SetFloat("u_specular", obj->material.specular);
        shader.SetFloat("u_shininess", obj->material.shininess);
        
        if      (obj->material.renderMode == RenderMode::Wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else if (obj->material.renderMode == RenderMode::Points)    { glPolygonMode(GL_FRONT_AND_BACK, GL_POINT); glPointSize(2.0f); }
        else                                                         glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glBindVertexArray(obj->mesh->m_gpu.vao);
        glDrawElements(GL_TRIANGLES, (GLsizei)obj->mesh->indices.size(), GL_UNSIGNED_INT, 0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glPointSize(1.0f);
    }
    glBindVertexArray(0);
}

void Renderer::RenderOverlays(SceneManager& scene, const Camera& camera, const AppSettings& settings) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL); 
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -2.0f);
    glEnable(GL_BLEND);

    capShader->Use();
    capShader->SetMat4("u_view", camera.viewMatrix);
    capShader->SetMat4("u_projection", camera.projMatrix);
    capShader->SetInt("u_unlit", 1); 
    capShader->SetInt("u_applyClipping", 0);

    for (const auto& obj : scene.GetObjects()) {
        if (!obj->isVisible || !obj->mesh) continue;
        capShader->SetMat4("u_model", Mat4f(obj->transform.worldMatrix));
        capShader->SetVec4("u_color", settings.measurementColor);

        if (!obj->mesh->highlightEdgeIndices.empty()) {
            glLineWidth(3.0f);
            glBindVertexArray(obj->mesh->m_gpu.highlightEdgeVao);
            glDrawElements(GL_LINES, (GLsizei)obj->mesh->highlightEdgeIndices.size(), GL_UNSIGNED_INT, 0);
        }
        if (!obj->mesh->measuredEdgeIndices.empty()) {
            glLineWidth(settings.measurementThickness);
            glBindVertexArray(obj->mesh->m_gpu.measuredEdgeVao);
            glDrawElements(GL_LINES, (GLsizei)obj->mesh->measuredEdgeIndices.size(), GL_UNSIGNED_INT, 0);
        }
    }
    glLineWidth(1.0f);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthFunc(GL_LESS);
}

void Renderer::RenderSectionCaps(SceneManager& scene, const Camera& camera, const AppSettings& settings) {
    if (!scene.AnyClipEnabled()) return;

    auto& clipPlanes = scene.GetClipPlanes();
    auto selectedObj = scene.GetSelectedObject();
    Mat4f ucsModel = selectedObj ? Mat4f(selectedObj->transform.worldMatrix) : Mat4f(1.0f);
    glm::mat3 ucsNormalMat = glm::transpose(glm::inverse(glm::mat3(ucsModel)));

    // Extract World Equations for sections
    glm::vec4 worldEquations[6];
    int worldActiveMask[6];
    for (int i = 0; i < 6; ++i) {
        worldActiveMask[i] = clipPlanes[i].active ? 1 : 0;
        if (clipPlanes[i].active) {
            glm::vec4 lEq = clipPlanes[i].GetPlaneEquation();
            glm::vec3 nW = glm::normalize(ucsNormalMat * glm::vec3(lEq));
            glm::vec3 oW = glm::vec3(ucsModel * glm::vec4(-glm::vec3(lEq) * lEq.w, 1.0f));
            worldEquations[i] = glm::vec4(nW, -glm::dot(nW, oW));
        } else worldEquations[i] = glm::vec4(0);
    }

    // MULTI-PLANE PARITY STENCIL ALGORITHM
    for (int i = 0; i < 6; ++i) {
        if (!clipPlanes[i].active) continue;

        // Clear stencil for this specific plane evaluation
        glClear(GL_STENCIL_BUFFER_BIT);

        for (const auto& obj : scene.GetObjects()) {
            if (!obj->isVisible || !obj->mesh || !obj->isActive) continue;

            // Step 1: Mask Pass (Parity counting)
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS, 0, 0xFF);
            
            // MASK-ONLY mode: Disable color and depth writing
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE); // Crucial: Count both front and back faces

            // Increment on front, Decrement on back (Relative to scene depth)
            glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
            glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);

            // Draw object with clipping enabled (only count fragments that pass the clipping planes)
            DrawMeshObject(obj, *solidShader, camera, true, false, settings);

            // Step 2: Cap Pass (Solid Capping)
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LEQUAL); // Avoid flickering Z-fighting
            
            // Only draw where parity != 0
            glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
            // REPLACE ref with 0 to clear stencil as we draw (Advanced clean-up)
            glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);

            capShader->Use();
            capShader->SetMat4("u_view", camera.viewMatrix);
            capShader->SetMat4("u_projection", camera.projMatrix);
            capShader->SetVec3("u_cameraPos", camera.GetPosition());
            
            // Heredar color original de la pieza
            capShader->SetVec4("u_color", glm::vec4(obj->material.diffuseColor, 1.0f)); 
            
            // Tintado de eje (15% del color del plano para identificar X/Y/Z)
            capShader->SetVec4("u_planeColor", glm::vec4(clipPlanes[i].color, 1.0f));

            capShader->SetInt("u_applyClipping", 1);
            capShader->SetInt("u_numClipPlanes", 6);
            
            int capActiveMask[6];
            memcpy(capActiveMask, worldActiveMask, sizeof(worldActiveMask));
            capActiveMask[i] = 0; // The cap quad itself is NOT clipped by its own plane

            glUniform4fv(glGetUniformLocation(capShader->id, "u_clipPlanes"), 6, glm::value_ptr(worldEquations[0]));
            glUniform1iv(glGetUniformLocation(capShader->id, "u_clipPlanesActive"), 6, capActiveMask);

            glm::vec3 nW = glm::vec3(worldEquations[i]);
            float dW = worldEquations[i].w;
            UpdateCapQuad(nW, dW);

            capShader->SetMat4("u_model", glm::mat4(1.0f));
            capShader->SetVec3("u_normal", nW);

            glBindVertexArray(capVao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            // Reset States
            glDepthFunc(GL_LESS);
            glDisable(GL_STENCIL_TEST);
        }
    }
    
    // Core Reset
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

void Renderer::RenderPlaneGizmos(SceneManager& scene, const Camera& camera, const AppSettings& settings) {
    if (settings.activeGizmoPlane < 0 || settings.activeGizmoPlane >= 6) return;

    auto& clipPlanes = scene.GetClipPlanes();
    auto& p = clipPlanes[settings.activeGizmoPlane];
    if (!p.active) return;

    auto selectedObj = scene.GetSelectedObject();
    Mat4f ucsModel = selectedObj ? Mat4f(selectedObj->transform.worldMatrix) : Mat4f(1.0f);
    glm::mat3 ucsNormalMat = glm::transpose(glm::inverse(glm::mat3(ucsModel)));

    glm::vec4 lEq = p.GetPlaneEquation();
    glm::vec3 nW = glm::normalize(ucsNormalMat * glm::vec3(lEq));
    glm::vec3 oW = glm::vec3(ucsModel * glm::vec4(-glm::vec3(lEq) * lEq.w, 1.0f));
    glm::vec4 worldEq = glm::vec4(nW, -glm::dot(nW, oW));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE); // No depth writing for ghost plane
    
    capShader->Use();
    capShader->SetMat4("u_view", camera.viewMatrix);
    capShader->SetMat4("u_projection", camera.projMatrix);
    capShader->SetMat4("u_model", glm::mat4(1.0f));
    capShader->SetVec3("u_normal", nW);
    capShader->SetVec4("u_color", glm::vec4(p.color, 0.35f)); // 35% opacity ghost (High Vis)
    capShader->SetInt("u_unlit", 1);
    capShader->SetInt("u_applyClipping", 0);

    UpdateCapQuad(nW, worldEq.w);
    
    glBindVertexArray(capVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
}

void Renderer::UpdateCapQuad(glm::vec3 n, float d) {
    if (glm::length(n) < 0.1f) return;
    glm::vec3 p0 = -n * d;
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(n, up)) > 0.99f) up = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(n, up));
    glm::vec3 forward = glm::cross(right, n);
    float size = 10000.0f;
    glm::vec3 p1 = p0 - right * size - forward * size;
    glm::vec3 p2 = p0 + right * size - forward * size;
    glm::vec3 p3 = p0 + right * size + forward * size;
    glm::vec3 p4 = p0 - right * size + forward * size;
    float v[] = { p1.x, p1.y, p1.z, p2.x, p2.y, p2.z, p3.x, p3.y, p3.z, p1.x, p1.y, p1.z, p3.x, p3.y, p3.z, p4.x, p4.y, p4.z };
    glBindVertexArray(capVao);
    glBindBuffer(GL_ARRAY_BUFFER, capVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}
