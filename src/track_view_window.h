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

		const static double track_header_height;
		const static double ruler_width;
		const static double track_width;
		const static double padding_width;

		void draw_ruler();

		void draw_track_header();
		void draw_tracks();
		void draw_cursors();

		double draw_event(double x, double y, int position, const Track_Info::Ext_Event& event);
		void draw_event_border(double x1, double x2, double y, const Track_Info::Ext_Event& event);

		void hover_event(int position, const Track_Info::Ext_Event& event);

		std::string get_note_name(uint16_t note) const;

		double x_pos;
		double y_pos;

		double x_scale;
		double y_scale;
		double y_scale_log;

		double x_scroll;
		double y_scroll;

		bool y_follow; // If set, follow song position.
		uint32_t y_player; // Player Y position
		double y_user; // User Y position

		std::shared_ptr<Song_Manager> song_manager;

		// scrolling state
		bool hovered;
		bool dragging;

		// tooltip state
		int hover_time;
		const Track_Info::Ext_Event* hover_obj;

		// drawing stuff
		ImVec2 canvas_pos;
		ImVec2 canvas_size;
		ImDrawList* draw_list;
		std::vector<ImVec2> cursor_list;

		// buffered drawing the bottom border of tied notes
		bool border_complete;
		ImVec2 border_pos;
};

#endif
