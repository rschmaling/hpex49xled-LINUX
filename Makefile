# Makefile for building demo stuff
#
#
#  Edit this file to compile extra C files into their programs
ifneq (,)
	This makefile requires GNU Make.
endif

TARGETS = hpex49xled
OBJS = hpex49xled.o init.o updatemonitor.o
INCLUDES = 
CC_C = $(CROSS_TOOL)gcc

CFLAGS = -O2 -Wall -std=gnu99 -Werror
LDFLAGS = -ludev -pthread -lm
#CFLAGS = -Wall -g -std=c99 -Werror 

all: clean ${TARGETS}

${OBJS}: hpex49xled.c init.c updatemonitor.c
	${CC} ${CFLAGS} ${INCLUDES} -c $?

#$(TARGETS):
#	$(CC_C) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

${TARGETS}: ${OBJS}
	${CC} ${CFLAGS} ${INCLUDES} -o $@ ${OBJS} ${LDFLAGS}

clean:
	rm -f *.core $(OBJS) $(TARGETS)

