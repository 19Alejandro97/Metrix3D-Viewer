#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <imgui.h>

#include <functional>
#include <mutex>

class AppConsole {
public:
    static AppConsole& Instance() {
        static AppConsole inst;
        return inst;
    }
    struct LogEntry {
        std::string message;
        ImVec4 color;
        bool hasCustomColor = false;
    };

    static void Log(const std::string& msg, const ImVec4& color = ImVec4(0,0,0,0)) {
        Instance().AddLogInternal(msg, color);
    }

    void Clear() {
        entries.clear();
    }

    void Draw() {
        if (ImGui::Button("Clear")) Clear();
        ImGui::SameLine();
        
        // Copy entries under lock to avoid blocking other threads while drawing
        std::vector<LogEntry> localEntries;
        {
            std::lock_guard<std::mutex> lock(logMutex);
            localEntries = entries;
        }

        ImGui::TextDisabled("(%zu lines)", localEntries.size());
        ImGui::Separator();

        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("##log_scroll", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& e : localEntries) {
            if (e.hasCustomColor) {
                ImGui::PushStyleColor(ImGuiCol_Text, e.color);
            } else {
                // Default color coding based on prefix
                if (e.message.rfind("[ERROR]", 0) == 0)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                else if (e.message.rfind("[WARN]", 0) == 0 || e.message.rfind("[Warning]", 0) == 0)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
                else if (e.message.rfind("[INFO]", 0) == 0)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
                else if (e.message.rfind("[CMD]", 0) == 0)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
                else
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
            }
            
            ImGui::TextUnformatted(e.message.c_str());
            ImGui::PopStyleColor();
        }
        if (scrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            scrollToBottom = false;
        }
        ImGui::EndChild();

        ImGui::Separator();
        bool reclaim_focus = false;
        ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue;
        if (ImGui::InputText("Command", inputBuffer, sizeof(inputBuffer), input_text_flags)) {
            char* s = inputBuffer;
            if (s[0]) ExecCommand(s);
            inputBuffer[0] = 0;
            reclaim_focus = true;
        }
        ImGui::SetItemDefaultFocus();
        if (reclaim_focus) ImGui::SetKeyboardFocusHere(-1);
    }

    // Callback for external command registration
    typedef std::function<void(const std::string&)> CommandCallback;
    void SetCommandCallback(CommandCallback cb) { onCommand = cb; }

private:
    void ExecCommand(const char* cmd_line) {
        Log(std::string("[CMD] ") + cmd_line);
        if (onCommand) onCommand(cmd_line);
    }

    AppConsole() {
        AddLogInternal("[INFO] Metrix3D ready. Type /help for commands.");
        scrollToBottom = true;
        inputBuffer[0] = 0;
    }

    void AddLogInternal(const std::string& msg, const ImVec4& color = ImVec4(0,0,0,0)) {
        std::lock_guard<std::mutex> lock(logMutex);
        LogEntry e;
        e.message = msg;
        if (color.w > 0.0f) {
            e.color = color;
            e.hasCustomColor = true;
        }
        entries.push_back(e);
        if (entries.size() > 512) entries.erase(entries.begin());
        scrollToBottom = true;
    }

    std::vector<LogEntry> entries;
    char                     inputBuffer[256];
    CommandCallback          onCommand = nullptr;
    bool                     scrollToBottom = false;
    std::mutex               logMutex;
};

#define LOG_INFO(msg)  AppConsole::Instance().Log(std::string("[INFO]  ") + msg)
#define LOG_WARN(msg)  AppConsole::Instance().Log(std::string("[WARN]  ") + msg)
#define LOG_ERROR(msg) AppConsole::Instance().Log(std::string("[ERROR] ") + msg)
