#ifndef TRACK_LIST_WINDOW_H
#define TRACK_LIST_WINDOW_H

#include <memory>
#include <string>

#include "imgui.h"

#include "window.h"
#include "song_manager.h"

// todo: just get the Track_Info struct.
// I don't want to bring all of ctrmml in the global namespace here
#include "track_info.h"

class Track_List_Window : public Window
{
	public:
		Track_List_Window(std::shared_ptr<Song_Manager> song_mgr);

		void display() override;

	private:
		std::shared_ptr<Song_Manager> song_manager;
};

#endif
