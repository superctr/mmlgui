#ifndef CONFIG_WINDOW_H
#define CONFIG_WINDOW_H

#include "window.h"

//! Config window container
class Config_Window : public Window
{
	public:
		Config_Window();
		void display() override;

	private:
		void show_audio_tab();
		void show_emu_tab();
		void show_mixer_tab();
		void show_confirm_buttons();
};

#endif //CONFIG_WINDOW_H

