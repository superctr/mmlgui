#include "audio_manager.h"

// for debug output
#include <stdio.h>
#include <typeinfo>

#include <cstring>

#include <vgm/audio/AudioStream.h>
#include <vgm/audio/AudioStream_SpcDrvFuns.h>

//! First time initialization
Audio_Manager::Audio_Manager()
	: audio_enabled(0)
	, driver_id(-1)
	, device_id(-1)
	, sample_rate(44100)
	, sample_size(4)
	, volume(1.0)
	, converted_volume(0x100)
	, streams()
	, window_handle(nullptr)
	, driver_handle(nullptr)
	, waiting_for_handle(false)
	, driver_opened(false)
	, device_opened(false)
	, driver_names()
	, device_names()
{
	if (Audio_Init())
	{
		fprintf(stderr, "Warning: audio initialization failed. Muting audio\n");
		set_audio_enabled(false);
	}
	else
	{
		set_audio_enabled(true);
	}

	enumerate_drivers();
	if(open_driver())
	{
		set_audio_enabled(false);
	}
	if(driver_opened)
	{
		open_device();
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

//! Set window handle (for APIs where this is required)
void Audio_Manager::set_window_handle(void* new_handle)
{
	window_handle = new_handle;
	if(waiting_for_handle)
	{
		waiting_for_handle = false;
		if(open_driver())
			set_audio_enabled(false);
	}
	if(driver_opened && !device_opened)
	{
		open_device();
	}
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
	// TODO: reinit streams
	std::lock_guard<std::mutex> lock(mutex);
	sample_rate = new_sample_rate;
	return 0;
}

//! Set global volume
void Audio_Manager::set_volume(float new_volume)
{
	volume = volume;
	converted_volume = volume * 256.0;
}

//! Get global volume
float Audio_Manager::get_volume() const
{
	return volume;
}

//! Add an audio stream
int Audio_Manager::add_stream(std::shared_ptr<Audio_Stream> stream)
{
	std::lock_guard<std::mutex> lock(mutex);
	stream->setup_stream(sample_rate);
	printf("Adding stream %s\n", typeid(*stream).name());
	streams.push_back(stream);
	return 0;
}

//! Kill all streams and close audio system
void Audio_Manager::clean_up()
{
	close_driver();
	if(audio_enabled)
		Audio_Deinit();
}

//=====================================================================

void Audio_Manager::enumerate_drivers()
{
	unsigned int driver_count = Audio_GetDriverCount();
	if(!driver_count)
	{
		fprintf(stderr, "Warning: no audio drivers available. Muting audio\n");
		set_audio_enabled(0);
		return;
	}

	printf("Available audio drivers ...\n");
	for(unsigned int i = 0; i < driver_count; i++)
	{
		AUDDRV_INFO* info;
		Audio_GetDriverInfo(i, &info);

		if(info->drvType)
		{
			printf("%d = '%s' (type = %02x, sig = %02x)\n", i, info->drvName, info->drvType, info->drvSig);
			driver_names[i] = info->drvName;
		}

		// Select the first available non-logging driver.
		if(info->drvType == ADRVTYPE_OUT && driver_id == -1)
		{
			driver_id = i;
		}
	}
	printf("default = %d\n", driver_id);
}

void Audio_Manager::enumerate_devices()
{
	if(driver_opened)
		return;

	const AUDIO_DEV_LIST* list;
	list = AudioDrv_GetDeviceList(driver_handle);

	if(!list->devCount)
	{
		fprintf(stderr, "Warning: no audio devices available. Muting audio\n");
		set_audio_enabled(0);
		return;
	}

	printf("Available audio devices ...\n");
	for(unsigned int i = 0; i < list->devCount; i++)
	{
		AUDDRV_INFO* info;
		Audio_GetDriverInfo(i, &info);

		if(info->drvType)
		{
			printf("%d = '%s'\n", i, list->devNames[i]);
			device_names[i] = list->devNames[i];
		}

		// Select the first available device
		if(device_id == -1)
		{
			device_id = i;
		}
	}
	printf("default = %d\n", device_id);
}

//! Open a driver
int Audio_Manager::open_driver()
{
	AUDDRV_INFO* info;
	Audio_GetDriverInfo(driver_id, &info);

#ifdef AUDDRV_DSOUND
	if(info->drvSig == ADRVSIG_DSOUND && !window_handle)
	{
		printf("Please give me a HWND first\n");
		waiting_for_handle = true;
		return 0;
	}
#endif

	uint8_t error_code;
	error_code = AudioDrv_Init(driver_id, &driver_handle);
	if(error_code)
	{
		printf("AudioDrv_Init error %02x\n", error_code);
		return -1;
	}

#ifdef AUDDRV_PULSE
	if(info->drvSig == ADRVSIG_PULSE)
	{
		void* pulseDrv;
		pulseDrv = AudioDrv_GetDrvData(audDrv);
		Pulse_SetStreamDesc(pulseDrv, "mmlgui");
	}
#endif

	enumerate_devices();
	driver_opened = true;
	return 0;
}

void Audio_Manager::close_driver()
{
	if(device_opened)
		close_device();
	if(driver_opened)
		AudioDrv_Deinit(&driver_handle);
}

int Audio_Manager::open_device()
{
	if(device_opened)
		return -1;
	const std::lock_guard<std::mutex> lock(mutex);
	auto opts = AudioDrv_GetOptions(driver_handle);
	opts->numChannels = 2;
	opts->numBitsPerSmpl = 16;
	sample_size = opts->numChannels * opts->numBitsPerSmpl / 8;
	AudioDrv_SetCallback(driver_handle, Audio_Manager::callback, NULL);
	int error_code = AudioDrv_Start(driver_handle, device_id);
	if(error_code)
	{
		printf("AudioDrv_Start error %02x\n", error_code);
		return -1;
	}
	device_opened = true;
	return 0;
}

void Audio_Manager::close_device()
{
	const std::lock_guard<std::mutex> lock(mutex);
	AudioDrv_Stop(driver_handle);
	device_opened = false;
}

uint32_t Audio_Manager::callback(void* drv_struct, void* user_param, uint32_t buf_size, void* data)
{
	Audio_Manager& am = Audio_Manager::get();
	int sample_count = buf_size / am.sample_size;
	std::vector<WAVE_32BS> buffer(sample_count, {0, 0});
	const std::lock_guard<std::mutex> lock(am.mutex);

	// Input buffer
	for(auto stream = am.streams.begin(); stream != am.streams.end();)
	{
		Audio_Stream* s = stream->get();
		s->get_sample(buffer.data(), sample_count, 2);
		if(s->get_finished())
		{
			printf("Removing stream %s\n", typeid(*s).name());
			s->stop_stream();
			am.streams.erase(stream);
		}
		else
		{
			stream++;
		}
	}

	// Output buffer
	switch(am.sample_size)
	{
		case 4:
		{
			uint16_t* sd = (uint16_t*) data;
			for(int i = 0; i < sample_count; i ++)
			{
				uint32_t l = buffer[i].L >> 8;
				uint32_t r = buffer[i].R >> 8;
				*sd++ = (l * am.converted_volume) >> 8;
				*sd++ = (r * am.converted_volume) >> 8;
			}
			return sample_count * am.sample_size;
		}
		default:
		{
			return 0;
		}
	}
}

