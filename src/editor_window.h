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
		std::string filename;
		uint32_t flag;

		inline void set_flag(uint32_t data) { flag |= data; };
		inline void clear_flag(uint32_t data) { flag &= ~data; };
		inline bool test_flag(uint32_t data) { return flag & data; };

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