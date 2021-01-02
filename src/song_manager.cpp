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

//! Check if event is a note or subroutine call
static inline bool is_note_or_jump(Event::Type type)
{
	if(type == Event::NOTE || type == Event::TIE || type == Event::REST || type == Event::JUMP)
		return true;
	else
		return false;
}

//! Check if event is a loop end
static inline bool is_loop_event(Event::Type type)
{
	if(type == Event::LOOP_START || type == Event::LOOP_BREAK || type == Event::LOOP_END)
		return true;
	else
		return false;
}

//! Get the length of a loop section.
static unsigned int get_loop_length(Song& song, Track& track, unsigned int position, InputRef*& refptr, unsigned int max_recursion)
{
	// This function is a hack only needed in the case where a subroutine ends with a loop section.
	// Until I properly count the length of subroutines again, this will be necessary.
	// Example: passport.mml:177
	int depth = 0;
	int count = track.get_event(position).param - 1;
	int start_time = 0;
	int end_time = track.get_event(position).play_time;
	int break_time = 0;
	refptr = nullptr;
	while(position-- > 0)
	{
		auto event = track.get_event(position);
		start_time = event.play_time;
		if(is_note_or_jump(event.type) && refptr == nullptr)
		{
			refptr = event.reference.get();
		}
		else if(event.type == Event::LOOP_END)
		{
			if(refptr == nullptr)
				get_loop_length(song, track, position, refptr, max_recursion - 1);
			depth++;
		}
		else if(event.type == Event::LOOP_BREAK && !depth)
		{
			break_time = end_time - event.play_time;
			refptr = nullptr;
		}
		else if(event.type == Event::LOOP_START)
		{
			if(depth)
				depth--;
			else
				break;
		}
	}
	unsigned int result = (end_time - start_time) * count - break_time;
	return result;
}

//! Get the length of a subroutine.
static unsigned int get_subroutine_length(Song& song, unsigned int param, unsigned int max_recursion)
{
	try
	{
		Track& track = song.get_track(param);
		InputRef* dummy;
		if(track.get_event_count())
		{
			auto event = track.get_event(track.get_event_count() - 1);
			uint32_t end_time;
			if(event.type == Event::JUMP && max_recursion != 0)
				end_time = event.play_time + get_subroutine_length(song, event.param, max_recursion - 1);
			else if(event.type == Event::LOOP_END && max_recursion != 0)
				end_time = event.play_time + get_loop_length(song, track, track.get_event_count() - 1, dummy, max_recursion - 1);
			else
				end_time = event.play_time + event.on_time + event.off_time;
			return end_time - track.get_event(0).play_time;
		}
	}
	catch(std::exception &e)
	{
	}
	return 0;
}

//! Set the current editor position, and find any events adjacent to the editor cursor.
/*!
 *  Call this function from the UI thread.
 *
 *  Cursor should be displayed if an InputRef* from editor_refs can be also found in the Track_Info array (tracks).
 *
 *  This approach is not perfect, there are a few things that need to be looked into.
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

			// Disable subroutine cursor hack if we are editing a line containing a subroutine.
			if(i.first > max_channels)
				editor_jump_hack = true;

			// Find the reference to the note adjacent to the note, and the song playtime at the
			// beginning of the line and at the cursor.
			if(event_count > 0)
			{
				InputRef* refptr = nullptr;

				// Backtrack until we find an event adjacent to the cursor
				while(position-- > 0)
				{
					auto ref = track.get_event(position).reference;
					if(ref != nullptr)
					{
						int line = ref->get_line(), column = ref->get_column();
						if((line < d.line) || (line == d.line && column < d.column))
							break;
					}
				}

				// Select the adjacent note / rest / tie event right of the cursor
				while(++position < event_count)
				{
					auto event = track.get_event(position);

					if(is_note_or_jump(event.type))
					{
						refptr = event.reference.get();
						if(event.play_time < song_pos_at_cursor)
							song_pos_at_cursor = event.play_time;
						break;
					}
					else if(is_loop_event(event.type))
					{
						break;
					}
				}

				// Nothing was found, select the adjacent event left of cursor.
				// Also scan for the first event at the line.
				bool passed_line = false;
				while((!passed_line || refptr == nullptr) && position-- > 0)
				{
					auto event = track.get_event(position);
					uint32_t length = event.on_time + event.off_time;
					InputRef* loop_refptr;

					if(event.type == Event::JUMP)
						length = get_subroutine_length(*song, event.param, 10);
					else if(event.type == Event::LOOP_END)
						length = get_loop_length(*song, track, position, loop_refptr, 10);

					auto ref = track.get_event(position).reference;
					if(ref != nullptr)
					{
						passed_line = ((signed)ref->get_line() < d.line);
						if(!passed_line && event.play_time < song_pos_at_line)
							song_pos_at_line = event.play_time;
					}

					if(refptr == nullptr && length != 0 && is_note_or_jump(event.type))
					{
						refptr = event.reference.get();
						if((event.play_time + length) < song_pos_at_cursor)
							song_pos_at_cursor = event.play_time + length;
					}
					else if(refptr == nullptr && length != 0 && event.type == Event::LOOP_END)
					{
						refptr = loop_refptr;
						if((event.play_time + length) < song_pos_at_cursor)
							song_pos_at_cursor = event.play_time + length;
					}
				}

				if(song_pos_at_cursor < song_pos_at_line)
					song_pos_at_line = song_pos_at_cursor;

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
			input.read_line(tabs_to_spaces(str), line);
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

//! Convert all tabs to spaces in a string.
/*!
 *  Currently tabstop is hardcoded to 4 to match the editor.
 */
std::string Song_Manager::tabs_to_spaces(const std::string& str) const
{
	const unsigned int tabstop = 4;
	std::string out = "";
	for(char i : str)
	{
		if(i == '\t')
		{
			do
			{
				out.push_back(' ');
			}
			while(out.size() % tabstop != 0);
		}
		else
		{
			out.push_back(i);
		}
	}
	return out;
}

