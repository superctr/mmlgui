#ifndef WINDOW_H
#define WINDOW_H

#include "window_type.h"
//#include <cstdint>
#include <vector>
#include <memory>

//! Abstract window class
class Window
{
	private:
		static uint32_t id_counter;

	protected:
		uint32_t id;
		Window_Type type;
		bool active;
		class Window* parent;
		std::vector<std::shared_ptr<class Window>> children;

		std::vector<std::shared_ptr<class Window>>::iterator find_child(Window_Type type);

	public:
		Window(class Window* parent = nullptr);
		virtual ~Window();

		bool display_all();
		virtual void display() = 0;
		virtual void close();
};

#endif //WINDOW_H