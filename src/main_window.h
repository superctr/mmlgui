#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "window.h"

//! FPS/version overlay
class FPS_Overlay : public Window
{
	public:
		FPS_Overlay();
		void display() override;
};

//! Main window container
class Main_Window : public Window
{
	public:
		Main_Window();
		void display() override;
};

#endif //MAIN_WINDOW_H