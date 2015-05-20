TARGETS = decipede

OS= $(shell uname -s)

ifeq ($(OS),Linux)
LDFLAGS+= -lutil
endif

include Makefile.common
