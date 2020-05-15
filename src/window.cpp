#include "window.h"
#include <stdio.h>

uint32_t Window::id_counter = 0;

Window::Window(Window* parent)
	: active(true)
	, parent(parent)
{
	id = id_counter++;
	children.clear();
}

Window::~Window()
{
}

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

void Window::close()
{
	active = false;
}

std::vector<std::shared_ptr<class Window>>::iterator Window::find_child(Window_Type type)
{
	for(auto i = children.begin(); i != children.end(); )
	{
		if(i->get()->active && i->get()->type == type)
			return i;
	}
	return children.end();
}
