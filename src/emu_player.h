#ifndef EMU_INTERFACE_H
#define EMU_INTERFACE_H

#include <memory>
#include <map>
#include <vector>

#include <vgm/emu/EmuStructs.h>
#include <vgm/emu/SoundEmu.h>
#include <vgm/emu/Resampler.h>

#include "audio_manager.h"
#include "vgm.h"


class Device_Wrapper
{
	public:
		Device_Wrapper();
		virtual ~Device_Wrapper();

		void set_rate(uint32_t rate, uint16_t volume);
		void reset_resampler();

		void setup_sn76489(uint32_t freq,
			int16_t volume = 0x100,
			uint8_t lfsr_w = 0x10,
			uint16_t lfsr_t = 0x09);

		void write(uint16_t addr, uint16_t data);

	private:
		DEV_INFO* dev;
		RESMPL_STATE* resmpl;

		bool started;
		uint32_t sample_rate;
		uint16_t volume;
		DEVFUNC_WRITE_A8D8 write_a8d8;
};


class Emu_Player
	: private VGM_Interface
	, public Audio_Stream
{
	public:
		Emu_Player(std::shared_ptr<Song> song);
		virtual ~Emu_Player();

		void setup_stream(uint32_t sample_rate);
		int get_sample(WAVE_32BS* output, int count, int channels);
		void stop_stream();

	private:
		void write(uint8_t command, uint16_t port, uint16_t reg, uint16_t data);
		void dac_setup(uint8_t sid, uint8_t chip_id, uint32_t port, uint32_t reg, uint8_t db_id);
		void dac_start(uint8_t sid, uint32_t start, uint32_t length, uint32_t freq);
		void dac_stop(uint8_t sid);
		void poke32(uint32_t offset, uint32_t data);
		void poke16(uint32_t offset, uint16_t data);
		void poke8(uint32_t offset, uint8_t data);
		void set_loop();
		void stop();
		void datablock(
			uint8_t dbtype,
			uint32_t dbsize,
			const uint8_t* db,
			uint32_t maxsize,
			uint32_t mask = 0xffffffff,
			uint32_t flags = 0,
			uint32_t offset = 0);

		int sample_rate;
		std::map<int, Device_Wrapper> devices;
		std::map<int, std::vector<uint8_t>> datablocks;

		float delta_time;
};

#endif