CTRMML = ctrmml
CTRMML_SRC = $(CTRMML)/src
CTRMML_LIB = $(CTRMML)/lib
LIBCTRMML  = ctrmml

ifneq ($(RELEASE),1)
LIBCTRMML := $(LIBCTRMML)_debug
endif

# ctrmml library is in a local submodule and only a static library can be built
CFLAGS += -I$(CTRMML_SRC)
LDFLAGS += -L$(CTRMML_LIB) -l$(LIBCTRMML)

LIBCTRMML_CHECK := $(CTRMML_LIB)/lib$(LIBCTRMML).a