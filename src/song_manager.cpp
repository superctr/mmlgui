#include "song_manager.h"
#include "song.h"
#include "input.h"
#include "player.h"

#include "mml_input.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <sstream>

//! constructs a Song_Manager
Song_Manager::Song_Manager()
	: worker_ptr(nullptr)
	, worker_fired(false)
	, job_done(false)
	, job_successful(false)
	, song(nullptr)
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

//! Get song data
std::shared_ptr<Song> Song_Manager::get_song()
{
	std::lock_guard<std::mutex> guard(mutex);
	return song;
}

//! Get error message
std::string Song_Manager::get_error_message()
{
	std::lock_guard<std::mutex> guard(mutex);
	return error_message;
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
	std::string str;
	std::string message;
	int line = 0;

	try
	{
		temp_song = std::make_shared<Song>();

		int path_break = filename.find_last_of("/\\");
		if(path_break != -1)
			song->add_tag("include_path", filename.substr(0, path_break + 1));

		MML_Input input = MML_Input(temp_song.get());

		// Read line by line
		std::stringstream stream(buffer);
		for(; std::getline(stream, str);)
		{
			input.read_line(str, line);
			line++;
		}

		// try to run Song_Validator (to be replaced in future with a custom ...)
		auto validator = Song_Validator(*temp_song);

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
	error_message = message;
	error_reference = ref;
}

