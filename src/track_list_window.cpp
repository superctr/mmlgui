#include "track_list_window.h"
#include "track_info.h"
#include "song.h"

Track_List_Window::Track_List_Window(std::shared_ptr<Song_Manager> song_mgr)
	: song_manager(song_mgr)
{
}

void Track_List_Window::display()
{
	// Draw window
	std::string window_id;
	window_id = "Track List##" + std::to_string(id);

	ImGui::Begin(window_id.c_str(), &active);
	ImGui::SetWindowSize(ImVec2(200, 300), ImGuiCond_Once);

	ImGui::Columns(3, "tracklist");
	ImGui::Separator();
	ImGui::Text("Name"); ImGui::NextColumn();
	ImGui::Text("Length"); ImGui::NextColumn();
	ImGui::Text("Loop"); ImGui::NextColumn();
	ImGui::Separator();

	Song_Manager::Track_Map& map = *song_manager->get_tracks();

	for(auto&& i : map)
	{
		int id = i.first;
		std::string str = "";
		if(id < 'Z'-'A')
			str.push_back(id + 'A');
		else
			str = std::to_string(id);

		ImGui::Selectable(str.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
		ImGui::NextColumn();
		ImGui::Text("%5d", i.second.length);
		ImGui::NextColumn();
		ImGui::Text("%5d", i.second.loop_length);
		ImGui::NextColumn();
	}
	ImGui::Columns(1);
	ImGui::Separator();

	ImGui::End();
}
