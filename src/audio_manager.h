#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <mutex>

#include <vgm/audio/AudioStream.h>
#include <vgm/emu/Resampler.h>

//! Abstract class for audio stream control
class Audio_Stream
{
	public:
		inline Audio_Stream()
			: finished(false)
		{}

		inline virtual ~Audio_Stream()
		{}

		//! called by Audio_Manager when starting the stream.
		/*!
		 *  setup your resamplers and stuff here.
		 */
		virtual void setup_stream(uint32_t sample_rate) = 0;

		//! called by Audio_Manager during stream update.
		/*!
		 *  return zero to indicate that the stream should be stopped.
		 */
		virtual int get_sample(WAVE_32BS* output, int count, int channels) = 0;

		//! called by Audio_Manager when stopping the stream.
		/*!
		 *  resamplers should be cleaned up, but the playback may start again
		 *  so the "finished" state should not be updated here.
		 */
		virtual void stop_stream() = 0;

		//! get the "finished" flag status
		/*!
		 *  when set, the audio manager will stop mixing this stream and
		 *  destroy its pointer.
		 */
		inline void set_finished(bool flag)
		{
			finished = true;
		}

		//! get the "finished" flag status
		/*!
		 *  when set, the audio manager will stop mixing this stream and
		 *  destroy its pointer.
		 */
		inline bool get_finished()
		{
			return finished;
		}

	protected:
		bool finished;
};

//! Audio manager class
/*!
 *  Please note that initialization/deinitialization of the libvgm audio library 
 *  (Audio_Init() and Audio_Deinit() should be done in the main function first.
 */
class Audio_Manager
{
	public:
		// singleton guard
		Audio_Manager(Audio_Manager const&) = delete;
		void operator=(Audio_Manager const&) = delete;

		static Audio_Manager& get();

		bool get_audio_enabled() const;

		void set_window_handle(void* new_handle);

		int set_sample_rate(uint32_t new_sample_rate);
		void set_volume(float new_volume);
		float get_volume() const;

		void set_driver(int new_driver_sig, int new_device_id = -1);
		inline int get_driver() const { return driver_sig; };

		void set_device(int new_device_id);
		inline int get_device() const { return device_id; };

		int add_stream(std::shared_ptr<Audio_Stream> stream);

		const std::map<int, std::pair<int,std::string>>& get_driver_list() const { return driver_list; }
		const std::map<int, std::string>& get_device_list() const { return device_list; }

		void clean_up();

	private:
		Audio_Manager();

		void enumerate_drivers();
		void enumerate_devices();

		int open_driver();
		void close_driver();

		int open_device();
		void close_device();

		static int16_t clip16(int32_t input);

		static uint32_t callback(void* drv_struct, void* user_param, uint32_t buf_size, void* data);

		int driver_sig; // Actual driver signature, -1 if not loaded
		int driver_id;  // Actual driver id, -1 if not loaded
		int device_id;  // Actual device id, -1 if not loaded

		uint32_t sample_rate;
		uint32_t sample_size;

		float volume;
		int32_t converted_volume;
		std::vector<std::shared_ptr<Audio_Stream>> streams;

		void* window_handle;
		void* driver_handle;

		bool waiting_for_handle;
		bool audio_initialized;
		bool driver_opened;
		bool device_opened;

		std::map<int, std::pair<int,std::string>> driver_list;
		std::map<int, std::string> device_list;

		std::mutex mutex;
};

#endif
