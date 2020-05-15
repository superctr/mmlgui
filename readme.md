mmlgui
======

## Compiling

### Prerequisites
First, make sure all submodules have been cloned. If you didn't clone this repository
with `git clone --recurse-submodules`, do this:

	git submodule update --init

Make sure you have the following packages installed: (this list might not be 100% correct)

	glfw  libvgm

#### Installing libvgm

If libvgm is not available on your system, you need to install it manually. Example:

##### Linux

	mkdir build && cd build
	cmake ..
	make
	make install

##### Windows (MSYS2)
The `CMAKE_INSTALL_PREFIX` should of course match where your MSYS2/MinGW install folder is.

	mkdir build && cd build
	cmake .. -G "MSYS Makefiles" -D AUDIODRV_LIBAO=OFF -D CMAKE_INSTALL_PREFIX=d:/msys64/mingw64
	make
	make install

### Building
Once all prerequisites are installed, it should be as easy as

	make

### Special thanks

- [Dear ImGui](https://github.com/ocornut/imgui)
- [Dear ImGui addons branch](https://github.com/Flix01/imgui)
- [ImGuiColorTextEditor](https://github.com/BalazsJako/ImGuiColorTextEdit)

### Copyright

(c) 2020 Ian Karlsson