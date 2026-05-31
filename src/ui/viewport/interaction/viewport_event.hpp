#pragma once
#include "../../../core/math_utils.hpp"
#include <imgui.h>

struct ViewportEvent {
    ImVec2 mousePos;
    ImVec2 localMouse;
    ImVec2 viewportPos;
    ImVec2 viewportSize;
    Ray    ray;
    float  mouseWheel;
    ImVec2 mouseDelta;
    bool   isHovered;
    bool   isLeftClicked;
    bool   isLeftDragging;
    bool   isRightDragging;
    bool   isMiddleDown;
    bool   shiftHeld;
    bool   ctrlHeld;

    ViewportEvent() : ray(Vec3(0), Vec3(0,0,1)) {}
};
