#ifndef TRACK_INFO_H
#define TRACK_INFO_H

#include <map>

#include "player.h"

struct Track_Info
{
	//! Extended event struct
	struct Ext_Event
	{
		uint16_t note;
		uint16_t on_time;
		bool is_tie;
		bool is_slur;

		uint16_t off_time;

		bool coarse_vol_flag;
		uint16_t vol;
		uint16_t ins;
		uint16_t pitch_env;
		uint16_t portamento;
	};

	std::map<int, Ext_Event> events;

	int loop_start;						// -1 for no loop
	unsigned int loop_length;
	unsigned int length;
};

class Track_Info_Generator : public Player, public Track_Info
{
	public:
		Track_Info_Generator(Song& song, Track& track);

	private:
		void write_event() override;
		bool loop_hook() override;

		bool slur_flag;
};

#endif