# libvgm should be installed on the system and we must tell the linker to
# pick the statically build version (to avoid .so headaches)

# Platform specific flags for libvgm
ifeq ($(OS),Windows_NT)
CFLAGS += -DAUDDRV_WINMM -DAUDDRV_DSOUND -DAUDDRV_XAUD2
LDFLAGS_LIBVGM += -Wl,-Bstatic -lvgm-audio -lvgm-emu -lvgm-utils -Wl,-Bdynamic
LDFLAGS_LIBVGM += -ldsound -luuid -lwinmm -lole32
else
CFLAGS += `pkg-config --with-path=/usr/local/lib/pkgconfig --cflags vgm-audio`
LDFLAGS_LIBVGM += -Wl,-rpath,/usr/local/lib `pkg-config --with-path=/usr/local/lib/pkgconfig --libs vgm-audio vgm-emu`
endif
