#include "window.h"
#include <stdio.h>

uint32_t Window::id_counter = 0; // next window's ID
bool Window::modal_open = 0;     // indicates if a modal is open, since imgui can
								 // only display one modal at a time, and will softlock
								 // if another modal is opened.

Window::Window(Window* parent)
	: active(true)
	, parent(parent)
	, close_req_state(Window::NO_CLOSE_REQUEST)
{
	id = id_counter++;
	children.clear();
}

Window::~Window()
{
}

//! Display window including child windows
bool Window::display_all()
{
	display();
	for(auto i = children.begin(); i != children.end(); )
	{
		bool child_active = i->get()->display_all();
		if(!child_active)
			children.erase(i);
		else
			++i;
	}
	return active;
}

//! Tell window to close.
void Window::close()
{
	active = false;
}

//! Find child window with matching type
std::vector<std::shared_ptr<class Window>>::iterator Window::find_child(Window_Type type)
{
	for(auto i = children.begin(); i != children.end(); i++)
	{
		if(i->get()->active && i->get()->type == type)
			return i;
	}
	return children.end();
}

//! Request windows to close.
void Window::close_request_all()
{
	if(close_req_state != Window::CLOSE_IN_PROGRESS)
	{
		for(auto i = children.begin(); i != children.end(); i++)
		{
			if(i->get()->active)
				i->get()->close_request_all();
		}
		close_request();
	}
}

//! Request window to close.
void Window::close_request()
{
	close_req_state = Window::CLOSE_OK;
}

//! Get the status of close request.
Window::Close_Request_State Window::get_close_request()
{
	for(auto i = children.begin(); i != children.end(); i++)
	{
		if(i->get()->active)
		{
			auto child_request = i->get()->get_close_request();
			if(child_request == Window::CLOSE_IN_PROGRESS)
				return Window::CLOSE_IN_PROGRESS;
			else if(child_request == Window::CLOSE_NOT_OK)
				return Window::CLOSE_NOT_OK;
		}
	}
	return close_req_state;
}

//! Clear the close request status
void Window::clear_close_request()
{
	for(auto i = children.begin(); i != children.end(); i++)
	{
		if(i->get()->active)
		{
			i->get()->clear_close_request();
		}
	}
	close_req_state = Window::NO_CLOSE_REQUEST;
}

