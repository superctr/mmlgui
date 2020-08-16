#include "song_manager.h"
#include "track_info.h"
#include "song.h"
#include "input.h"
#include "player.h"

#include "mml_input.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <climits>

const int Song_Manager::max_channels = 16;

//! constructs a Song_Manager
Song_Manager::Song_Manager()
	: worker_ptr(nullptr)
	, worker_fired(false)
	, job_done(false)
	, job_successful(false)
	, song(nullptr)
	, player(nullptr)
	, editor_position({-1, -1})
	, editor_jump_hack(false)
	, song_pos_at_line(0)
	, song_pos_at_cursor(0)
{
}

//! Song_Manager destructor
Song_Manager::~Song_Manager()
{
	if(worker_ptr && worker_ptr->joinable())
	{
		{
			// kill worker thread
			std::lock_guard<std::mutex> guard(mutex);
			worker_fired = true;
		}
		condition_variable.notify_one();

		if(!job_done)
		{
			// detach the compile thread in order to not crash the entire application
			std::cerr << "Song_Manager killed during ongoing compile job. Compiler stuck?\n";
			worker_ptr->detach();
		}
		else
		{
			// wait for compile thread to end normally
			worker_ptr->join();
		}
	}
}

//! Get compile result
Song_Manager::Compile_Result Song_Manager::get_compile_result()
{
	std::lock_guard<std::mutex> guard(mutex);
	if(!job_done || !worker_ptr)
		return COMPILE_NOT_DONE;
	else if(job_successful)
		return COMPILE_OK;
	else
		return COMPILE_ERROR;
}

//! Get compile in progress
bool Song_Manager::get_compile_in_progress()
{
	std::lock_guard<std::mutex> guard(mutex);
	if(!worker_ptr)
		return false;
	else
		return !job_done;
}

//! Compile from a buffer
/*
 *  \return non-zero if compile thread was busy.
 *  \return zero if compile was successfully started
 */
int Song_Manager::compile(const std::string& buffer, const std::string& filename)
{
	{
		std::lock_guard<std::mutex> guard(mutex);
		if(job_done || !worker_ptr)
		{
			if(!worker_ptr)
				worker_ptr = std::make_unique<std::thread>(&Song_Manager::worker, this);

			job_buffer = buffer;
			job_filename = filename;
			job_done = 0;
			job_successful = 0;
		}
		else
		{
			return -1;
		}
	}
	condition_variable.notify_one();
	return 0;
}

//! Start song playback.
/*!
 *  \exception InputError Song playback errors, for example missing samples or bad data.
 *  \exception std::exception Should always be catched to avoid data loss.
 */
void Song_Manager::play(uint32_t start_position)
{
	stop();

	Audio_Manager& am = Audio_Manager::get();
	player = std::make_shared<Emu_Player>(get_song(), start_position);
	am.add_stream(std::static_pointer_cast<Audio_Stream>(player));
}

//! Stop song playback
void Song_Manager::stop()
{
	if(player.get() != nullptr)
	{
		player->set_finished(true);
	}
}

//! Get song data
std::shared_ptr<Song> Song_Manager::get_song()
{
	std::lock_guard<std::mutex> guard(mutex);
	return song;
}

//! Get player
std::shared_ptr<Emu_Player> Song_Manager::get_player()
{
	return player;
}

//! Get track info data
std::shared_ptr<std::map<int,Track_Info>> Song_Manager::get_tracks()
{
	std::lock_guard<std::mutex> guard(mutex);
	return tracks;
}

//! Get line info
std::shared_ptr<Song_Manager::Line_Map> Song_Manager::get_lines()
{
	std::lock_guard<std::mutex> guard(mutex);
	return lines;
}

//! Get error message
std::string Song_Manager::get_error_message()
{
	std::lock_guard<std::mutex> guard(mutex);
	return error_message;
}

//! Set the current editor position. Used to display cursors. Call this from the UI thread.
/*!
 *  Cursor should be displayed if an InputRef* from editor_refs can be also found in the Track_Info array (tracks).
 *
 *  This approach is not perfect, there are a few things that need to be looked into.
 *  - A subroutine which does not contain any notes will cause the cursor to not appear. See comment below.
 *  - Cursor will still point to the notes inside the loop, even if the editor points to a command after the loop end (']')
 */
void Song_Manager::set_editor_position(const Editor_Position& d)
{
	editor_position = d;

	song_pos_at_cursor = UINT_MAX;
	song_pos_at_line = UINT_MAX;

	editor_refs.clear();
	editor_jump_hack = false;

	if(d.line != -1 && get_compile_result() == COMPILE_OK)
	{
		// Take ownership of the song and track info pointers.
		auto song = get_song();
		auto lines = get_lines();
		auto& line_map = (*lines.get())[d.line];

		for(auto && i : line_map)
		{
			Track& track = song->get_track(i.first);
			unsigned int position = i.second;
			unsigned int event_count = track.get_event_count();

			bool found_note_at_cursor = false;

			// Disable subroutine cursor hack if we are editing a line containing a subroutine.
			if(i.first > max_channels)
				editor_jump_hack = true;

			// Find reference to first event after the pointer.
			if(event_count > 0)
			{
				InputRef* refptr = nullptr;
				while(position-- > 0)
				{
					// This code doesn't look very nice and has some bugs. It would be good to rewrite it at some point in the future.
					auto event = track.get_event(position);
					auto ref = event.reference;
					unsigned int play_time = event.play_time;

					// Skip following if there is no reference. Reference pointer is needed to identify where a cursor should be drawn.
					if(ref == nullptr)
						continue;

					// Calculate the corrected play time. If the cursor is _after_ the note, add the note duration as well.
					// TODO: if the last event was a subroutine call, this value is not properly calculated. Need to find the position
					// of the last note in the subroutine call.
					if((signed)ref->get_line() < d.line)
						play_time += event.on_time + event.off_time;

					// If the play time is currently lowest recorded, set the "song position at beginning of line".
					// This variable will be correct since the loop continues until the first event of the line.
					if(play_time < song_pos_at_line)
						song_pos_at_line = play_time;

					// Do not save an InputRef* if we do not have a note, rest, tie or subroutine (jump) event.
					// TODO: A subroutine InputRef* will only work if said subroutine contains any notes. If not, then the following
					// InputRef* will not be found in any Track_Info event. We will need to add code to identify and skip such subroutines.
					if(event.type != Event::NOTE && event.type != Event::TIE && event.type != Event::REST && event.type != Event::JUMP)
						continue;

					// Only save the InputRef* for the event most adjacent to the cursor. Still need to continue the loop though, in order
					// to identify the play time of the first event of the line.
					if(!found_note_at_cursor)
						refptr = ref.get();

					int line = ref->get_line(), column = ref->get_column();
					if(line > d.line)
						continue;
					if(line == d.line && column > d.column)
						continue;

					// Calculate the corrected play time. If the cursor is _after_ the note, add the note duration as well.
					play_time = event.play_time;
					if((signed)ref->get_line() < d.line || (signed)ref->get_column() < d.column)
						play_time += event.on_time + event.off_time;

					if(!found_note_at_cursor && play_time < song_pos_at_cursor)
						song_pos_at_cursor = play_time;

					found_note_at_cursor = true;

					// Finally break the loop once we found an event at a previous line.
					if((signed)ref->get_line() < d.line)
						break;
				}
				if(refptr != nullptr)
					editor_refs.insert(refptr);
			}
		}
	}
	if(song_pos_at_cursor == UINT_MAX)
		song_pos_at_cursor = 0;
	if(song_pos_at_line == UINT_MAX)
		song_pos_at_line = 0;
}

//! worker thread
void Song_Manager::worker()
{
	std::unique_lock<std::mutex> lock(mutex);
	while(!worker_fired)
	{
		if(!job_done)
			compile_job(lock, job_buffer, job_filename);
		condition_variable.wait(lock);
	}
}

//! Compile job
void Song_Manager::compile_job(std::unique_lock<std::mutex>& lock, std::string buffer, std::string filename)
{
	lock.unlock();

	bool successful = false;
	std::shared_ptr<InputRef> ref = nullptr;
	std::shared_ptr<Song> temp_song = nullptr;
	std::shared_ptr<Track_Map> temp_tracks = nullptr;
	std::shared_ptr<Line_Map> temp_lines = nullptr;
	std::string str;
	std::string message;
	int line = 0;

	try
	{
		temp_song = std::make_shared<Song>();
		temp_tracks = std::make_shared<Track_Map>();
		temp_lines = std::make_shared<Line_Map>();

		int path_break = filename.find_last_of("/\\");
		if(path_break != -1)
			temp_song->add_tag("include_path", filename.substr(0, path_break + 1));

		MML_Input input = MML_Input(temp_song.get());

		// Read MML input line by line
		std::stringstream stream(buffer);
		for(; std::getline(stream, str);)
		{
			input.read_line(str, line);
			temp_lines.get()->insert({line, input.get_track_map()});
			line++;
		}

		// Generate track note lists.
		for(auto it = temp_song->get_track_map().begin(); it != temp_song->get_track_map().end(); it++)
		{
			// TODO: Max track count should be decided based on the target platform.
			if(it->first < max_channels)
				temp_tracks->emplace_hint(temp_tracks->end(),
					std::make_pair(it->first, Track_Info_Generator(*temp_song, it->second)));
		}

		successful = true;
		message = "";
	}
	catch (InputError& error)
	{
		ref = error.get_reference();
		message = error.what();
	}
	catch (std::exception& except)
	{
		ref = std::make_shared<InputRef>("", str, line, 0);
		message = "Exception: " + std::string(except.what());
	}

	lock.lock();
	job_done = true;
	job_successful = successful;
	song = temp_song;
	tracks = temp_tracks;
	lines = temp_lines;
	error_message = message;
	error_reference = ref;
}

