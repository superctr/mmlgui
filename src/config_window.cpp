#include "imgui.h"
#include "config_window.h"

//=====================================================================
Config_Window::Config_Window()
{
	type = WT_CONFIG;
}

void Config_Window::display()
{
	std::string window_id;
	window_id = "Configuration##" + std::to_string(id);

        ImGui::SetNextWindowSize(ImVec2(500,0), ImGuiCond_Always);
	ImGui::Begin(window_id.c_str(), &active, ImGuiWindowFlags_NoResize);
	ImGui::TextColored(ImVec4(250,0,0,255), "This window is currently a placeholder.");

	ImGui::BeginChild("tab_bar", ImVec2(0, 500), false);
	ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

	if (ImGui::BeginTabBar("tabs", ImGuiTabBarFlags_None))
	{
		if (ImGui::BeginTabItem("Audio"))
		{
			show_audio_tab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Emulator"))
		{
			show_emu_tab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Mixer"))
		{
			show_mixer_tab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::PopItemWidth();
	ImGui::EndChild();

	show_confirm_buttons();

	ImGui::End();
}

void Config_Window::show_audio_tab()
{
	if (ImGui::ListBoxHeader("Audio driver", 5))
	{
		ImGui::Selectable("Placeholder", true);
		ImGui::Selectable("Placeholder", false);
		ImGui::ListBoxFooter();
	}
	ImGui::Separator();
	if (ImGui::ListBoxHeader("Audio device", 5))
	{
		ImGui::Selectable("Placeholder", true);
		ImGui::Selectable("Placeholder", false);
		ImGui::Selectable("Placeholder", false);
		ImGui::Selectable("Placeholder", false);
		ImGui::Selectable("Placeholder", false);
		ImGui::Selectable("Placeholder", false);
		ImGui::Selectable("Placeholder", false);
		ImGui::Selectable("Placeholder", false);
		ImGui::ListBoxFooter();
	}
}

void Config_Window::show_emu_tab()
{
	if (ImGui::ListBoxHeader("SN76489 emulator", 5))
	{
		ImGui::Selectable("Placeholder", true);
		ImGui::Selectable("Placeholder", false);
		ImGui::ListBoxFooter();
	}
	ImGui::Separator();
	if (ImGui::ListBoxHeader("YM2612 emulator", 5))
	{
		ImGui::Selectable("Placeholder", true);
		ImGui::Selectable("Placeholder", false);
		ImGui::ListBoxFooter();
	}
}

void Config_Window::show_mixer_tab()
{
	static int global_vol = 100;
	static int sn_vol = 100;
	static int ym_vol = 100;
	ImGui::SliderInt("Global volume", &global_vol, 0, 100);
	ImGui::Separator();
	ImGui::SliderInt("SN76489", &sn_vol, 0, 100);
	ImGui::SliderInt("YM2612", &ym_vol, 0, 100);
}

void Config_Window::show_confirm_buttons()
{
	float content = ImGui::GetContentRegionMax().x;
	//float offset = content * 0.4f;	// with progress bar
	float offset = content * 0.75f;		// without progress bar
	float width = content - offset;
	float curpos = ImGui::GetCursorPos().x;
	if(offset < curpos)
		offset = curpos;
	ImGui::SetCursorPosX(offset);

	// PushNextItemWidth doesn't work for some stupid reason, width must be set manually
	auto size = ImVec2(content * 0.1f, 0);
	if(size.x < ImGui::GetFontSize() * 4.0f)
		size.x = ImGui::GetFontSize() * 4.0f;

	if(ImGui::Button("OK", size))
	{
		active = false;
	}
	ImGui::SameLine();
	if(ImGui::Button("Cancel", size))
	{
		active = false;
	}
}
