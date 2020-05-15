#ifndef EDITOR_WINDOW
#define EDITOR_WINDOW

#include <string>
#include "window.h"
#include "TextEditor.h"
#include "addons/imguifilesystem/imguifilesystem.h"

class Editor_Window : public Window
{
	private:
		const char* default_filename = "Untitled.mml";
		const char* default_filter = ".mml;.muc;.txt";

		TextEditor editor;
		ImGuiFs::Dialog fs;
		bool modified;
		std::string filename;
		bool filename_set;

		bool clear_req;
		bool load_req;
		bool save_req;
		bool save_as_req;
		bool new_dialog;
		bool ignore_warning;

		void show_close_warning();
		void show_warning();
		void handle_file_io();
		int load_file(const char* fn);
		int save_file(const char* fn);

		std::string get_display_filename();

	public:
		Editor_Window();

		void display() override;
		void close_request() override;
};

#endif