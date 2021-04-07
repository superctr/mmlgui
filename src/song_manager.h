#ifndef SONG_MANAGER_H
#define SONG_MANAGER_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <unordered_set>
#include <set>
#include <map>

#include "core.h"
#include "song.h"
#include "input.h"
#include "mml_input.h"

#include "audio_manager.h"
#include "emu_player.h"

struct Track_Info;

class Song_Manager
{
	public:
		enum Compile_Result
		{
			COMPILE_NOT_DONE = -1,			 // also used to indicate that a compile is in progress
			COMPILE_OK = 0,
			COMPILE_ERROR = 1
		};

		typedef std::map<int, Track_Info> Track_Map;
		typedef std::map<int, MML_Input::Track_Position_Map> Line_Map;
		typedef std::set<InputRef*> Ref_Ptr_Set;

		typedef struct
		{
			int line;
			int column;
		} Editor_Position;

		Song_Manager();
		virtual ~Song_Manager();

		Compile_Result get_compile_result();
		bool get_compile_in_progress();

		int compile(const std::string& buffer, const std::string& filename);
		void play(uint32_t start_position = 0);
		void stop();

		std::shared_ptr<Song> get_song();
		std::shared_ptr<Emu_Player> get_player();
		std::shared_ptr<Track_Map> get_tracks();
		std::shared_ptr<Line_Map> get_lines();
		std::string get_error_message();

		void set_editor_position(const Editor_Position& d);

		//! Get the current editor position. Used to display cursors.
		inline const Editor_Position& get_editor_position() const { return editor_position; }

		//! Get a set of references pointers at the editor position. Note that the pointers may be invalid.
		inline const Ref_Ptr_Set& get_editor_refs() const { return editor_refs; }

		//! Return true if editor position points to a subroutine. Used to enable a hack.
		inline bool get_editor_subroutine() const { return editor_jump_hack; }

		//! Get the song playtime at the beginning of line pointed at by the editor.
		inline uint32_t get_song_pos_at_line() const { return song_pos_at_line; }

		//! Get the song playtime at the cursor pointed at by the editor.
		inline uint32_t get_song_pos_at_cursor() const { return song_pos_at_cursor; }

		std::pair<int16_t,uint32_t> get_channel(uint16_t track) const;

		void toggle_mute(uint16_t track_id);
		void toggle_solo(uint16_t track_id);

		bool get_mute(uint16_t track_id) const;
		bool get_solo(uint16_t track_id) const;

		void reset_mute();

	private:
		void worker();
		void compile_job(std::unique_lock<std::mutex>& lock, std::string buffer, std::string filename);
		std::string tabs_to_spaces(const std::string& str) const;
		void update_mute();

		// song status
		const static int max_channels;

		// worker state
		std::mutex mutex;
		std::condition_variable condition_variable;
		std::unique_ptr<std::thread> worker_ptr;

		// worker status
		bool worker_fired;	// set to 1 to kill worker thread
		bool job_done;		// set to 0 to begin compile
		bool job_successful;

		// worker input
		std::string job_buffer;
		std::string job_filename;

		// worker output
		std::shared_ptr<Song> song;
		std::shared_ptr<Track_Map> tracks;
		std::shared_ptr<Line_Map> lines;
		std::string error_message;
		std::shared_ptr<InputRef> error_reference;

		// playback state
		std::shared_ptr<Emu_Player> player;

		// editor state
		Editor_Position editor_position;
		Ref_Ptr_Set editor_refs;
		bool editor_jump_hack;

		unsigned int song_pos_at_line;
		unsigned int song_pos_at_cursor;

		// muting
		std::map<int16_t, uint32_t> mute_mask; // Chip_id, channel_id
		const static std::map<uint16_t, std::pair<int16_t, uint32_t>> track_channel_table; // Track to channel ID table
};

#endif
