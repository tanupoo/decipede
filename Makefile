TARGETS = decipede

ifeq ($(OS),Linux)
LDFLAGS+= -lutil
endif

include Makefile.common
