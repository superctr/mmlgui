#include "audio_manager.h"
#include <stdio.h>

//! First time initialization
Audio_Manager::Audio_Manager()
	: audio_enabled(0)
	, sample_rate(44100)
	, volume(1.0)
	, converted_volume(0x10000)
{
	if (Audio_Init())
	{
		fprintf(stderr, "Warning: audio initialization failed. Muting audio");
		set_audio_enabled(0);
	}
	else
	{
		set_audio_enabled(1);
	}

}

//! get the singleton instance of Audio_Manager
Audio_Manager& Audio_Manager::get()
{
	static Audio_Manager instance;
	return instance;
}

//! Set global volume
void Audio_Manager::set_audio_enabled(bool flag)
{
	audio_enabled = flag;
}

//! Get global volume
bool Audio_Manager::get_audio_enabled() const
{
	return audio_enabled;
}
	
//! Set global sample rate.
/*!
 *  This function should re-initialize the streams with the new sample rate, if needed.
 *  (Using Audio_Stream::stop_stream() and Audio_Stream::start_stream())
 *
 *  \return zero if successful, non-zero otherwise.
 */
int Audio_Manager::set_sample_rate(uint32_t new_sample_rate)
{
	sample_rate = new_sample_rate;
	return 0;
}

//! Set global volume
void Audio_Manager::set_volume(float new_volume)
{
	volume = volume;
}

//! Get global volume
float Audio_Manager::get_volume() const
{
	return volume;
}

//! Add an audio stream
int Audio_Manager::add_stream(std::shared_ptr<Audio_Stream>& stream)
{
	return -1;
}

//! Kill all streams.
void Audio_Manager::Audio_Manager::clean_up()
{
	if(audio_enabled)
		Audio_Deinit();
}
