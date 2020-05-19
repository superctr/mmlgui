#include "imgui.h"
#include "main_window.h"
#include "editor_window.h"

#include <iostream>
#include <csignal>
#include "editor_window.h"

//=====================================================================
// only for debug
extern Main_Window main_window;

//=====================================================================
FPS_Overlay::FPS_Overlay()
{
	type = WT_FPS_OVERLAY;
}

void FPS_Overlay::display()
{
	static bool show_metrics = false;
	const float DISTANCE = 10.0f;
	static int corner = 0;
	ImGuiIO& io = ImGui::GetIO();
	if (corner != -1)
	{
		ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE, (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
		ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
		ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
	}
	ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
	if (ImGui::Begin("overlay", &active, (corner != -1 ? ImGuiWindowFlags_NoMove : 0) | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
	{
		ImGui::Text("mmlgui (prototype)");
		ImGui::Separator();
		ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
		if (ImGui::BeginPopupContextWindow())
		{
			if (ImGui::MenuItem("Debug metrics",    NULL, show_metrics)) show_metrics ^= 1;
			if (ImGui::MenuItem("Debug state"))     std::cerr << main_window.dump_state_all() << "\n";
			if (ImGui::BeginMenu("Overlay"))
			{
				if (ImGui::MenuItem("Custom",       NULL, corner == -1)) corner = -1;
				if (ImGui::MenuItem("Top-left",     NULL, corner == 0)) corner = 0;
				if (ImGui::MenuItem("Top-right",    NULL, corner == 1)) corner = 1;
				if (ImGui::MenuItem("Bottom-left",  NULL, corner == 2)) corner = 2;
				if (ImGui::MenuItem("Bottom-right", NULL, corner == 3)) corner = 3;
				if (ImGui::MenuItem("Close")) active = false;
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}
	}
	ImGui::End();
	if(show_metrics)
		ImGui::ShowMetricsWindow(&show_metrics);
}

//=====================================================================
Main_Window::Main_Window()
{
	children.push_back(std::make_shared<FPS_Overlay>());
	children.push_back(std::make_shared<Editor_Window>());
}

void Main_Window::display()
{
	if (ImGui::BeginPopupContextVoid())
	{
		bool overlay_active = find_child(WT_FPS_OVERLAY) != children.end();
		if (ImGui::MenuItem("Show FPS Overlay", nullptr, overlay_active))
		{
			if(overlay_active)
			{
				find_child(WT_FPS_OVERLAY)->get()->close();
			}
			else
			{
				children.push_back(std::make_shared<FPS_Overlay>());
			}
		}
		if (ImGui::MenuItem("New MML...", nullptr, nullptr))
		{
			children.push_back(std::make_shared<Editor_Window>());
		}
		ImGui::EndPopup();
	}
}