#include "emu_player.h"
#include <cstdlib>

using std::malloc;
using std::free;

Device_Wrapper::Device_Wrapper()
	: dev(nullptr)
	, resmpl(nullptr)
	, write_a8d8(nullptr)
{
}

Device_Wrapper::~Device_Wrapper()
{
	if(started)
	{
		// deinit
	}

	if(dev)
		free(dev);
	if(resmpl)
		free(resmpl);
}

void Device_Wrapper::set_rate(uint32_t rate, uint16_t volume)
{
}

void Device_Wrapper::reset_resampler()
{
}

void Device_Wrapper::setup_sn76489(uint32_t freq, int16_t volume, uint8_t lfsr_w, uint16_t lfsr_t)
{
}

void Device_Wrapper::write(uint16_t addr, uint16_t data)
{
}

//=====================================================================

Emu_Player::Emu_Player(std::shared_ptr<Song>& song)
{
}

Emu_Player::~Emu_Player()
{
}

//=====================================================================
// Audio_Stream (front end)
// Audio_Manager -> Emu_Player
//=====================================================================

void Emu_Player::setup_stream(uint32_t sample_rate)
{
}

int Emu_Player::get_sample(uint32_t* output, int count, int channels)
{
	return 0;
}

void Emu_Player::stop_stream()
{
}

//=====================================================================
// VGM_Interface (back end)
// Driver -> Emu_Player
//=====================================================================

void Emu_Player::write(uint8_t command, uint16_t port, uint16_t reg, uint16_t data)
{
}

void Emu_Player::dac_setup(uint8_t sid, uint8_t chip_id, uint32_t port, uint32_t reg, uint8_t db_id)
{
}

void Emu_Player::dac_start(uint8_t sid, uint32_t start, uint32_t length, uint32_t freq)
{
}

void Emu_Player::dac_stop(uint8_t sid)
{
}

void Emu_Player::poke32(uint32_t offset, uint32_t data)
{
}

void Emu_Player::poke16(uint32_t offset, uint16_t data)
{
}

void Emu_Player::poke8(uint32_t offset, uint8_t data)
{
}

void Emu_Player::set_loop()
{
}

void Emu_Player::stop()
{
}

void Emu_Player::datablock(uint8_t dbtype, uint32_t dbsize, const uint8_t* db, uint32_t maxsize, uint32_t mask,
	uint32_t flags, uint32_t offset)
{
}
