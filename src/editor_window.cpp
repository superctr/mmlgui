/*
	MML text editor

	todo
		1. file i/o key shortcuts
		2. syntax highlighting for MML

	bugs in the editor (to be fixed in my fork of ImGuiColorTextEdit)
		1. numpad enter doesn't work
		2. highlighted area when doublclicking includes an extra space
*/

#include "editor_window.h"

#include "imgui.h"
#include <string>
#include <cstring>
#include <fstream>

Editor_Window::Editor_Window()
	: editor()
	, modified(false)
	, filename(default_filename)
	, filename_set(false)
	, clear_req(false)
	, load_req(false)
	, save_req(false)
	, save_as_req(false)
	, new_dialog(false)
	, ignore_warning(false)
{
	editor.SetColorizerEnable(false); // disable syntax highlighting for now
}

void Editor_Window::display()
{
	bool quit_flag = true;

	std::string window_id;
	window_id = get_display_filename();
	if(modified)
		window_id += "*";
	window_id += "###Editor" + std::to_string(id);

	auto cpos = editor.GetCursorPosition();
	ImGui::Begin(window_id.c_str(), &quit_flag, /*ImGuiWindowFlags_HorizontalScrollbar |*/ ImGuiWindowFlags_MenuBar);
	ImGui::SetWindowSize(ImVec2(800, 600), ImGuiCond_Once);
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New")) //TODO: CTRL+N
			{
				clear_req = true;
			}
			if (ImGui::MenuItem("Open...")) //TODO: CTRL+O
			{
				load_req = true;
				new_dialog = true;
			}
			if (ImGui::MenuItem("Save", nullptr, nullptr, filename_set)) //TODO: CTRL+S
			{
				save_req = true;
				new_dialog = true;
			}
			if (ImGui::MenuItem("Save As...")) //TODO: CTRL+ALT+S
			{
				save_req = true;
				save_as_req = true;
				new_dialog = true;
			}
			if (ImGui::MenuItem("Quit"))  //TODO :CTRL+W
			{
				quit_flag = false;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			bool ro = editor.IsReadOnly();
			if (ImGui::MenuItem("Read-only mode", nullptr, &ro))
				editor.SetReadOnly(ro);
			ImGui::Separator();

			if (ImGui::MenuItem("Undo", "Ctrl+Z or Alt+Backspace", nullptr, !ro && editor.CanUndo()))
				editor.Undo(), modified = 1;
			if (ImGui::MenuItem("Redo", "Ctrl+Y", nullptr, !ro && editor.CanRedo()))
				editor.Redo(), modified = 1;

			ImGui::Separator();

			if (ImGui::MenuItem("Cut", "Ctrl+X", nullptr, !ro && editor.HasSelection()))
				editor.Cut(), modified = 1;
			if (ImGui::MenuItem("Copy", "Ctrl+C", nullptr, editor.HasSelection()))
				editor.Copy();
			if (ImGui::MenuItem("Delete", "Del", nullptr, !ro && editor.HasSelection()))
				editor.Delete(), modified = 1;
			if (ImGui::MenuItem("Paste", "Ctrl+V", nullptr, !ro && ImGui::GetClipboardText() != nullptr))
				editor.Paste(), modified = 1;

			ImGui::Separator();

			if (ImGui::MenuItem("Select All", "Ctrl+A", nullptr))
				editor.SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(editor.GetTotalLines(), 0));

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Dark palette"))
				editor.SetPalette(TextEditor::GetDarkPalette());
			if (ImGui::MenuItem("Light palette"))
				editor.SetPalette(TextEditor::GetLightPalette());
			if (ImGui::MenuItem("Retro blue palette"))
				editor.SetPalette(TextEditor::GetRetroBluePalette());
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGui::BeginChild("editor view", ImVec2(0, -ImGui::GetFrameHeight())); // Leave room for 1 line below us

	editor.Render(window_id.c_str());

	if(editor.IsTextChanged())
		modified = 1;

	ImGui::EndChild();

	ImGui::Text("%6d/%-6d %6d lines  | %s", cpos.mLine + 1, cpos.mColumn + 1, editor.GetTotalLines(),
		editor.IsOverwrite() ? "Ovr" : "Ins");

	ImGui::End();

	if(!quit_flag)
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
	if(modified)
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
			ignore_warning = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			clear_req = false;
			load_req = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void Editor_Window::handle_file_io()
{
	// new file requested
	if(clear_req && !modal_open)
	{
		if(modified && !ignore_warning)
		{
			show_warning();
		}
		else
		{
			modified = false;
			filename_set = false;
			clear_req = false;
			load_req = false;
			save_req = false;
			new_dialog = false;
			ignore_warning = false;
			filename = default_filename;
			editor.SetText("");
		}
	}
	// load dialog requested
	else if(load_req && !modal_open)
	{
		if(modified && !ignore_warning)
		{
			show_warning();
		}
		else
		{
			modal_open = 1;
			fs.chooseFileDialog(new_dialog, fs.getLastDirectory(), default_filter);
			new_dialog = false;
			if(strlen(fs.getChosenPath()) > 0)
			{
				if(load_file(fs.getChosenPath()))
					new_dialog = true;
				else
				{
					ignore_warning = false;
					load_req = false;
				}
			}
			else if(fs.hasUserJustCancelledDialog())
			{
				load_req = false;
				ignore_warning = false;
			}
		}
	}
	// save dialog requested
	else if(save_req && !modal_open)
	{
		if(save_as_req || !filename_set)
		{
			fs.saveFileDialog(new_dialog, fs.getLastDirectory(), get_display_filename().c_str(), default_filter);
			new_dialog = false;
			if(strlen(fs.getChosenPath()) > 0)
			{
				if(save_file(fs.getChosenPath()))
					new_dialog = true;
				else
				{
					save_as_req = false;
					save_req = false;
				}
			}
			else if(fs.hasUserJustCancelledDialog())
			{
				save_as_req = false;
				save_req = false;
			}
		}
		else
		{
			new_dialog = false;
			save_req = false;
			save_file(filename.c_str());
		}
	}
}

std::string Editor_Window::get_display_filename()
{
	auto pos = filename.rfind("/");
	if(pos != std::string::npos)
		return filename.substr(pos+1);
	else
		return filename;
}

int Editor_Window::load_file(const char* fn)
{
	auto t = std::ifstream(fn);
	if (t.good())
	{
		modified = false;
		filename_set = true;
		filename = fn;
		std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
		editor.SetText(str);
		return 0;
	}
	return -1;
}

int Editor_Window::save_file(const char* fn)
{
	auto t = std::ofstream(fn);
	if (t.good())
	{
		modified = false;
		filename_set = true;
		filename = fn;
		std::string str = editor.GetText();
		t.write((char*)str.c_str(), str.size());
		return 0;
	}
	return -1;
}
