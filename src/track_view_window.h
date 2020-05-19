#ifndef TRACK_VIEW_WINDOW_H
#define TRACK_VIEW_WINDOW_H

#include <memory>
#include <string>

#include "imgui.h"

#include "window.h"
#include "song_manager.h"

// todo: just get the Track_Info struct.
// I don't want to bring all of ctrmml in the global namespace here
#include "track_info.h"

class Track_View_Window : public Window
{
	public:
		Track_View_Window(std::shared_ptr<Song_Manager> song_mgr);

		void display() override;

	private:
		const static int max_objs_per_column;
		const static unsigned int measure_beat_count; //time signature (currently fixed)
		const static unsigned int measure_beat_value;

		const static double x_min;
		const static double y_min;
		const static double inertia_threshold;
		const static double inertia_acceleration;

		const static double ruler_width;
		const static double track_width;
		const static double padding_width;

		void draw_ruler();
		void draw_tracks();
		double draw_event(double x, double y, int position, const Track_Info::Ext_Event& event);

		double x_pos;
		double y_pos;

		double x_scale;
		double y_scale;
		double y_scale_log;

		double x_scroll;
		double y_scroll;

		std::shared_ptr<Song_Manager> song_manager;

		// scrolling state
		bool hovered;
		bool dragging;
		ImVec2 last_mouse_pos;
		ImVec2 mouse_pos;

		// drawing stuff
		ImVec2 canvas_pos;
		ImVec2 canvas_size;
		ImDrawList* draw_list;

		// buffered drawing the bottom border of tied notes
		bool border_complete;
		ImVec2 border_pos;
};

#endif