#ifndef EDITOR_WINDOW
#define EDITOR_WINDOW

#include <string>
#include "window.h"
#include "TextEditor.h"

class Editor_Window : public Window
{
	private:
		TextEditor editor;
		bool modified;
		std::string filename;

	public:
		Editor_Window();

		void display() override;
		void close_request() override;
};

#endif