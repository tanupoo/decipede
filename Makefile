TARGETS = decipede

OS= $(shell uname -s)

ifeq ($(OS),Linux)
LDLIBS+= -lutil
endif

include Makefile.common
