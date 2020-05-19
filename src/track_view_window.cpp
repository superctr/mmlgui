/*
	Track view window

	todo:
		- Note labels
		- tooltips
		- doubleclick to open reference in text editor
		- handle time signature / realign measure lines..

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

	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.3);

	ImGui::InputDouble("y_pos", &y_pos, 1.0f, 1.0f, "%.2f");
	ImGui::SameLine();
	ImGui::InputDouble("y_scale", &y_scale_log, 0.01f, 0.1f, "%.2f");
	y_scale = std::pow(y_scale_log, 2);

	ImGui::PopItemWidth();

	canvas_pos = ImGui::GetCursorScreenPos();
	canvas_size = ImGui::GetContentRegionAvail();
	if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
	if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;

	draw_list = ImGui::GetWindowDrawList();

	// handle controls
	ImGui::InvisibleButton("canvas", canvas_size);
	if(dragging)
	{
		if(!ImGui::IsMouseDown(0))
		{
			dragging = false;
		}
		else
		{
			last_mouse_pos = mouse_pos;
			mouse_pos = ImGui::GetIO().MousePos;
			y_scroll = mouse_pos.y - last_mouse_pos.y;
		}
	}
	if(ImGui::IsItemHovered())
	{
		if(!dragging && ImGui::IsMouseClicked(0))
		{
			dragging = true;
			mouse_pos = ImGui::GetIO().MousePos;
		}
	}

	// handle scrolling
	if(std::abs(y_scroll) >= inertia_threshold)
	{
		y_pos -= y_scroll / y_scale;
		y_scroll *= inertia_acceleration;
		if(y_pos < y_min)
			y_pos = y_min;
	}

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
			draw_list->AddText(
				ImGui::GetFont(),
				ImGui::GetFontSize(),
				ImVec2(
					canvas_pos.x + 2.0, //todo: centered text
					canvas_pos.y + y - (ImGui::GetFontSize()/2.)),
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

//! Draw the tracks
void Track_View_Window::draw_tracks()
{
	std::map<int, Track_Info>& map = *song_manager->get_tracks();

	double x = std::floor(ruler_width * 2.0);

	int y_off = 0;
	double yr = y_pos * y_scale;

	for(auto track_it = map.begin(); track_it != map.end(); track_it++)
	{
		auto& info = track_it->second;

		// calculate offset to first loop
		if(y_pos > info.length && info.loop_length)
			y_off = (((int)y_pos - info.loop_start) / info.loop_length) * info.loop_length;
		else
			y_off = 0;

		auto it = info.events.lower_bound(y_pos - y_off);
		double y = (it->first + y_off) * y_scale - yr;

		border_complete = true;

		if(it != info.events.begin())
		{
			--it;
			y = (it->first + y_off) * y_scale - yr;
			y = draw_event(x, y, it->first, it->second);
			++it;
		}

		for(int i=0; i<max_objs_per_column; i++)
		{
			if(y > canvas_size.y)
				break;
			if(it == info.events.end())
			{
				// go back to loop point if possible
				if(info.loop_length)
					it = info.events.lower_bound(info.loop_start);
				else
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

	// draw the note
	if(event.on_time)
	{
		draw_list->AddRectFilled(
			ImVec2(x1,y1),
			ImVec2(x2,y2),
			fill_color);
	}

	// draw the gap
	if(event.off_time)
	{
		border_complete = true;
	}

	return y + (event.on_time + event.off_time) * y_scale;
}