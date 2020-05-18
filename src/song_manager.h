#ifndef SONG_MANAGER_H
#define SONG_MANAGER_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>

#include "core.h"
#include "input.h"

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

		Song_Manager();
		virtual ~Song_Manager();

		Compile_Result get_compile_result();
		bool get_compile_in_progress();

		int compile(const std::string& buffer, const std::string& filename);

		std::shared_ptr<Song> get_song();
		std::string get_error_message();

	private:
		void worker();
		void compile_job(std::unique_lock<std::mutex>& lock, std::string buffer, std::string filename);

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
		std::shared_ptr<std::map<int,Track_Info>> tracks;
		std::string error_message;
		std::shared_ptr<InputRef> error_reference;
};

#endif