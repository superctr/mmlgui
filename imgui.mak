IMGUI_CTE_SRC = ImGuiColorTextEdit
IMGUI_CTE_OBJ = obj/$(IMGUI_CTE_SRC)
IMGUI_SRC = imgui
IMGUI_OBJ = obj/$(IMGUI_SRC)

ifeq ($(RELEASE),1)
CFLAGS += -DIMGUI_DISABLE_DEMO_WINDOWS=1 -DIMGUI_DISABLE_METRICS_WINDOW=1
endif

##---------------------------------------------------------------------
## BUILD FLAGS PER PLATFORM
##---------------------------------------------------------------------
CFLAGS += -I$(IMGUI_SRC) -I$(IMGUI_SRC)/examples -I$(IMGUI_SRC)/examples/libs/gl3w -DIMGUI_IMPL_OPENGL_LOADER_GL3W -I$(IMGUI_CTE_SRC)
LDFLAGS_IMGUI =

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LDFLAGS_IMGUI += -lGL `pkg-config --static --libs glfw3`

	CFLAGS += `pkg-config --cflags glfw3`
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LDFLAGS_IMGUI += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	LDFLAGS_IMGUI += -L/usr/local/lib -L/opt/local/lib
	#LDFLAGS_IMGUI += -lglfw3
	LDFLAGS_IMGUI += -lglfw

	CFLAGS += -I/usr/local/include -I/opt/local/include
endif

ifeq ($(OS),Windows_NT)
	ECHO_MESSAGE = "MinGW"
	LDFLAGS_IMGUI += -lglfw3 -lgdi32 -lopengl32 -limm32

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

$(IMGUI_CTE_OBJ)/%.o: $(IMGUI_CTE_SRC)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -MMD -c $< -o $@

$(IMGUI_OBJ)/%.o: $(IMGUI_SRC)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -MMD -c $< -o $@

$(IMGUI_OBJ)/%.o: $(IMGUI_SRC)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c $< -o $@
