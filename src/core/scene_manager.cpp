#include "scene_manager.hpp"
#include <algorithm>
#include <iostream>
#include "app_settings.hpp"
#include "material.hpp"

SceneManager::SceneManager() {
    selectedObject = nullptr;
    
    // Initialize default clipping planes
    clipPlanes.resize(6);
    clipPlanes[0] = {false, false, 0, false, {0.8f, 0.2f, 0.2f}, 0.0f, 0.1f}; // X
    clipPlanes[1] = {false, false, 1, false, {0.2f, 0.8f, 0.2f}, 0.0f, 0.1f}; // Y
    clipPlanes[2] = {false, false, 2, false, {0.2f, 0.2f, 0.8f}, 0.0f, 0.1f}; // Z
    clipPlanes[3] = {false, false, 0, false, {0.8f, 0.2f, 0.8f}, 0.0f, 0.1f}; // X
    clipPlanes[4] = {false, false, 1, false, {0.8f, 0.5f, 0.0f}, 0.0f, 0.1f}; // Y
    clipPlanes[5] = {false, false, 2, false, {0.0f, 0.8f, 0.8f}, 0.0f, 0.1f}; // Z
}

SceneManager::~SceneManager() {
    std::cout << "[Shutdown] SceneManager destructor started." << std::endl;
    ClearScene();
    std::cout << "[Shutdown] SceneManager destructor finished." << std::endl;
}

void SceneManager::AddObject(std::shared_ptr<SceneObject> obj) {
    if (obj) {
        // Auto-assign color from premium palette based on count
        auto& palette = AppSettings::Instance().cadPalette;
        if (!palette.empty()) {
            size_t idx = objects.size() % palette.size();
            obj->material.diffuseColor = Vec3f(palette[idx]);
        }
        obj->scene = this; // Set back-pointer
        objects.push_back(obj);
    }
}

void SceneManager::RemoveObject(const std::string& id) {
    auto it = std::remove_if(objects.begin(), objects.end(),
        [&](const std::shared_ptr<SceneObject>& o) { return o->id == id; });
    
    if (it != objects.end()) {
        if (selectedObject && selectedObject->id == id) {
            selectedObject = nullptr;
        }
        objects.erase(it, objects.end());
    }
}

void SceneManager::ClearScene() {
    objects.clear();
    selectedObject = nullptr;
}

void SceneManager::SelectObject(const std::string& id, bool multi) {
    if (!multi) {
        for (auto& o : objects) {
            o->isSelected = false;
            o->isActive = false;
        }
    }

    for (auto& o : objects) {
        if (o->id == id) {
            o->isSelected = true;
            o->isActive = true;
            selectedObject = o;
            break;
        }
    }
}

void SceneManager::DeselectAll() {
    for (auto& o : objects) {
        o->isSelected = false;
        o->isActive = false;
    }
    selectedObject = nullptr;
}

bool SceneManager::AnyClipEnabled() const {
    for (const auto& p : clipPlanes) {
        if (p.active) return true;
    }
    return false;
}
