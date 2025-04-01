#ifndef GUI_H
#define GUI_H

#include "kinesis.h"
#include "window.h"

namespace Kinesis::GUI
{
    bool show_demo;
    bool show_scene;
    bool show_rendering;
    bool show_gameobject;
    bool show_toolbar;
    bool dark_mode;
    ImVec4 clear_color;

    static void HelpMarker(const char *desc)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::BeginItemTooltip())
        {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void toolbar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Windows"))
            {
                ImGui::MenuItem("Scene Viewer", "", &show_scene);
                ImGui::MenuItem("Render Editor", "", &show_rendering);
                ImGui::MenuItem("GameObject Manager", "", &show_gameobject);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Options"))
            {
                if (ImGui::BeginMenu("ImGui"))
                {
                    ImGui::MenuItem("Show ImGui Demo", "", &show_demo);
                    ImGui::MenuItem("Dark Mode", "", &dark_mode);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
                
                if (ImGui::BeginMenu("ImGui"))
                {
                    ImGui::MenuItem("Show ImGui Demo", "", &show_demo);
                    ImGui::MenuItem("Dark Mode", "", &dark_mode);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }

    void gameobject_manager(bool *p_open)
    {
        if (!ImGui::Begin("GameObject Manager", p_open))
        {
            ImGui::End();
            return;
        }
    }

    void scene_viewer(bool *p_open)
    {
        if (!ImGui::Begin("Scene Viewer", p_open))
        {
            ImGui::End();
            return;
        }
    }

    void rendering_editor(bool *p_open)
    {
        if (!ImGui::Begin("Rendering Editor", p_open))
        {
            ImGui::End();
            return;
        }
        if (ImGui::CollapsingHeader(""))
        {
            ImGui::Text("");
        }
    }

    void initialize()
    {
        // Our state
        ImGui::StyleColorsDark();
        show_demo = false;
        show_scene = false;
        show_rendering = false;
        show_gameobject = false;
        show_toolbar = true;
        dark_mode = true;
        clear_color = ImVec4(0.45f, 0.55f, 0.60f, 0.0f);
    }

    void update_imgui()
    {
        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // show windows/darkmode if enabled
        show_demo ? ImGui::ShowDemoWindow(&show_demo) : void();
        show_toolbar ? toolbar() : void();
        show_gameobject ? gameobject_manager(&show_gameobject) : void();
        show_scene ? scene_viewer(&show_scene) : void();
        show_rendering ? rendering_editor(&show_rendering) : void();
        dark_mode ? ImGui::StyleColorsDark() : ImGui::StyleColorsLight();

        // Rendering
        ImGui::Render();

    }
}

#endif