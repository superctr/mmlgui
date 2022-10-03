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

typedef Pattern_Mml_Writer::Row::Type RowType;

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

std::string Dmf_Importer::get_mml(bool patches, bool patterns)
{
	return (patches ? patch_output : "") + (patterns ? mml_output : "");
}

static inline uint32_t read_le32(uint8_t* data)
{
	return data[0]|(data[1]<<8)|(data[2]<<16)|(data[3]<<24);
}

static inline uint32_t read_le16(uint8_t* data)
{
	return data[0]|(data[1]<<8);
}

static inline uint32_t read_rate(uint8_t* data)
{
	int max = 3; // max 3 characters
	uint32_t rate = 0;
	while(*data && max--)
		rate = (rate*10) + ((*data++) & 0x0f);
	return rate;
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

	system = *dmfptr++;
	if((system & 0x3f) != 0x02)
	{
		error_output = "Sorry, the DMF must be a GENESIS module.\n";
		return;
	}

	channel_map =  "ABCDEFGHIJ";
	channel_type = "FFFFFFsssn";
	channel_count = 10;
	if(system & 0x40)
	{
		channel_count += 3;
		channel_map =  "ABCMNODEFGHIJ";
		channel_type = "FFSSSSFFFsssn";
	}

	// Skip song name
	tmp8 = *dmfptr++;
	dmfptr += tmp8;

	// Skip song author
	tmp8 = *dmfptr++;
	dmfptr += tmp8;

	// get highlight A and B. A should be the beat value and B the measure length
	// Although few DMF modules actually conform to this.
	highlight_a = *dmfptr++;
	highlight_b = *dmfptr++;

	// Get timebase and tempo A
	tmp8 = 1 + *dmfptr++;
	speed_1 = tmp8 * *dmfptr++;
	speed_2 = tmp8 * *dmfptr++;

	// custom rate (this Hz a lot)
	rate = (*dmfptr++) ? 60 : 50;
	if(*dmfptr++)
		rate = read_rate(dmfptr);
	dmfptr += 3;

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

	// Skip wavetables
	tmp8 = *dmfptr++;
	for(int wt_counter = 0; wt_counter < tmp8; wt_counter++)
	{
		uint32_t wtsize = read_le32(dmfptr);
		dmfptr += 4 + wtsize * 4;
	}

	loop_position = -1;

	// Read channels
	channel_pattern_rows.resize(channel_count);
	for(int ch = 0; ch < channel_count; ch++)
	{
		// Read patterns
		channel_pattern_rows[ch].resize(matrix_rows);
		uint8_t effect_column_count = *dmfptr++;
		for(int pat = 0; pat < matrix_rows; pat++)
		{
			for(int row = 0; row < pattern_rows; row++)
			{
				Dmf_Importer::Channel_Row in_row;
				in_row.note = read_le16(dmfptr);
				in_row.octave = read_le16(dmfptr+2);
				in_row.volume = read_le16(dmfptr+4);
				dmfptr += 6;
				for(int effect = 0; effect < effect_column_count; effect++)
				{
					int16_t effect_code = read_le16(dmfptr);
					int16_t effect_value = read_le16(dmfptr+2);
					in_row.effects.push_back({effect_code,effect_value});
					dmfptr += 4;
					// Special case - handle loop command here
					if(effect_code == 0x0b)
						loop_position = effect_value;
				}
				in_row.instrument = read_le16(dmfptr);
				dmfptr += 2;
				channel_pattern_rows[ch][pat].push_back(in_row);
			}
		}
	}

	// Parse patterns
	uint32_t whole_len = 4 * (speed_1 + speed_2) * highlight_a / 2;
	Pattern_Mml_Writer writer(whole_len, (speed_1 + speed_2) * highlight_b / 2, matrix_rows, channel_map);
	parse_patterns(writer);

	mml_output += "\n";

	// write initial commands
	std::string init_commands = "";
	uint32_t fm3_mask = 0x0001;
	for(unsigned int channel = 0; channel < channel_count; channel++)
	{
		mml_output += channel_map[channel]; //'A' + channel;
		if(channel == 0)
		{
			init_commands += stringf("%c t%d\n", channel_map[channel], 120 * rate / highlight_a / (speed_1 + speed_2));
		}
		if(channel_type[channel] == 'S')
		{
			init_commands += stringf("%c 'fm3 %04x'\n", channel_map[channel], fm3_mask);
			fm3_mask <<= 4;
		}
	}
	mml_output += stringf(" C%d\n", whole_len) + init_commands;

	mml_output += writer.output_all();
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

	patch_output += stringf("@%d fm %d %d ; %s\n", id, ALG, FB, str.c_str());
	for(int opr=0; opr<4; opr++)
	{
		uint8_t op = op_table[opr];
		patch_output += stringf(" %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d \n",
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

//! Plays back DMF patterns and creates an MML event stream.
void Dmf_Importer::parse_patterns(Pattern_Mml_Writer& writer)
{
	int pattern = 0;
	int ticks = 0;
	int row = 0;
	int next_pattern_row = 0;
	int speed_1 = this->speed_1;
	int speed_2 = this->speed_2;

	std::vector<int> last_was_note(channel_count, 0);
	std::vector<int> last_instrument(channel_count, -1);

	while(pattern < matrix_rows)
	{
		next_pattern_row = 0;
		for(int channel = 0; channel < channel_count; channel++)
		{
			RowType type = last_was_note[channel] ? RowType::TIE : RowType::REST;
			writer.patterns[pattern].channels.resize(channel_count);
			writer.patterns[pattern].channels[channel].optimizable = true;
			writer.patterns[pattern].channels[channel].rows[0] = {type, 0, 0, ""};
		}

		for(; !next_pattern_row && row < pattern_rows; row ++)
		{
			for(int channel = 0; channel < channel_count; channel++)
			{
				bool insert_event = false;
				int delay = 0;

				Channel_Row& in_row = channel_pattern_rows[channel][pattern][row];

				Pattern_Mml_Writer::Row out_row;
				out_row.type = last_was_note[channel] ? RowType::TIE : RowType::REST;

				// There are probably some issues with the way loops are handled here.
				// If the loop starts at any other row than 0, this will be missed
				// Also the loop will in some cases cut the note played at the end of the song
				// if it is supposed to extend into the loop start pattern.
				if(loop_position == pattern && row == 0)
				{
					out_row.mml += "L ";
					last_instrument[channel] = -1;
				}

				if(in_row.note == 100)
				{
					insert_event = true;
					out_row.type = RowType::REST;
					last_was_note[channel] = false;
				}
				else if(in_row.note != 0)
				{
					insert_event = true;
					out_row.type = RowType::NOTE;
					out_row.note = in_row.note % 12;
					out_row.octave = ((in_row.note == 12) ? 2 : 1) + in_row.octave;
					if(in_row.instrument != -1 && in_row.instrument != last_instrument[channel])
					{
						out_row.mml += stringf("@%d",in_row.instrument);
						last_instrument[channel] = in_row.instrument;
					}
					last_was_note[channel] = true;
				}

				if(in_row.volume != -1)
				{
					insert_event = true;
					if(channel_type[channel] <= 'Z')
						out_row.mml += stringf("V%d",127 - in_row.volume);
					else
						out_row.mml += stringf("v%d",in_row.volume);
				}

				for(auto && effect : in_row.effects)
				{
					switch(effect.first)
					{
						case -1:
							break;
						case 0x09: //speed 1
							speed_1 = effect.second;
							break;
						case 0x0b: // loop
							if(!next_pattern_row)
								next_pattern_row = 1;
							break;
						case 0x0d: // skip to next row
							next_pattern_row = effect.second + 1;
							break;
						case 0x0f: //speed 2
							speed_2 = effect.second;
							break;
						case 0xEC: //note cut
							writer.patterns[pattern].channels[channel].rows[ticks + effect.second] = {RowType::REST, 0, 0, ""};
							last_was_note[channel] = false;
							break;
						case 0xED: //note delay
							delay = effect.second; // deflemask probably only delays the note and not other stuff but whatev
							break;
						case 0x03: //tone portamento (incomplete)
							// Note: Actual pitch slides not supported. This just adds a slur command
							// Currently unimplemented DM behavior: 3xx after a key off will ignore the 3xx effect
							if(out_row.type == RowType::NOTE && effect.second)
								out_row.type = RowType::SLUR;
							break;
						case 0x08: //panning
							out_row.mml += stringf("p%d", (effect.second & 0x01) | ((effect.second & 0x10) >> 3));
							break;
						case 0x20: //noise mode
							if(effect.second == 0x11)
								out_row.mml += "'mode 1'";
							else if(effect.second == 0x10)
								out_row.mml += "'mode 2'";
							else
								out_row.mml += "'mode 0'";
							break;
						default:
							writer.patterns[pattern].channels[channel].optimizable = false;
							break;
					}
				}

				if(insert_event)
				{
					writer.patterns[pattern].channels[channel].rows[ticks + delay] = out_row;
				}
			}
			ticks += (row & 1) ? speed_2 : speed_1;
		}

		writer.patterns[pattern].length = ticks;
		ticks = 0;
		if(next_pattern_row)
			row = next_pattern_row - 1;
		else
			row = 0;
		pattern ++;
	}

}

//================

Pattern_Mml_Writer::Pattern_Mml_Writer(int initial_whole_len, int initial_measure_len, int number_of_patterns, std::string& initial_channel_map)
	: whole_len(initial_whole_len)
	, measure_len(initial_measure_len)
	, channel_map(initial_channel_map)
{
	patterns.resize(number_of_patterns);

	smallest_len = 1;
	while(!((whole_len / smallest_len) & 1))
	{
		smallest_len <<= 1;
	}
	printf("whole_len = %d\n",whole_len);
	printf("smallest_len = %d\n",smallest_len);

	if(whole_len != measure_len)
	{
		printf("Warning: time signatures other than 4/4 are not supported yet...\n");
	}

#if 0 // move this to unittests
	std::cout << "192 = " << convert_length(192, 0, 16, '^') << "\n";
	std::cout << "96 = " << convert_length(96, 0, 16, '^') << "\n";
	std::cout << "48 = " << convert_length(48, 0, 16, '^') << "(l1) \n";
	std::cout << "24 = " << convert_length(24, 0, 16, '^') << "(l2) \n";
	std::cout << "21 = " << convert_length(21, 0, 16, '^') << "(l4..)\n";
	std::cout << "18 = " << convert_length(18, 0, 16, '^') << "(l4.)\n";
	std::cout << "15 = " << convert_length(15, 0, 16, '^') << "(l4^16)\n";
	std::cout << "12 = " << convert_length(12, 0, 16, '^') << "(l4) \n";
	std::cout << "9 = " << convert_length(9, 0, 16, '^') << "(l8.)\n";
	std::cout << "6 = " << convert_length(6, 0, 16, '^') << "(l8)\n";
	std::cout << "5 = " << convert_length(5, 0, 16, '^') << "\n";
	std::cout << "4 = " << convert_length(4, 0, 16, '^') << "\n";
	std::cout << "3 = " << convert_length(3, 0, 16, '^') << "(l16)\n";
	std::cout << "2 = " << convert_length(2, 0, 16, '^') << "\n";
	std::cout << "1 = " << convert_length(3, 0, 16, '^') << "\n";
#endif
}

std::string Pattern_Mml_Writer::output_all()
{
	std::string output;

	// patterns
	for(unsigned int pattern = 0; pattern < patterns.size(); pattern++)
	{
		output += stringf("\n; Pattern %d\n", pattern);

		for(unsigned int channel = 0; channel < patterns[pattern].channels.size(); channel++)
		{
			int default_len = smallest_len >> 1;
			std::string smallest = output_line(pattern, channel, smallest_len);
			while(default_len > 0)
			{
				std::string test = output_line(pattern, channel, default_len);
				//printf("testing %d, size = %d vs %d\n", default_len, test.size(), smallest.size());
				if(test.size() < smallest.size())
					smallest = test;
				default_len >>= 1;
			}
			output += smallest + "\n";
		}
	}

	return output;
}

std::string Pattern_Mml_Writer::output_line(int pattern, int channel, int default_length)
{
	// Maybe implement key signatures?
	static const char* note_mapping[12] = {"c","c+","d","d+","e","f","f+","g","g+","a","a+","b"};

	std::string output = stringf("%c ", channel_map[channel]);
	auto& rows = patterns[pattern].channels[channel].rows;
	int octave = -100;

	if(default_length > 0)
		output += stringf("l%d ",default_length);

	for(auto it = rows.begin(); it != rows.end(); it ++)
	{
		int time = it->first;
		int length = patterns[pattern].length - it->first;
		it++; //iterator dancing...
		if(it != rows.end())
			length = it->first - time;
		it--;

		output += it->second.mml;

		if(length)
		{
			char tie_character = '^';
			switch(it->second.type)
			{
				case RowType::TIE:
					output += "^";
					break;
				case RowType::SLUR:
					output += "&";
					//fall through
				case RowType::NOTE:
					if(it->second.octave == (octave - 1))
						output += "<";
					else if(it->second.octave == (octave + 1))
						output += ">";
					else if(it->second.octave != octave)
						output += stringf("o%d",it->second.octave);
					octave = it->second.octave;
					output += note_mapping[it->second.note % 12];
					break;
				case RowType::REST:
					output += "r";
					tie_character = 'r';
					break;
				default:
					output += "BUG!!!!";
					break;
			}
			output += convert_length(length, time, default_length, tie_character);
			time += length;
		}
	}

	if(output.back() == ' ')
		output.pop_back();

	return output;
}

std::string Pattern_Mml_Writer::convert_length(int length, int starting_time, int default_length, char tie_char)
{
	if(default_length < 0)
		return stringf(":%d",length);

	std::string output = "";

	// always tie notes across measures..
	if(starting_time && ((starting_time % measure_len) + length) > measure_len)
	{
		int first_length = measure_len - (starting_time % measure_len);
		output += convert_length(first_length, starting_time, default_length, tie_char);
		starting_time += first_length;
		length -= first_length;

		if(length)
			output += tie_char; // stringf("%c",tie_char);
	}

	// add whole notes (or rests)
	while(length >= whole_len)
	{
		//TODO: will need to handle odd time signatures here
		if(default_length != 1)
			output += "1";
		starting_time += whole_len;
		length -= whole_len;
		if(length)
			output += tie_char; //stringf("%c",tie_char);
	}

	// add frame durations if not possible to divide into a power of two
	if(length && length < (whole_len/smallest_len))
	{
		output += stringf(":%d",length);

		starting_time += length;
		length = 0;
	}

	// add note values
	else if(length)
	{
		int note_length = whole_len/smallest_len;
		int mml_length = smallest_len << 1;
		while(length >= note_length)
		{
			note_length <<= 1;
			mml_length >>= 1;
		}

		if(default_length != mml_length)
			output += stringf("%d",mml_length);

		note_length >>= 1;
		starting_time += note_length;
		length -= note_length;

		// add dots
		if(!(note_length & 1))
		{
			note_length >>= 1;
			int dots = 0;
			while(length >= note_length && dots < 2)
			{
				length -= note_length;
				starting_time += note_length;
				output += ".";
				dots ++;
				if(note_length & 1)
					break;
				note_length >>= 1;
			}
		}

		// add additional tied notes
		if(length)
		{
			output += tie_char; // stringf("%c",tie_char);
			output += convert_length(length, starting_time, default_length, tie_char);
		}
	}

	// separate between measures
	if(!(starting_time % measure_len))
		output += " ";

	return output;
}
