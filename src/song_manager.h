#ifndef SONG_MANAGER_H
#define SONG_MANAGER_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <unordered_set>
#include <set>

#include "core.h"
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
		void play();
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

	private:
		void worker();
		void compile_job(std::unique_lock<std::mutex>& lock, std::string buffer, std::string filename);

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
};

#endif
