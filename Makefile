TARGETS = decipede

.PHONY: clean

OS= $(shell uname -s)

CFLAGS += -Werror -Wall
#CFLAGS += -I.
#CFLAGS += -I/usr/local/include

#LDFLAGS	+= -L/usr/local/lib

ifeq ($(OS),Linux)
#LDFLAGS+= -lpthread
endif

all: $(TARGETS)

$(TARGETS):

#
#.SUFFIXES: .o .c
#
#.c.o:
#	$(CC) $(CFLAGS) -c $<

clean:
	-rm -rf *.dSYM *.o
ifneq ($(TARGETS),)
	-rm -f $(TARGETS)
endif

distclean: clean
	-rm -f Makefile config.cache config.status config.log .depend

make_var_test:
	@echo CFLAGS =$(CFLAGS)
	@echo LDFLAGS=$(LDFLAGS)

