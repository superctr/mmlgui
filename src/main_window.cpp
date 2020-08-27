#include "imgui.h"
#include "main_window.h"
#include "editor_window.h"
#include "config_window.h"

#include <iostream>
#include <csignal>

//=====================================================================
static const char* version_string = "v0.1";

static const char* gpl_string =
	"This program is free software; you can redistribute it and/or\n"
	"modify it under the terms of the GNU General Public License\n"
	// TODO: Need to clarify the license of libvgm first.
	//"as published by the Free Software Foundation; either version 2\n"
	//"of the License, or (at your option) any later version.\n"
	"version 2 as published by the Free Software Foundation.\n"
	"\n"
	"This program is distributed in the hope that it will be useful,\n"
	"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
	"GNU General Public License for more details.\n"
	"\n"
	"You should have received a copy of the GNU General Public License\n"
	"along with this program; if not, write to the Free Software\n"
	"Foundation Inc., 51 Franklin Street, Fifth Floor, Boston, MA\n"
	"02110-1301, USA.";
//=====================================================================

#ifndef IMGUI_DISABLE_DEMO_WINDOWS
static bool debug_imgui_demo_windows = false;
#endif
#ifndef IMGUI_DISABLE_METRICS_WINDOW
static bool debug_imgui_metrics = false;
#endif
static bool debug_state_window = false;

static void debug_menu()
{
#ifndef IMGUI_DISABLE_METRICS_WINDOW
	ImGui::MenuItem("ImGui metrics", NULL, &debug_imgui_metrics);
#endif
#ifndef IMGUI_DISABLE_DEMO_WINDOWS
	ImGui::MenuItem("ImGui demo windows", NULL, &debug_imgui_demo_windows);
#endif
	ImGui::MenuItem("Display dump state", NULL, &debug_state_window);
	if (ImGui::MenuItem("Quit"))
	{
		// if ctrl+shift was held, stimulate a segfault
		if(ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyAlt)
			*(int*)0 = 0;
		// if ctrl was held, raise a signal (usually that won't happen)
		if(ImGui::GetIO().KeyCtrl)
			std::raise(SIGFPE);
		// otherwise we quit normally
		else
			main_window.close_request_all();
	}
	else if (ImGui::IsItemHovered())
		ImGui::SetTooltip("hold ctrl to make a crash dump\n");
	ImGui::EndMenu();
}

static void debug_window()
{
#ifndef IMGUI_DISABLE_METRICS_WINDOW
	if(debug_imgui_metrics)
		ImGui::ShowMetricsWindow(&debug_imgui_metrics);
#endif
#ifndef IMGUI_DISABLE_DEMO_WINDOWS
	if(debug_imgui_demo_windows)
		ImGui::ShowDemoWindow(&debug_imgui_demo_windows);
#endif
	if(debug_state_window)
	{
		std::string debug_state = main_window.dump_state_all();
		ImGui::SetNextWindowPos(ImVec2(500, 400), ImGuiCond_Once);
		ImGui::Begin("Debug state", &debug_state_window);
		if (ImGui::Button("copy to clipboard"))
			ImGui::SetClipboardText(debug_state.c_str());
		ImGui::BeginChild("debug_log", ImGui::GetContentRegionAvail(), false, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::TextUnformatted(debug_state.c_str(), debug_state.c_str()+debug_state.size());
		ImGui::EndChild();
		ImGui::End();
	}
}

//=====================================================================
FPS_Overlay::FPS_Overlay()
{
	type = WT_FPS_OVERLAY;
}

void FPS_Overlay::display()
{
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
		ImGui::Text("mmlgui (%s)", version_string);
		ImGui::Separator();
		ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
		if (ImGui::BeginPopupContextWindow())
		{
			if (ImGui::BeginMenu("Debug"))
				debug_menu();
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
}

//=====================================================================
About_Window::About_Window()
{
	type = WT_ABOUT;
}
void About_Window::display()
{
	std::string window_id;
	window_id = "About mmlgui##" + std::to_string(id);

	ImGui::Begin(window_id.c_str(), &active, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("MML (Music Macro Language) editor and player.");
	ImGui::Text("%s", version_string);
	ImGui::Text("Copyright 2020 Ian Karlsson.");
	ImGui::Separator();
	ImGui::Text("Source code repository and issue reporting:");
	ImGui::Text("https://github.com/superctr/mmlgui");
	ImGui::Separator();
	ImGui::BeginChild("credits", ImVec2(500, 300), false);
	ImGui::Text(gpl_string);
	ImGui::Separator();
	ImGui::Text("This program uses the following libraries:");
	ImGui::BulletText("ctrmml\nBy Ian Karlsson\nGPLv2 or later\nhttps://github.com/superctr/ctrmml");
	ImGui::BulletText("Dear ImGui\nBy Omar Cornut, et al.\nMIT license\nhttps://github.com/ocornut/imgui");
	ImGui::BulletText("glfw\nBy Marcus Geelnard & Camilla Löwy\nzlib / libpng license\nhttps://glfw.org");
	ImGui::BulletText("ImGuiColorTextEdit\nBy BalazsJako\nMIT license\nhttps://github.com/superctr/ImGuiColorTextEdit");
	ImGui::BulletText("libvgm\nBy Valley Bell, et al.\nGPLv2\nhttps://github.com/ValleyBell/libvgm");
	ImGui::EndChild();
	ImGui::End();
}

//=====================================================================
Main_Window::Main_Window()
	: show_about(false)
	, show_config(false)
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
		if (ImGui::MenuItem("About...", nullptr, nullptr))
		{
			show_about_window();
		}
		ImGui::EndPopup();
	}
	if (show_about)
	{
		show_about = false;
		bool overlay_active = find_child(WT_ABOUT) != children.end();
		if(!overlay_active)
		{
			children.push_back(std::make_shared<About_Window>());
		}
	}
	if (show_config)
	{
		show_config = false;
		bool overlay_active = find_child(WT_CONFIG) != children.end();
		if(!overlay_active)
		{
			children.push_back(std::make_shared<Config_Window>());
		}
	}
	debug_window();
}

void Main_Window::show_about_window()
{
	show_about = true;
}

void Main_Window::show_config_window()
{
	show_config = true;
}

