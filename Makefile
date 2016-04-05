TARGETS = decipede

include Makefile.common

OS= $(shell uname -s)

ifeq ($(OS),Linux)
LDLIBS+= -lutil
endif

