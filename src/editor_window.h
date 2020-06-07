#ifndef EDITOR_WINDOW
#define EDITOR_WINDOW

#include <memory>
#include <string>

#include "TextEditor.h"
#include "addons/imguifilesystem/imguifilesystem.h"

#include "window.h"
#include "song_manager.h"
#include "emu_player.h"

class Editor_Window : public Window
{
	public:
		Editor_Window();

		void display() override;
		void close_request() override;
		std::string dump_state() override;

		void play_song();
		void stop_song();

	private:
		const char* default_filename = "Untitled.mml";
		const char* default_filter = ".mml;.muc;.txt";

		void show_player_error();
		void show_close_warning();
		void show_warning();
		void handle_file_io();

		int load_file(const char* fn);
		int save_file(const char* fn);

		std::string get_display_filename();
		void show_player_controls();
		void get_compile_result();

		inline void set_flag(uint32_t data) { flag |= data; };
		inline void clear_flag(uint32_t data) { flag &= ~data; };
		inline bool test_flag(uint32_t data) { return flag & data; };

		TextEditor editor;
		ImGuiFs::Dialog fs;
		std::string filename;
		uint32_t flag;
		std::shared_ptr<Song_Manager> song_manager;

		std::shared_ptr<Emu_Player> player;
		std::string player_error;
};

#endif