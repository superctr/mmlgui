/*
	Track view window

	currently implemented:
		- drag to y-zoom
		- note labels (simple)
	todo:
		- outlined note borders (?)
			- not sure if i want this anymore, the current "flat" design looks consistent
			- always draw left/right border
			- if note is tie or slur, don't draw top border
			- buffer the bottom border
				- note is tie or slur, don't draw bottom border of last note or top border of this note
				- otherwise, draw top border of this note
				- if there is a rest, draw bottom border of last note
		- channel name indicator
			- should be displayed at top
			- single click fast to mute
			- double click to solo
			- drag to x-scroll
		- tooltips
			- show note information
		- doubleclick a note/rest to open reference in text editor
		- handle time signature / realign measure lines..
		- scroll wheel to y-zoom
*/

#include "track_view_window.h"
#include "track_info.h"
#include "song.h"

#include <string>
#include <cmath>

// max objects drawn per object per frame
const int Track_View_Window::max_objs_per_column = 200;

//time signature (currently fixed)
const unsigned int Track_View_Window::measure_beat_count = 4;
const unsigned int Track_View_Window::measure_beat_value = 4;

// minimum scroll position
const double Track_View_Window::x_min = 0.0;
const double Track_View_Window::y_min = 0.0;

// scroll inertia
const double Track_View_Window::inertia_threshold = 1.0;
const double Track_View_Window::inertia_acceleration = 0.95;

// height of header
const double Track_View_Window::track_header_height = 20.0;
// width of columns
const double Track_View_Window::ruler_width = 25.0;
const double Track_View_Window::track_width = 25.0;
const double Track_View_Window::padding_width = 5.0;

Track_View_Window::Track_View_Window(std::shared_ptr<Song_Manager> song_mgr)
	: x_pos(0.0)
	, y_pos(0.0)
	, x_scale(1.0)
	, y_scale(1.0)
	, y_scale_log(1.0)
	, x_scroll(0.0)
	, y_scroll(0.0)
	, y_follow(true)
	, y_user(0.0)
	, song_manager(song_mgr)
	, dragging(false)
{
}

void Track_View_Window::display()
{
	std::string window_id;
	window_id = "Track View##" + std::to_string(id);

	ImGui::Begin(window_id.c_str(), &active);
	ImGui::SetWindowSize(ImVec2(550, 600), ImGuiCond_Once);

	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.2);

	ImGui::InputDouble("Time", &y_user, 1.0f, 1.0f, "%.2f");
	ImGui::SameLine();
	ImGui::InputDouble("Scale", &y_scale_log, 0.01f, 0.1f, "%.2f");
	ImGui::SameLine();
	ImGui::Checkbox("Follow", &y_follow);
	if(y_player)
	{
		ImGui::SameLine();
		ImGui::Text("%d", y_player);
	}
	y_scale = std::pow(y_scale_log, 2);

	ImGui::PopItemWidth();

	canvas_pos = ImGui::GetCursorScreenPos();
	canvas_size = ImGui::GetContentRegionAvail();
	if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
	if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;

	draw_list = ImGui::GetWindowDrawList();
	cursor_list.clear();

	// handle controls
	ImGuiIO& io = ImGui::GetIO();
	ImGui::InvisibleButton("canvas", canvas_size);
	if(dragging)
	{
		if(!ImGui::IsMouseDown(0))
			dragging = false;
		else
			y_scroll = io.MouseDelta.y;
	}
	if(ImGui::IsItemHovered())
	{
		if(!dragging && ImGui::IsMouseClicked(0))
			dragging = true;
		y_scroll += io.MouseWheel * 5;
	}

	// handle scrolling
	if(std::abs(y_scroll) >= inertia_threshold)
	{
		y_user -= y_scroll / y_scale;
		y_scroll *= inertia_acceleration;
		if(y_user < y_min)
			y_user = y_min;
	}

	// Get player position
	auto player =  song_manager->get_player();
	if(player != nullptr && !player->get_finished())
		y_player = player->get_driver()->get_player_ticks();
	else
		y_player = 0;

	// Set scroll position
	if(y_follow && y_player)
		y_pos = y_player - (canvas_size.y / 2.0) / y_scale;
	else
		y_pos = y_user - track_header_height / y_scale;

	// draw background
	draw_list->AddRectFilled(
		canvas_pos,
		ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
		IM_COL32(0, 0, 0, 255));

	// draw ruler and beat lines
	draw_list->PushClipRect(
		canvas_pos,
		ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
		true);
	draw_ruler();
	draw_list->PopClipRect();

	// draw track events
	draw_list->PushClipRect(
		ImVec2(canvas_pos.x + ruler_width, canvas_pos.y),
		ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
		true);
	draw_tracks();
	draw_track_header();
	draw_list->PopClipRect();

	// draw cursor
	draw_list->PushClipRect(
		canvas_pos,
		ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
		true);
	draw_cursors();
	draw_list->PopClipRect();

	ImGui::End();
}


//! Draw measure and beat ruler
void Track_View_Window::draw_ruler()
{
	int yp = y_pos;

	// calculate time signature
	unsigned int whole = song_manager->get_song()->get_ppqn() * 4;
	unsigned int beat_len = whole / measure_beat_value;
	//unsigned int measure_len = measure_beat_value * measure_beat_count;

	// calculate current beat
	int beat = y_pos / beat_len;

	// start from the beginning of the beat (above clip area if necessary)
	double y = 0.0 - std::fmod(y_pos, beat_len) * y_scale;

	// special case for negative position
	if(y_pos < 0)
	{
		y = -y_pos * y_scale;
		yp = 0;
		beat = 0;
	}

	double ruler_width = 25.0;

	// draw ruler face
	draw_list->AddRectFilled(
		canvas_pos,
		ImVec2(
			canvas_pos.x + std::floor(ruler_width),
			canvas_pos.y + canvas_size.y),
		IM_COL32(85, 85, 85, 255));

	// draw beats
	for(int i=0; i<max_objs_per_column; i++)
	{
		// beginning of measure?
		if(beat % measure_beat_count == 0)
		{
			// draw measure number
			int measure = beat / measure_beat_count;
			std::string str = std::to_string(measure);

			ImVec2 size = ImGui::GetFont()->CalcTextSizeA(
				ImGui::GetFontSize(),
				ruler_width,
				ruler_width,
				str.c_str());

			draw_list->AddText(
				ImGui::GetFont(),
				ImGui::GetFontSize(),
				ImVec2(
					canvas_pos.x + ruler_width/2 - size.x/2,
					canvas_pos.y + y - size.y/2),
				IM_COL32(255, 255, 255, 255),
				str.c_str());

			// draw a "bright" line
			draw_list->AddRectFilled(
				ImVec2(
					canvas_pos.x + std::floor(ruler_width),
					canvas_pos.y + std::floor(y)),
				ImVec2(
					canvas_pos.x + canvas_size.x,
					canvas_pos.y + std::floor(y)+1),
				IM_COL32(85, 85, 85, 255));
		}
		else
		{
			// draw a dark line
			draw_list->AddRectFilled(
				ImVec2(
					canvas_pos.x + std::floor(ruler_width),
					canvas_pos.y + std::floor(y)),
				ImVec2(
					canvas_pos.x + canvas_size.x,
					canvas_pos.y + std::floor(y)+1),
				IM_COL32(35, 35, 35, 255));
		}
		beat++;
		y += beat_len * y_scale;
		if(y > canvas_size.y)
			break;
	}
}

//! Draw the track header
void Track_View_Window::draw_track_header()
{
	double x1  = canvas_pos.x + ruler_width;
	double x2  = canvas_pos.x + canvas_size.x;
	double y1  = canvas_pos.y;
	double y2  = canvas_pos.y + track_header_height;
	ImU32 fill_color = IM_COL32(65, 65, 65, 180);

	draw_list->AddRectFilled(
		ImVec2(x1,y1),
		ImVec2(x2,y2),
		fill_color);

	Song_Manager::Track_Map& map = *song_manager->get_tracks();

	double x = std::floor(ruler_width * 2.0);

	for(auto track_it = map.begin(); track_it != map.end(); track_it++)
	{
		x1 = canvas_pos.x + std::floor(x);
		x2 = canvas_pos.x + std::floor(x + track_width);

		int id = track_it->first;

		static const double margin = 2.0;
		double max_width = track_width - margin * 2;

		std::string str = "";
		if(id < 'Z'-'A')
			str.push_back(id + 'A');
		else
			str = std::to_string(id);

		ImFont* font = ImGui::GetFont();
		ImVec2 size = font->CalcTextSizeA(font->FontSize, max_width, max_width, str.c_str());

		double y_offset = (track_header_height - font->FontSize) / 2.0;

		draw_list->AddText(
			font,
			font->FontSize,
			ImVec2(
				x1 + track_width/2 - size.x/2,
				y1 + y_offset),
			IM_COL32(255, 255, 255, 255),
			str.c_str());

		x += std::floor(track_width + padding_width);
	}
}

//! Draw the tracks
void Track_View_Window::draw_tracks()
{
	auto map = song_manager->get_tracks();

	double x = std::floor(ruler_width * 2.0);

	int y_off = 0;
	double yr = y_pos * y_scale;

	for(auto track_it = map->begin(); track_it != map->end(); track_it++)
	{
		auto& info = track_it->second;

		// calculate offset to first loop
		if(y_pos > info.length && info.loop_length)
			y_off = (((int)y_pos - info.loop_start) / info.loop_length) * info.loop_length;
		else
			y_off = 0;

		// calculate position
		auto it = info.events.lower_bound(y_pos - y_off);
		double y = (it->first + y_off) * y_scale - yr;

		border_complete = true;
		last_ref = nullptr;

		// draw the previous event if we can
		if(it != info.events.begin())
		{
			--it;
			y = (it->first + y_off) * y_scale - yr;
			y = draw_event(x, y, it->first, it->second);
			++it;
		}

		// draw each event
		for(int i=0; i<max_objs_per_column; i++)
		{
			if(it == info.events.end())
			{
				// go back to loop point if possible
				if(info.loop_length)
					it = info.events.lower_bound(info.loop_start);
				else
					break;
			}
			if(y > canvas_size.y)
			{
				if(it->second.on_time)
				{
					double x1 = canvas_pos.x + std::floor(x);
					double x2 = canvas_pos.x + std::floor(x + track_width);
					double y1 = canvas_pos.y + std::floor(y);
					draw_event_border(x1, x2, y1, it->second);
				}
				break;
			}
			y = draw_event(x, y, it->first, it->second);
			it++;
		}

		x += std::floor(track_width + padding_width);
	}
}

//! Draw a single event
double Track_View_Window::draw_event(double x, double y, int position, const Track_Info::Ext_Event& event)
{
	// calculate coordinates
	double x1  = canvas_pos.x + std::floor(x);
	double x2  = canvas_pos.x + std::floor(x + track_width);
	double y1  = canvas_pos.y + std::floor(y);
	double y2  = canvas_pos.y + std::floor(y + event.on_time * y_scale);
	double y2a = canvas_pos.y + std::floor(y + (event.on_time + event.off_time) * y_scale);
	ImU32 fill_color = IM_COL32(195, 0, 0, 255);

	//testing
	if(ImGui::GetIO().MousePos.y >= canvas_pos.y + track_header_height
		&& ImGui::IsMouseHoveringRect(ImVec2(x1,y1),ImVec2(x2,y2a))
		&& ImGui::IsItemHovered())
	{
		fill_color = IM_COL32(235, 40, 40, 255);
		hover_event(position, event);
	}

	// draw the note
	if(event.on_time)
	{
		draw_event_border(x1, x2, y1, event);

		draw_list->AddRectFilled(
			ImVec2(x1,y1),
			ImVec2(x2,y2),
			fill_color);

		// draw note text
		ImFont* font = ImGui::GetFont();
		if(!event.is_tie && std::floor(event.on_time * y_scale) > font->FontSize)
		{
			static const double margin = 2.0;
			std::string str = get_note_name(event.note + event.transpose);
			double max_width = track_width - margin * 2;
			ImVec2 size = font->CalcTextSizeA(font->FontSize, max_width, max_width, str.c_str());

			draw_list->AddText(
				font,
				font->FontSize,
				ImVec2(
					x1 + track_width/2 - size.x/2,
					y1 + margin),
				IM_COL32(255, 255, 255, 255),
				str.c_str());
		}

	}

	// draw the gap
	if(event.off_time)
	{
		border_complete = true;
	}

	// add editor cursor
	auto editor_refs = song_manager->get_editor_refs();
	int index = 0;
	for(auto&& ref : event.references)
	{
		if(editor_refs.count(ref.get()))
		{
			auto editor_pos = song_manager->get_editor_position();
			double cursor_y = y1;

			// Set flag if we have already displayed a cursor for the current ref, and we are in a subroutine call.
			bool jump_hack = !song_manager->get_editor_subroutine() && (event.references.size() > 1) && last_ref == ref.get();
			last_ref = ref.get();

			if((int)ref->get_line() < editor_pos.line || (int)ref->get_column() < editor_pos.column)
			{
				cursor_y = y2a;
				if(jump_hack && cursor_list.size())
					cursor_list[cursor_list.size() - 1] = ImVec2(std::floor(x1 - padding_width / 2), cursor_y);
			}

			if(!jump_hack)
				cursor_list.push_back(ImVec2(std::floor(x1 - padding_width / 2), cursor_y));
		}
		index++;
	}

	return y + (event.on_time + event.off_time) * y_scale;

}

void Track_View_Window::draw_event_border(double x1, double x2, double y, const Track_Info::Ext_Event& event)
{
	int border_width = y_scale * 0.55;

	// draw a border before the note if we're not a slur or tie
	if(!event.is_tie && !event.is_slur && border_width)
	{
		draw_list->AddRectFilled(
			ImVec2(x1,y-border_width),
			ImVec2(x2,y),
			IM_COL32(0, 0, 0, 255));
	}
}

//! Handle mouse hovering
void Track_View_Window::hover_event(int position, const Track_Info::Ext_Event& event)
{
	if((hover_obj != &event) || dragging)
	{
		hover_obj = &event;
		hover_time = 0;
	}
	else
	{
		hover_time++;
		if(hover_time > 20)
		{
			ImGui::BeginTooltip();
			ImGui::Text("@%d %c%d",
				event.instrument,
				(event.coarse_volume_flag) ? 'v' : 'V',
				event.volume);
			if(event.transpose)
			{
				ImGui::SameLine();
				ImGui::Text("k%d", event.transpose);
			}
			if(event.pitch_envelope)
			{
				ImGui::SameLine();
				ImGui::Text("M%d", event.pitch_envelope);
			}
			if(event.portamento)
			{
				ImGui::SameLine();
				ImGui::Text("G%d", event.portamento);
			}
			ImGui::SameLine();
			if(event.on_time && !event.is_tie)
			{
				ImGui::Text("o%d%s",
					event.note/12,
					get_note_name(event.note).c_str());
				ImGui::Text("t: %d-%d", position, position+event.on_time - 1);
			}
			else if(event.is_tie)
			{
				ImGui::Text(";tie");
				if(event.on_time)
					ImGui::Text("t: %d-%d", position, position+event.on_time - 1);
				else
					ImGui::Text("t: %d", position);
			}
			else if(event.off_time)
			{
				ImGui::Text(";rest");
				ImGui::Text("t: %d", position);
			}
			ImGui::EndTooltip();
		}
	}
}

//! Get the note name
std::string Track_View_Window::get_note_name(uint16_t note) const
{
	const char* semitones[12] = { "c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b" };
	std::string str = semitones[note % 12];
	// TODO: add octave if we have enough space

	return str;
}

void Track_View_Window::draw_cursors()
{
	if(y_player)
	{
		// draw player position
		double y = (y_player - y_pos) * y_scale;
		if(y > 0 && y < canvas_size.y)
		{
			draw_list->AddRectFilled(
				ImVec2(
					canvas_pos.x + std::floor(ruler_width),
					canvas_pos.y + std::floor(y)),
				ImVec2(
					canvas_pos.x + canvas_size.x,
					canvas_pos.y + std::floor(y)+1),
				IM_COL32(0, 200, 0, 255));
		}
	}
	// Draw editor cursors.
	for(auto && i : cursor_list)
	{
		draw_list->AddRectFilled(
			i,
			ImVec2(
				i.x + track_width + padding_width,
				i.y + 1),
			IM_COL32(255, 255, 255, 255));
	}
}
