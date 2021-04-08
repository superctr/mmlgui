#include "audio_manager.h"

// for debug output
#include <stdio.h>
#include <typeinfo>

#include <cstring>

#include <vgm/audio/AudioStream.h>
#include <vgm/audio/AudioStream_SpcDrvFuns.h>

//! First time initialization
Audio_Manager::Audio_Manager()
	: driver_sig(-1)
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
	, audio_initialized(false)
	, driver_opened(false)
	, device_opened(false)
	, driver_list()
	, device_list()
{
	if (Audio_Init())
	{
		fprintf(stderr, "Warning: audio initialization failed. Muting audio\n");
	}
	else
	{
		audio_initialized = true;
		enumerate_drivers();
	}
}

//! get the singleton instance of Audio_Manager
Audio_Manager& Audio_Manager::get()
{
	static Audio_Manager instance;
	return instance;
}

void Audio_Manager::set_driver(int new_driver_sig, int new_device_id)
{
	std::string name = "";

	if(!audio_initialized)
		return;

	// Reset device to default if we have already opened a device
	device_id = new_device_id;

	close_driver();

	// Find a driver ID
	driver_sig = new_driver_sig;
	driver_id = -1;
	if(driver_sig != -1)
	{
		auto it = driver_list.find(driver_sig);

		if(it != driver_list.end())
		{
			driver_id = it->second.first;
			name = it->second.second;
			open_driver();
		}
		else
		{
			fprintf(stderr, "Warning: Driver id %02x unavailable, using default\n", driver_sig);
		}
	}

	if(driver_id == -1)
	{
		// pick the next one automatically if loading fails
		for(auto && i : driver_list)
		{
			driver_sig = i.first;
			driver_id = i.second.first;
			name = i.second.second;
			if(!open_driver())
				break;
		}
	}

	if(driver_opened)
	{
		fprintf(stdout, "Loaded audio driver: '%s'\n", name.c_str());
		enumerate_devices();
		set_device(device_id);
	}
	else
	{
		if(driver_id == -1)
			fprintf(stderr, "Warning: No available drivers\n");
		else
			fprintf(stderr, "Warning: Failed to open driver '%s'\n", name.c_str());
	}
}

void Audio_Manager::set_device(int new_device_id)
{
	std::string name = "";

	if(!driver_opened)
		return;

	close_device();

	// Find a driver ID
	device_id = -1;
	if(new_device_id != -1)
	{
		auto it = device_list.find(new_device_id);

		if(it != device_list.end())
		{
			device_id = it->first;
			name = it->second;
			open_device();
		}
		else
		{
			fprintf(stderr, "Warning: Device id %02x unavailable, using default\n", new_device_id);
		}
	}

	if(device_id == -1)
	{
		// pick the next one automatically if loading fails
		for(auto && i : device_list)
		{
			device_id = i.first;
			name = i.second;
			if(!open_device())
				break;
		}
	}

	if(!device_opened)
	{
		if(device_id == -1)
			fprintf(stderr, "Warning: No available devices\n");
		else
			fprintf(stderr, "Warning: Failed to open device '%s'\n", name.c_str());
	}
	else
	{
		fprintf(stdout, "Loaded audio device: '%s'\n", name.c_str());
	}
}

//! Get global volume
bool Audio_Manager::get_audio_enabled() const
{
	return audio_initialized && driver_opened && device_opened;
}

//! Set window handle (for APIs where this is required)
void Audio_Manager::set_window_handle(void* new_handle)
{
	window_handle = new_handle;
	if(waiting_for_handle)
	{
		if(!open_driver())
			waiting_for_handle = false;
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
	if(audio_initialized)
		Audio_Deinit();
}

//=====================================================================

void Audio_Manager::enumerate_drivers()
{
	driver_list.clear();

	unsigned int driver_count = Audio_GetDriverCount();
	if(!driver_count)
	{
		fprintf(stderr, "Warning: no audio drivers available. Muting audio\n");
	}
	else
	{
		printf("Available audio drivers ...\n");
		for(unsigned int i = 0; i < driver_count; i++)
		{
			AUDDRV_INFO* info;
			Audio_GetDriverInfo(i, &info);

			if(info->drvType == ADRVTYPE_OUT)
			{
				printf("%d = '%s' (type = %02x, sig = %02x)\n", i, info->drvName, info->drvType, info->drvSig);
				driver_list[info->drvSig] = std::make_pair<int, std::string>(i, info->drvName);
			}
		}
	}
}

void Audio_Manager::enumerate_devices()
{
	if(!driver_opened)
		return;

	device_list.clear();

	const AUDIO_DEV_LIST* list = AudioDrv_GetDeviceList(driver_handle);
	if(!list->devCount)
	{
		fprintf(stderr, "Warning: no audio devices available. Muting audio\n");
	}
	else
	{
		printf("Available audio devices ...\n");
		for(unsigned int i = 0; i < list->devCount; i++)
		{
			AUDDRV_INFO* info;
			Audio_GetDriverInfo(i, &info);

			if(info->drvType)
			{
				printf("%d = '%s'\n", i, list->devNames[i]);
				device_list[i] = list->devNames[i];
			}
		}
	}
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

#ifdef AUDDRV_DSOUND
	if(info->drvSig == ADRVSIG_DSOUND && window_handle)
	{
		void* dsoundDrv;
		dsoundDrv = AudioDrv_GetDrvData(driver_handle);
		DSound_SetHWnd(dsoundDrv, (HWND)window_handle);
	}
#endif

#ifdef AUDDRV_PULSE
	if(info->drvSig == ADRVSIG_PULSE)
	{
		void* pulseDrv;
		pulseDrv = AudioDrv_GetDrvData(driver_handle);
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
	// libvgm may halt if we lock our own mutex before calling this. (ALSA)
	AudioDrv_Stop(driver_handle);
	device_opened = false;
}

inline int16_t Audio_Manager::clip16(int32_t input)
{
	if(input > 32767)
		input = 32767;
	else if(input < -32768)
		input = -32768;
	return input;
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
			int16_t* sd = (int16_t*) data;
			for(int i = 0; i < sample_count; i ++)
			{
				int32_t l = buffer[i].L >> 8;
				int32_t r = buffer[i].R >> 8;
				*sd++ = clip16((l * am.converted_volume) >> 8);
				*sd++ = clip16((r * am.converted_volume) >> 8);
			}
			return sample_count * am.sample_size;
		}
		default:
		{
			return 0;
		}
	}
}

