#ifndef DMF_IMPORTER_H
#define DMF_IMPORTER_H

#include <vector>
#include <string>
#include <cstdint>

class Dmf_Importer
{
	public:
		Dmf_Importer(const char* filename);

		std::string get_error();
		std::string get_mml();

	private:
		void parse();
		uint8_t* parse_fm_instrument(uint8_t* ptr, int id, std::string str);
		uint8_t* parse_psg_instrument(uint8_t* ptr, int id, std::string str);

		std::string error_output;
		std::string mml_output;
		std::vector<uint8_t> data;

		uint8_t channel_count;
		uint8_t pattern_rows;
		uint8_t matrix_rows;
		uint8_t instrument_count;

};
#endif
