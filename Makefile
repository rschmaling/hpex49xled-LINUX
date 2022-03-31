# Makefile for building demo stuff
#
#
#  Edit this file to compile extra C files into their programs
ifneq (,)
	This makefile requires GNU Make.
endif
PREFIX = 
TARGETS = hpex49xled
OBJS = hpex49xled.o init.o updatemonitor.o
INCLUDES = 
CC_C = $(CROSS_TOOL)gcc
CC = gcc

CFLAGS = -O2 -Wall -Werror -std=gnu99 -march=native
LDFLAGS = -ludev -pthread -lm
#CFLAGS = -Wall -g -std=c99 -Werror 
RCPREFIX := /etc/systemd/system/
RCFILE := hpex49xled.service



ifeq ($(PREFIX),)
       PREFIX := /usr/local
endif


all: clean ${TARGETS}

${OBJS}: hpex49xled.c init.c updatemonitor.c
	${CC} ${CFLAGS} ${INCLUDES} -c $?

#$(TARGETS):
#	$(CC_C) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

${TARGETS}: ${OBJS}
	${CC} ${CFLAGS} ${INCLUDES} -o $@ ${OBJS} ${LDFLAGS}

.PHONY: clean

clean:
	rm -f *.core $(OBJS) $(TARGETS)

.PHONY: install

install: all
	test -f $(RCPREFIX)$(RCFILE) || install -m 644 $(RCFILE) $(RCPREFIX)
	install -s -m 700 $(TARGETS) $(PREFIX)/bin/

