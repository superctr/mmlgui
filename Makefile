IMGUI_CTE_SRC = ImGuiColorTextEdit
IMGUI_CTE_OBJ = obj/$(IMGUI_CTE_SRC)
IMGUI_SRC = imgui
IMGUI_OBJ = obj/$(IMGUI_SRC)
CTRMML_LIB = ctrmml/lib
SRC = src
OBJ = obj

CFLAGS = -Wall
LDFLAGS =

ifneq ($(RELEASE),1)
CFLAGS += -g
else
CFLAGS += -O2
LDFLAGS += -s
endif

LDFLAGS_TEST = -lcppunit
ifeq ($(OS),Windows_NT)
	LDFLAGS += -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic
else
	UNAME_S := $(shell uname -s)
endif

##---------------------------------------------------------------------
## BUILD FLAGS PER PLATFORM
##---------------------------------------------------------------------
CFLAGS += -I$(IMGUI_SRC) -I$(IMGUI_SRC)/examples -I$(IMGUI_SRC)/examples/libs/gl3w -DIMGUI_IMPL_OPENGL_LOADER_GL3W -I$(IMGUI_CTE_SRC)
LDFLAGS += -L$(CTRMML_LIB) -lvgm-audio -lvgm-emu -lctrmml

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LDFLAGS += -lGL `pkg-config --static --libs glfw3`

	CFLAGS += `pkg-config --cflags glfw3`
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LDFLAGS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	LDFLAGS += -L/usr/local/lib -L/opt/local/lib
	#LDFLAGS += -lglfw3
	LDFLAGS += -lglfw

	CFLAGS += -I/usr/local/include -I/opt/local/include
endif

ifeq ($(OS),Windows_NT)
	ECHO_MESSAGE = "MinGW"
	LDFLAGS += -lglfw3 -lgdi32 -lopengl32 -limm32

	CFLAGS += `pkg-config --cflags glfw3`
endif

IMGUI_OBJS = \
	$(IMGUI_OBJ)/imgui.o \
	$(IMGUI_OBJ)/imgui_demo.o \
	$(IMGUI_OBJ)/imgui_draw.o \
	$(IMGUI_OBJ)/imgui_widgets.o \
	$(IMGUI_OBJ)/examples/imgui_impl_glfw.o \
	$(IMGUI_OBJ)/examples/imgui_impl_opengl3.o \
	$(IMGUI_OBJ)/examples/libs/gl3w/GL/gl3w.o \
	$(IMGUI_OBJ)/addons/imguifilesystem/imguifilesystem.o

IMGUI_CTE_OBJS = \
	$(IMGUI_CTE_OBJ)/TextEditor.o \

##---------------------------------------------------------------------
## IMGUI specific stuff ends
##---------------------------------------------------------------------

MMLGUI_OBJS = \
	$(IMGUI_OBJS) \
	$(IMGUI_CTE_OBJS) \
	$(OBJ)/main.o \
	$(OBJ)/window.o \
	$(OBJ)/main_window.o \
	$(OBJ)/editor_window.o \

$(OBJ)/%.o: $(SRC)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -MMD -c $< -o $@

$(IMGUI_CTE_OBJ)/%.o: $(IMGUI_CTE_SRC)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -MMD -c $< -o $@

$(IMGUI_OBJ)/%.o: $(IMGUI_SRC)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -MMD -c $< -o $@

$(IMGUI_OBJ)/%.o: $(IMGUI_SRC)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

all: mmlgui

$(CTRMML_LIB)/libctrmml.a:
	make -C ctrmml lib RELEASE=$(RELEASE)

mmlgui: $(MMLGUI_OBJS) $(CTRMML_LIB)/libctrmml.a
	$(CXX) $(MMLGUI_OBJS) $(LDFLAGS) -o $@
ifeq ($(OS),Windows_NT)
	cp `which glfw3.dll` $(@D)
endif

clean:
	rm -rf $(OBJ)
	$(MAKE) -C ctrmml clean
	rm -rf $(CTRMML_LIB)

.PHONY: all

-include $(OBJ)/*.d $(IMGUI_CTE_OBJ)/*.d $(IMGUI_OBJ)/*.d
