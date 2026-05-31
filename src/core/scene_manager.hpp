#pragma once
#include <vector>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include "scene_object.hpp"

struct SectionPlane {
    bool active = false;
    bool isLocal = false;
    int type = 0; // 0=X, 1=Y, 2=Z, 3=Custom
    bool flip = false;
    glm::vec3 color = {0.8f, 0.2f, 0.2f};
    float position = 0.0f;
    float step = 0.1f;

    glm::vec4 GetPlaneEquation() const {
        glm::vec3 normal(0.0f);
        if (type == 0) normal = {1, 0, 0};
        else if (type == 1) normal = {0, 1, 0};
        else if (type == 2) normal = {0, 0, 1};
        else normal = {1, 1, 1};

        if (flip) normal = -normal;
        return glm::vec4(normal, -position);
    }
};

class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    bool isDirty = true; // Global dirty flag for scene-wide invalidation

    // Object Management
    void AddObject(std::shared_ptr<SceneObject> obj);
    void RemoveObject(const std::string& id);
    void ClearScene();

    const std::vector<std::shared_ptr<SceneObject>>& GetObjects() const { return objects; }
    std::vector<std::shared_ptr<SceneObject>>& GetObjects() { return objects; }

    SceneObject* GetObjectById(const std::string& id) {
        for (auto& obj : objects) if (obj->id == id) return obj.get();
        return nullptr;
    }

    // Selection
    void SelectObject(const std::string& id, bool multi = false);
    void DeselectAll();
    void SetSelectedObject(std::shared_ptr<SceneObject> obj) { selectedObject = obj; }
    std::shared_ptr<SceneObject> GetSelectedObject() const { return selectedObject; }

    // Clipping Planes
    std::vector<SectionPlane>& GetClipPlanes() { return clipPlanes; }
    const std::vector<SectionPlane>& GetClipPlanes() const { return clipPlanes; }
    bool AnyClipEnabled() const;

private:
    std::vector<std::shared_ptr<SceneObject>> objects;
    std::shared_ptr<SceneObject> selectedObject;
    std::vector<SectionPlane> clipPlanes;
};
