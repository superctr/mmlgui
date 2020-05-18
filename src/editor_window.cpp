/*
	MML text editor

	todo
		1. file i/o key shortcuts
		2. syntax highlighting for MML

	bugs in the editor (to be fixed in my fork of ImGuiColorTextEdit)
		1. highlighted area when doubleclicking doesn't take account punctuation
			(based on the code, This should hopefully automatically be fixed when
			 MML highlighting is done)
		2. data copied to clipboard from mmlgui has UNIX-linebreaks regardless of OS.
		   This may break some windows tools (such as Notepad.exe)

	"bugs" in the imgui addons branch (maybe can be fixed locally and upstreamed?)
		1. file dialog doesn't automatically focus the filename.
		2. file dialog has no keyboard controls at all :/
*/

#include "editor_window.h"
#include "track_view_window.h"

#include "imgui.h"

#include <GLFW/glfw3.h>
#include <string>
#include <cstring>
#include <fstream>

enum Flags
{
	MODIFIED		= 1<<0,
	FILENAME_SET	= 1<<1,
	NEW				= 1<<2,
	OPEN			= 1<<3,
	SAVE			= 1<<4,
	SAVE_AS			= 1<<5,
	DIALOG			= 1<<6,
	IGNORE_WARNING	= 1<<7,
	RECOMPILE       = 1<<8,
};

Editor_Window::Editor_Window()
	: editor()
	, filename(default_filename)
	, flag(RECOMPILE)
{
	editor.SetColorizerEnable(false); // disable syntax highlighting for now
	song_manager = std::make_shared<Song_Manager>();
}

void Editor_Window::display()
{
	bool keep_open = true;

	if(test_flag(RECOMPILE))
	{
		if(!song_manager->get_compile_in_progress())
		{
			if(!song_manager->compile(editor.GetText(), ""))
				clear_flag(RECOMPILE);
		}
	}

	std::string window_id;
	window_id = get_display_filename();
	if(test_flag(MODIFIED))
		window_id += "*";
	window_id += "###Editor" + std::to_string(id);

	auto cpos = editor.GetCursorPosition();
	ImGui::Begin(window_id.c_str(), &keep_open, /*ImGuiWindowFlags_HorizontalScrollbar |*/ ImGuiWindowFlags_MenuBar);
	ImGui::SetWindowSize(ImVec2(800, 600), ImGuiCond_Once);

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New", "Ctrl+N"))
				set_flag(NEW);
			if (ImGui::MenuItem("Open...", "Ctrl+O"))
				set_flag(OPEN|DIALOG);
			if (ImGui::MenuItem("Save", "Ctrl+S", nullptr, test_flag(FILENAME_SET)))
				set_flag(SAVE|DIALOG);
			if (ImGui::MenuItem("Save As...", "Ctrl+Alt+S"))
				set_flag(SAVE|SAVE_AS|DIALOG);
			if (ImGui::MenuItem("Close", "Ctrl+W"))
				keep_open = false;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			bool ro = editor.IsReadOnly();

			if (ImGui::MenuItem("Undo", "Ctrl+Z or Alt+Backspace", nullptr, !ro && editor.CanUndo()))
				editor.Undo(), set_flag(MODIFIED|RECOMPILE);
			if (ImGui::MenuItem("Redo", "Ctrl+Y", nullptr, !ro && editor.CanRedo()))
				editor.Redo(), set_flag(MODIFIED|RECOMPILE);

			ImGui::Separator();

			if (ImGui::MenuItem("Cut", "Ctrl+X", nullptr, !ro && editor.HasSelection()))
				editor.Cut(), set_flag(MODIFIED|RECOMPILE);
			if (ImGui::MenuItem("Copy", "Ctrl+C", nullptr, editor.HasSelection()))
				editor.Copy();
			if (ImGui::MenuItem("Delete", "Del", nullptr, !ro && editor.HasSelection()))
				editor.Delete(), set_flag(MODIFIED|RECOMPILE);

			GLFWerrorfun prev_error_callback = glfwSetErrorCallback(NULL); // disable clipboard error messages...

			if (ImGui::MenuItem("Paste", "Ctrl+V", nullptr, !ro && ImGui::GetClipboardText() != nullptr))
				editor.Paste(), set_flag(MODIFIED|RECOMPILE);

			glfwSetErrorCallback(prev_error_callback);

			ImGui::Separator();

			if (ImGui::MenuItem("Select All", "Ctrl+A", nullptr))
				editor.SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(editor.GetTotalLines(), 0));

			ImGui::Separator();

			if (ImGui::MenuItem("Read-only mode", nullptr, &ro))
				editor.SetReadOnly(ro);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Track view..."))
			{
				children.push_back(std::make_shared<Track_View_Window>(song_manager));
			}
			ImGui::Separator();
			if (ImGui::BeginMenu("Editor style"))
			{
				if (ImGui::MenuItem("Dark palette"))
					editor.SetPalette(TextEditor::GetDarkPalette());
				if (ImGui::MenuItem("Light palette"))
					editor.SetPalette(TextEditor::GetLightPalette());
				if (ImGui::MenuItem("Retro blue palette"))
					editor.SetPalette(TextEditor::GetRetroBluePalette());
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	// focus on the text editor rather than the "frame"
	if (ImGui::IsWindowFocused())
		ImGui::SetNextWindowFocus();

	GLFWerrorfun prev_error_callback = glfwSetErrorCallback(NULL); // disable clipboard error messages...

	editor.Render("EditorArea", ImVec2(0, -ImGui::GetFrameHeight()));

	glfwSetErrorCallback(prev_error_callback);

	if(editor.IsTextChanged())
		set_flag(MODIFIED|RECOMPILE);

	if (ImGui::IsWindowFocused() | editor.IsWindowFocused())
	{
		ImGuiIO& io = ImGui::GetIO();
		auto isOSX = io.ConfigMacOSXBehaviors;
		auto alt = io.KeyAlt;
		auto ctrl = io.KeyCtrl;
		auto shift = io.KeyShift;
		auto super = io.KeySuper;
		auto isShortcut = (isOSX ? (super && !ctrl) : (ctrl && !super)) && !alt && !shift;
		auto isAltShortcut = (isOSX ? (super && !ctrl) : (ctrl && !super)) && alt && !shift;

		if (isShortcut && ImGui::IsKeyPressed(GLFW_KEY_N))
			set_flag(NEW);
		else if (isShortcut && ImGui::IsKeyPressed(GLFW_KEY_O))
			set_flag(OPEN|DIALOG);
		else if (isShortcut && ImGui::IsKeyPressed(GLFW_KEY_S))
			set_flag(SAVE|DIALOG);
		else if (isAltShortcut && ImGui::IsKeyPressed(GLFW_KEY_S))
			set_flag(SAVE|SAVE_AS|DIALOG);
		else if (isShortcut && ImGui::IsKeyPressed(GLFW_KEY_W))
			keep_open = false;
	}

	ImGui::Spacing();
	ImGui::Text("%6d/%-6d %6d line%c  | %s |", cpos.mLine + 1, cpos.mColumn + 1, editor.GetTotalLines(), (editor.GetTotalLines() == 1) ? ' ' : 's',
		editor.IsOverwrite() ? "Ovr" : "Ins");

	get_compile_result();

	ImGui::End();

	if(!keep_open)
		close_request_all();

	if(get_close_request() == Window::CLOSE_IN_PROGRESS && !modal_open)
		show_close_warning();

	if(get_close_request() == Window::CLOSE_OK)
		active = false;
	else
		handle_file_io();
}

void Editor_Window::close_request()
{
	if(test_flag(MODIFIED))
		close_req_state = Window::CLOSE_IN_PROGRESS;
	else
		close_req_state = Window::CLOSE_OK;
}

void Editor_Window::show_close_warning()
{
	modal_open = 1;
	std::string modal_id;
	modal_id = get_display_filename() + "###modal";
	if (!ImGui::IsPopupOpen(modal_id.c_str()))
		ImGui::OpenPopup(modal_id.c_str());
	if(ImGui::BeginPopupModal(modal_id.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("You have unsaved changes. Close anyway?\n");
		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			close_req_state = Window::CLOSE_OK;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			close_req_state = Window::CLOSE_NOT_OK;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void Editor_Window::show_warning()
{
	modal_open = 1;
	std::string modal_id;
	modal_id = get_display_filename() + "###modal";
	if (!ImGui::IsPopupOpen(modal_id.c_str()))
		ImGui::OpenPopup(modal_id.c_str());
	if(ImGui::BeginPopupModal(modal_id.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("You have unsaved changes. Continue anyway?\n");
		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			set_flag(IGNORE_WARNING);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			clear_flag(NEW|OPEN);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void Editor_Window::handle_file_io()
{
	// new file requested
	if(test_flag(NEW) && !modal_open)
	{
		if(test_flag(MODIFIED) && !test_flag(IGNORE_WARNING))
		{
			show_warning();
		}
		else
		{
			set_flag(RECOMPILE);
			clear_flag(MODIFIED|FILENAME_SET|NEW|OPEN|SAVE|DIALOG|IGNORE_WARNING);
			filename = default_filename;
			editor.SetText("");
		}
	}
	// open dialog requested
	else if(test_flag(OPEN) && !modal_open)
	{
		if((flag & MODIFIED) && !(flag & IGNORE_WARNING))
		{
			show_warning();
		}
		else
		{
			modal_open = 1;
			fs.chooseFileDialog(test_flag(DIALOG), fs.getLastDirectory(), default_filter);
			clear_flag(DIALOG);
			if(strlen(fs.getChosenPath()) > 0)
			{
				if(load_file(fs.getChosenPath()))
					set_flag(DIALOG);						// File couldn't be opened
				else
					clear_flag(OPEN|IGNORE_WARNING);
			}
			else if(fs.hasUserJustCancelledDialog())
			{
				clear_flag(OPEN|IGNORE_WARNING);
			}
		}
	}
	// save dialog requested
	else if(test_flag(SAVE) && !modal_open)
	{
		if(test_flag(SAVE_AS) || !test_flag(FILENAME_SET))
		{
			modal_open = 1;
			fs.saveFileDialog(test_flag(DIALOG), fs.getLastDirectory(), get_display_filename().c_str(), default_filter);
			clear_flag(DIALOG);
			if(strlen(fs.getChosenPath()) > 0)
			{
				if(save_file(fs.getChosenPath()))
					set_flag(DIALOG);						// File couldn't be saved
				else
					clear_flag(SAVE|SAVE_AS);
			}
			else if(fs.hasUserJustCancelledDialog())
			{
				clear_flag(SAVE|SAVE_AS);
			}
		}
		else
		{
			// TODO: show a message if file couldn't be saved ...
			clear_flag(DIALOG|SAVE);
			save_file(filename.c_str());
		}
	}
}

int Editor_Window::load_file(const char* fn)
{
	auto t = std::ifstream(fn);
	if (t.good())
	{
		clear_flag(MODIFIED);
		set_flag(FILENAME_SET|RECOMPILE);
		filename = fn;
		std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
		editor.SetText(str);
		return 0;
	}
	return -1;
}

int Editor_Window::save_file(const char* fn)
{
	// If we don't set ios::binary, the runtime will
	// convert linebreaks to the OS native format.
	// I think it is OK to keep this behavior for now.
	auto t = std::ofstream(fn);
	if (t.good())
	{
		clear_flag(MODIFIED);
		set_flag(FILENAME_SET|RECOMPILE);
		filename = fn;
		std::string str = editor.GetText();
		t.write((char*)str.c_str(), str.size());
		return 0;
	}
	return -1;
}

std::string Editor_Window::get_display_filename()
{
	auto pos = filename.rfind("/");
	if(pos != std::string::npos)
		return filename.substr(pos+1);
	else
		return filename;
}

void Editor_Window::get_compile_result()
{
	ImGui::SameLine();
	switch(song_manager->get_compile_result())
	{
		default:
		case Song_Manager::COMPILE_NOT_DONE:
			ImGui::Text("Compiling");
			break;
		case Song_Manager::COMPILE_OK:
			ImGui::Text("OK");
			break;
		case Song_Manager::COMPILE_ERROR:
			ImGui::Text("Error");
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				ImGui::TextUnformatted(song_manager->get_error_message().c_str());
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
			break;
	}
}
