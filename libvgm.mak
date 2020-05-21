# libvgm should be installed on the system and we must tell the linker to
# pick the statically build version (to avoid .so headaches)
LDFLAGS_LIBVGM += -Wl,-Bstatic -lvgm-audio -lvgm-emu -lvgm-utils -Wl,-Bdynamic

# Platform specific flags for libvgm
ifeq ($(OS),Windows_NT)
CFLAGS += -DAUDDRV_WINMM -DAUDDRV_DSOUND -DAUDDRV_XAUD2
LDFLAGS_LIBVGM += -ldsound -luuid -lwinmm -lole32
endif
# TODO: ldflags for linux
