#include <unistd.h>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>

#include "dmf_importer.h"
#include "stringf.h"

#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

Dmf_Importer::Dmf_Importer(const char* filename)
{
	if(std::ifstream is{filename, std::ios::binary | std::ios::ate})
	{
		auto size = is.tellg();
		std::vector<uint8_t> dmfz(size, '\0');
		is.seekg(0);
		if(is.read((char*)&dmfz[0],size))
		{
			uLong dmfsize = 0x1000000;
			data.resize(dmfsize, 0);

			int res;
			res = uncompress((Byte*) data.data(), &dmfsize, (Byte*) dmfz.data(), dmfz.size());
			if (res != Z_OK)
			{
				error_output = "File could not be decompressed\n";
			}
			else
			{
				parse();
			}
		}
	}
	else
	{
		error_output = "File could not be opened\n";
	}
}

std::string Dmf_Importer::get_error()
{
	return error_output;
}

std::string Dmf_Importer::get_mml()
{
	return mml_output;
}

static inline uint32_t read_le32(uint8_t* data)
{
	return data[0]|(data[1]<<8)|(data[2]<<16)|(data[3]<<24);
}

static inline std::string read_str(uint8_t* data, uint8_t length)
{
	std::string out;
	for(int i = 0; i < length; i++)
	{
		out.push_back(*data++);
	}
	return out;
}

void Dmf_Importer::parse()
{
	uint8_t tmp8;

	uint8_t* dmfptr = data.data();

	dmfptr += 16; // skip header

	uint8_t version = *dmfptr++;
	if(version != 0x18)
	{
		error_output = stringf("Incompatible DMF version (found '0x%02x', expected '0x18')\nTry opening and saving the file in Deflemask legacy.",version);
		return;
	}

	uint8_t system = *dmfptr++;
	if((system & 0x3f) != 0x02)
	{
		error_output = "Sorry, the DMF must be a GENESIS module.\n";
		return;
	}

	channel_count = 10;
	if(system & 0x40)
		channel_count += 3;

	// Skip song name
	tmp8 = *dmfptr++;
	dmfptr += tmp8;

	// Skip song author
	tmp8 = *dmfptr++;
	dmfptr += tmp8;

	// Skip highlight A/B, timebase, frame mode, custom HZ
	dmfptr += 10;

	pattern_rows = read_le32(dmfptr);
	dmfptr += 4;
	//printf("Number of pattern rows: %d (%08x)\n", dmf_pattern_rows, dmf_pattern_rows);

	matrix_rows = *dmfptr++;
	//printf("Number of pattern matrix rows: %d\n", dmf_matrix_rows);

	// Skip pattern matrix rows
	dmfptr += channel_count * matrix_rows;

	// Now read instruments
	instrument_count = *dmfptr++;

	for(int ins_counter = 0; ins_counter < instrument_count; ins_counter ++)
	{
		tmp8 = *dmfptr++;
		auto name = read_str(dmfptr, tmp8);
		dmfptr += tmp8;

		uint8_t type = *dmfptr++;

		if(type == 0)
		{
			dmfptr = parse_psg_instrument(dmfptr, ins_counter, name);
		}
		else if(type == 1)
		{
			dmfptr = parse_fm_instrument(dmfptr, ins_counter, name);
		}
		else
		{
			error_output = "Encountered an unknown instrument type.\n";
			return;
		}
	}
}

#define ALG ptr[0]
#define FB ptr[1]
#define AR ptr[op+1]
#define DR ptr[op+2]
#define ML ptr[op+3]
#define RR ptr[op+4]
#define SL ptr[op+5]
#define TL ptr[op+6]
#define KS ptr[op+8]
#define DT dt_table[ptr[op+9]]
#define SR ptr[op+10]
#define SSG (ptr[op+11] | ptr[op+0] * 100)

uint8_t* Dmf_Importer::parse_fm_instrument(uint8_t* ptr, int id, std::string str)
{
	uint8_t op_table[4] = {4, 28, 16, 40};
	uint8_t dt_table[7] = {7, 6, 5, 0, 1, 2, 3};

	mml_output += stringf("@%d fm %d %d ; %s\n", id, ALG, FB, str.c_str());
	for(int opr=0; opr<4; opr++)
	{
		uint8_t op = op_table[opr];
		mml_output += stringf(" %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d \n",
								AR, DR, SR, RR, SL, TL, KS, ML, DT, SSG);
	}
	return ptr + 52;
}

uint8_t* Dmf_Importer::parse_psg_instrument(uint8_t* ptr, int id, std::string str)
{
	uint8_t size;
	uint8_t loop;

	size = *ptr++;
	if(size)
	{
		mml_output += stringf("@%d psg ; %s", id, str.c_str());
		loop = ptr[size * 4];

		for(int i=0; i<size; i++)
		{
			mml_output += stringf(  "%s %s%d",
				(i % 16 == 0) ? "\n" : "",
				(loop == i) ? "| " : "",
				ptr[i * 4]);
		}
		mml_output += "\n";
		ptr += size * 4 + 1;
	}

	for(int i = 0; i < 3; i++)
	{
		size = *ptr++;
		if(size)
			ptr += size * 4 + 1;
		// skip arp macro mode
		if(i == 0)
			ptr++;
	}

	return ptr;
}
