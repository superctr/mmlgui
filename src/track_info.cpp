#include "track_info.h"
#include "track.h"

//! Generate Track_Info
/*!
 * \exception InputError if any validation errors occur. These should be displayed to the user.
 */
Track_Info_Generator::Track_Info_Generator(Song& song, Track& track)
	: Player(song, track)
	, Track_Info()
	, slur_flag(0)
{
	loop_start = -1;
	loop_length = 0;
	length = 0;

	// step all the way to the end
	while(is_enabled())
		step_event();

	length = get_play_time();
	if(loop_start >= 0)
		loop_length = length - loop_start;
}

void Track_Info_Generator::write_event()
{
	// Insert event
	if((event.type == Event::NOTE || event.type == Event::TIE || event.type == Event::REST) &&
		(on_time || off_time) )
	{
		// Fill the ext_event
		Track_Info::Ext_Event ext;
		ext.note = event.param;
		ext.on_time = on_time;
		ext.is_tie = (event.type == Event::TIE);
		ext.is_slur = slur_flag;
		ext.off_time = off_time;
		ext.volume = get_var(Event::VOL_FINE);
		ext.coarse_volume_flag = coarse_volume_flag();
		ext.instrument = get_var(Event::INS);
		ext.transpose = get_var(Event::TRANSPOSE);
		ext.pitch_envelope = get_var(Event::PITCH_ENVELOPE);
		ext.portamento = get_var(Event::PORTAMENTO);
		ext.references = get_references();
		if(get_var(Event::DRUM_MODE))
			ext.references.push_back(reference);
		events.emplace_hint(events.end(), std::pair<int, Track_Info::Ext_Event>(get_play_time(), ext));

		slur_flag = false;
	}
	// Record timestamp of segno event.
	else if(event.type == Event::SEGNO)
		loop_start = get_play_time();
	// Set the slur flag
	else if(event.type == Event::SLUR)
		slur_flag = true;
}

bool Track_Info_Generator::loop_hook()
{
	// do not loop
	return 0;
}
