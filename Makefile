SRC = src
OBJ = obj
BIN = bin

CFLAGS = -Wall
CXXFLAGS = -std=c++17
LDFLAGS =

ifneq ($(RELEASE),1)
ifeq ($(ASAN),1)
CFLAGS += -fsanitize=address -O1 -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address
endif
CFLAGS += -g -DDEBUG
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

include ctrmml.mak
include imgui.mak
include libvgm.mak

$(OBJ)/%.o: $(SRC)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) $(CXXFLAGS) -MMD -c $< -o $@

$(OBJ)/%.o: $(SRC)/%.c
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -MMD -c $< -o $@

#======================================================================

MMLGUI_BIN = $(BIN)/mmlgui
UNITTEST_BIN = $(BIN)/unittest

all: $(MMLGUI_BIN) test

#======================================================================
# target mmlgui
#======================================================================
MMLGUI_OBJS = \
	$(IMGUI_OBJS) \
	$(IMGUI_CTE_OBJS) \
	$(OBJ)/main.o \
	$(OBJ)/window.o \
	$(OBJ)/main_window.o \
	$(OBJ)/editor_window.o \
	$(OBJ)/song_manager.o \
	$(OBJ)/track_info.o \
	$(OBJ)/track_view_window.o \
	$(OBJ)/track_list_window.o \
	$(OBJ)/audio_manager.o \
	$(OBJ)/emu_player.o \
	$(OBJ)/config_window.o \
	$(OBJ)/miniz.o \
	$(OBJ)/dmf_importer.o \

LDFLAGS_MMLGUI := $(LDFLAGS_IMGUI) $(LDFLAGS_CTRMML) $(LDFLAGS_LIBVGM)

$(MMLGUI_BIN): $(MMLGUI_OBJS) $(LIBCTRMML_CHECK)
	@mkdir -p $(@D)
	$(CXX) $(MMLGUI_OBJS) $(LDFLAGS) $(LDFLAGS_MMLGUI) -o $@
#ifeq ($(OS),Windows_NT)
#	cp `which glfw3.dll` $(@D)
#endif

run: $(MMLGUI_BIN)
	$(MMLGUI_BIN)

#======================================================================
# target unittest
#======================================================================
UNITTEST_OBJS = \
	$(OBJ)/track_info.o \
	$(OBJ)/unittest/main.o \
	$(OBJ)/unittest/test_track_info.o

$(CTRMML_LIB)/lib$(LIBCTRMML).a:
	$(MAKE) -C $(CTRMML) lib

$(UNITTEST_BIN): $(UNITTEST_OBJS) $(LIBCTRMML_CHECK)
	@mkdir -p $(@D)
	$(CXX) $(UNITTEST_OBJS) $(LDFLAGS) $(LDFLAGS_TEST) -o $@

test: $(UNITTEST_BIN)
	$(UNITTEST_BIN)

clean:
	rm -rf $(OBJ)
	$(MAKE) -C $(CTRMML) clean
	rm -rf $(CTRMML_LIB)

#======================================================================

.PHONY: all test run

-include $(OBJ)/*.d $(OBJ)/unittest/*.d $(IMGUI_CTE_OBJ)/*.d $(IMGUI_OBJ)/*.d
