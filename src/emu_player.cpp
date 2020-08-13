#include "emu_player.h"
#include "platform/md.h"
#include "input.h"

#include <vgm/emu/EmuStructs.h>
#include <vgm/emu/SoundEmu.h>
#include <vgm/emu/SoundDevs.h>
#include <vgm/emu/EmuCores.h>
#include <vgm/emu/cores/sn764intf.h>

#include <cstdlib>
#include <stdexcept>
#include <algorithm>

using std::malloc;
using std::free;

Device_Wrapper::Device_Wrapper()
	: dev_init(false)
	, resmpl_init(false)
	, write_type(Device_Wrapper::NONE)
	, volume(0x100)
	, write_a8d8(nullptr)
{
}

Device_Wrapper::~Device_Wrapper()
{
	if(resmpl_init)
	{
		printf("deinit resampler\n");
		// deinit
		Resmpl_Deinit(&resmpl);
	}
	if(dev_init)
	{
		printf("deinit emu\n");
		// deinit
		SndEmu_Stop(&dev);
	}

}

void Device_Wrapper::set_default_volume(uint16_t vol)
{
	volume = vol;
}

void Device_Wrapper::set_rate(uint32_t rate)
{
	sample_rate = rate;

	if(resmpl_init)
	{
		printf("deinit resampler\n");
		Resmpl_Deinit(&resmpl);
		resmpl_init = false;
	}

	if(dev_init)
	{
		printf("init resampler\n");
		Resmpl_SetVals(&resmpl, 0xff, volume, sample_rate);
		Resmpl_DevConnect(&resmpl, &dev);
		Resmpl_Init(&resmpl);
		resmpl_init = true;
	}
}

void Device_Wrapper::init_sn76489(uint32_t freq, uint8_t lfsr_w, uint16_t lfsr_t)
{
	printf("init emu (sn76489 @ %d Hz)\n", freq);
	DEV_GEN_CFG dev_cfg;
	SN76496_CFG sn_cfg;

	dev_cfg.emuCore = 0;
	dev_cfg.srMode = DEVRI_SRMODE_NATIVE;
	dev_cfg.flags = 0x00;
	dev_cfg.clock = freq;
	dev_cfg.smplRate = 44100;
	sn_cfg._genCfg = dev_cfg;
	sn_cfg.shiftRegWidth = lfsr_w;
	sn_cfg.noiseTaps = lfsr_t;
	sn_cfg.negate = 0;
	sn_cfg.stereo = 0;
	sn_cfg.clkDiv = 8;
	sn_cfg.segaPSG = 1; //???
	sn_cfg.t6w28_tone = NULL;

	uint8_t status = SndEmu_Start(DEVID_SN76496, (DEV_GEN_CFG*)&sn_cfg, &dev);
	if(status)
		throw std::runtime_error("Device_Wrapper::init_sn76489");

	SndEmu_GetDeviceFunc(dev.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&write_a8d8);
	write_type = Device_Wrapper::A8D8;

	dev.devDef->Reset(dev.dataPtr);

	dev_init = true;
}

void Device_Wrapper::init_ym2612(uint32_t freq)
{
	printf("init emu (ym2612 @ %d Hz)\n", freq);
	DEV_GEN_CFG dev_cfg;

	dev_cfg.emuCore = 0;
	dev_cfg.srMode = DEVRI_SRMODE_NATIVE;
	dev_cfg.flags = 0x00;
	dev_cfg.clock = freq;
	dev_cfg.smplRate = 44100;

	uint8_t status = SndEmu_Start(DEVID_YM2612, (DEV_GEN_CFG*)&dev_cfg, &dev);
	if(status)
		throw std::runtime_error("Device_Wrapper::init_ym2612");

	SndEmu_GetDeviceFunc(dev.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&write_a8d8);
	write_type = Device_Wrapper::P1A8D8;

	dev.devDef->Reset(dev.dataPtr);

	dev_init = true;
}

inline void Device_Wrapper::write(uint16_t addr, uint16_t data)
{
	switch(write_type)
	{
		default:
			break;
		case Device_Wrapper::A8D8:
			write_a8d8(dev.dataPtr, addr, data);
			break;
	}
}

inline void Device_Wrapper::write(uint8_t port, uint16_t addr, uint16_t data)
{
	switch(write_type)
	{
		default:
			write(addr, data);
			break;
		case Device_Wrapper::P1A8D8:
			write_a8d8(dev.dataPtr, (port << 1), addr);
			write_a8d8(dev.dataPtr, (port << 1) + 1, data);
			break;
	}
}

inline void Device_Wrapper::get_sample(WAVE_32BS* output, int count)
{
	if(resmpl_init)
		Resmpl_Execute(&resmpl, count, output);
}

//=====================================================================

Emu_Player::Emu_Player(std::shared_ptr<Song> song)
	: sample_rate(1)
	, delta_time(0)
	, sample_delta(1)
	, play_time(0)
	, play_time2(0)
	, song(song)
{
	printf("Emu_Player created\n");

	// TODO: Hardcoded driver type
	driver = std::make_shared<MD_Driver>(1, (VGM_Interface*)this);
	driver.get()->play_song(*song.get());
}

Emu_Player::~Emu_Player()
{
	printf("Emu_Player destroyed\n");
}

//=====================================================================
// Audio_Stream (front end)
// Audio_Manager -> Emu_Player
//=====================================================================

void Emu_Player::setup_stream(uint32_t sample_rate)
{
	if(sample_rate == 0)
		sample_rate = 1;
	this->sample_rate = sample_rate;
	sample_delta = 1.0 / sample_rate;
	delta_time = 0;
	printf("Emu_Player stream setup %d Hz, delta = %.8f, init = %.8f\n", sample_rate, sample_delta, delta_time);

	for(auto it = devices.begin(); it != devices.end(); it++)
	{
		it->second.set_rate(sample_rate);
	}
}

int Emu_Player::get_sample(WAVE_32BS* output, int count, int channels)
{
	try
	{
		for(int i = 0; i < count; i++)
		{
			int max_steps = 100;
			delta_time += sample_delta;
			play_time += sample_delta;

			if(delta_time > 0)
			{
				//printf("\n%.8f,%.8f=%.8f ", play_time, play_time-play_time2, delta_time);
				play_time2 = play_time;
			}

			while(delta_time > 0)
			{
				double step = driver.get()->play_step();
				delta_time -= step;

				//printf("-%.8f, ", step);
				if(!--max_steps)
					break;
			}

			// Update any dac streams
			for(auto && it = streams.begin(); it != streams.end(); it++)
			{
				if(it->second.active)
				{
					it->second.counter += it->second.freq;
					while(it->second.counter >= sample_rate)
					{
						devices[it->second.chip_id].write(
								it->second.port,
								it->second.reg,
								datablocks[it->second.db_id][it->second.position]);
						it->second.position ++;
						it->second.counter -= sample_rate;
						if(!--it->second.length)
						{
							it->second.active = false;
						}
					}
				}
			}

			// Get sample from sound chips
			for(auto && it = devices.begin(); it != devices.end(); it++)
			{
				it->second.get_sample(&output[i], 1);
			}

			if(!driver.get()->is_playing())
				set_finished(true);
		}
	}
	catch(InputError& e)
	{
		handle_error(e.what());
	}
	catch(std::exception& e)
	{
		handle_error(e.what());
	}
	return count;
}

void Emu_Player::stop_stream()
{
	printf("Emu_Player stream stop\n");
}

void Emu_Player::handle_error(const char* str)
{
	printf("Playback error: %s\n", str);
	delta_time = -1000; // Prevent error from reoccuring
	set_finished(true);
}

//=====================================================================
// VGM_Interface (back end)
// Driver -> Emu_Player
//=====================================================================

void Emu_Player::write(uint8_t command, uint16_t port, uint16_t reg, uint16_t data)
{
	switch(command)
	{
		case 0x50:
			devices[DEVID_SN76496].write(reg, data);
			break;
		case 0x52:
			devices[DEVID_YM2612].write(port, reg, data);
		default:
			break;
	}
}

void Emu_Player::dac_setup(uint8_t sid, uint8_t chip_id, uint32_t port, uint32_t reg, uint8_t db_id)
{
	//printf("Emu_Player setup stream %02x = %02x,%02x,%02x,%02x\n", sid, chip_id, port, reg, db_id);
	streams[sid].chip_id = chip_id;
	streams[sid].port = port;
	streams[sid].reg = reg;
	streams[sid].db_id = db_id;
	streams[sid].active = false;
}

void Emu_Player::dac_start(uint8_t sid, uint32_t start, uint32_t length, uint32_t freq)
{
	//printf("Emu_Player start stream %02x = %d,%d,%d\n", sid, start,length,freq);
	streams[sid].position = start;
	streams[sid].length = length;
	streams[sid].freq = freq;
	streams[sid].counter = sample_rate;
	streams[sid].active = true;
}

void Emu_Player::dac_stop(uint8_t sid)
{
	//printf("Emu_Player stop stream %02x\n", sid);
	streams[sid].active = false;
}

//! Initialize sound chip.
void Emu_Player::poke32(uint32_t offset, uint32_t data)
{
	uint32_t clock = data & 0x3fffffff;
	bool dual_chip = data >> 30;
	bool extra_flag = data >> 31;
	switch(offset)
	{
		case 0x0c:
			devices[DEVID_SN76496].set_default_volume(0x80);
			devices[DEVID_SN76496].init_sn76489(clock);
			break;
		case 0x2c:
			devices[DEVID_YM2612].init_ym2612(clock);
			break;
		default:
			printf("Emu_Player poke %02x = %08x\n", offset, data);
			break;
	}
}

void Emu_Player::poke16(uint32_t offset, uint16_t data)
{
	printf("Emu_Player poke %02x = %04x\n", offset, data);
}

void Emu_Player::poke8(uint32_t offset, uint8_t data)
{
	printf("Emu_Player poke %02x = %02x\n", offset, data);
}

void Emu_Player::set_loop()
{
	printf("Emu_Player set loop\n");
}

void Emu_Player::stop()
{
	printf("Emu_Player stop\n");
	set_finished(true);
}

void Emu_Player::datablock(uint8_t dbtype, uint32_t dbsize, const uint8_t* db, uint32_t maxsize, uint32_t mask,
	uint32_t flags, uint32_t offset)
{
#if 0
	printf("Emu_Player datablock %02x = size %d, maxsize = %d, mask = %08x, flags = %08x, offset = %08x\n",
			dbtype,
			dbsize, maxsize, mask, flags, offset);
#endif
	datablocks[dbtype].resize(maxsize);
	std::copy_n(db, dbsize, datablocks[dbtype].begin() + offset);
}
