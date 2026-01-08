#ifndef WINDOW_H
#define WINDOW_H

#include "window_type.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <string>

//! Abstract window class
class Window
{
	public:
		enum Close_Request_State
		{
			NO_CLOSE_REQUEST = 0,
			CLOSE_NOT_OK = 1,
			CLOSE_IN_PROGRESS = 2,
			CLOSE_OK = 3,
		};

	private:
		static uint32_t id_counter;

	protected:
		uint32_t id;
		Window_Type type;
		bool active;
		class Window* parent;
		std::vector<std::shared_ptr<class Window>> children;
		Close_Request_State close_req_state;

		std::vector<std::shared_ptr<class Window>>::iterator find_child(Window_Type type);

	public:
		static bool modal_open;

		Window(class Window* parent = nullptr);
		virtual ~Window();

		bool display_all();
		virtual void display() = 0;
		virtual void close();

		void close_request_all();
		virtual void close_request();
		virtual Close_Request_State get_close_request();
		void clear_close_request();

		virtual std::string dump_state();
		std::string dump_state_all();
};

#endif //WINDOW_H
