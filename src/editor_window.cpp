/*
	MML text editor

	todo
		1. file i/o !!!
		2. syntax highlighting for MML

	bugs in the editor (to be fixed in my fork of ImGuiColorTextEdit)
		1. numpad enter doesn't work
		2. highlighted area when doublclicking includes an extra space
*/

#include "editor_window.h"

#include "imgui.h"
#include <string>

Editor_Window::Editor_Window()
	: editor()
	, modified(false)
	, filename("Untitled")
{
	editor.SetColorizerEnable(false); // disable syntax highlighting for now
}

void Editor_Window::display()
{
	bool quit_flag = true;

	std::string window_id;
	window_id = filename;
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
			if (ImGui::MenuItem("Save"))
			{
				auto textToSave = editor.GetText();
				/// save text....
			}
			if (ImGui::MenuItem("Quit", "Alt+F4"))
				quit_flag = false;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			bool ro = editor.IsReadOnly();
			if (ImGui::MenuItem("Read-only mode", nullptr, &ro))
				editor.SetReadOnly(ro);
			ImGui::Separator();

			if (ImGui::MenuItem("Undo", "Ctrl+Z or Alt+Backspace", nullptr, !ro && editor.CanUndo()))
				editor.Undo();
			if (ImGui::MenuItem("Redo", "Ctrl+Y", nullptr, !ro && editor.CanRedo()))
				editor.Redo();

			ImGui::Separator();

			if (ImGui::MenuItem("Copy", "Ctrl+C", nullptr, editor.HasSelection()))
				editor.Copy();
			if (ImGui::MenuItem("Cut", "Ctrl+X", nullptr, !ro && editor.HasSelection()))
				editor.Cut();
			if (ImGui::MenuItem("Delete", "Del", nullptr, !ro && editor.HasSelection()))
				editor.Delete();
			if (ImGui::MenuItem("Paste", "Ctrl+V", nullptr, !ro && ImGui::GetClipboardText() != nullptr))
				editor.Paste();

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
		modified |= 1;
	ImGui::EndChild();
	ImGui::Text("%6d/%-6d %6d lines  | %s", cpos.mLine + 1, cpos.mColumn + 1, editor.GetTotalLines(),
		editor.IsOverwrite() ? "Ovr" : "Ins");

	ImGui::End();

	if(!quit_flag)
		close_request_all();

	if(get_close_request() == Window::CLOSE_IN_PROGRESS && !modal_open)
	{
		modal_open = 1;

		std::string modal_id;
		modal_id = filename + "###modal";
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
	if(get_close_request() == Window::CLOSE_OK)
		active = false;
}

void Editor_Window::close_request()
{
	if(modified)
		close_req_state = Window::CLOSE_IN_PROGRESS;
	else
		close_req_state = Window::CLOSE_OK;
}