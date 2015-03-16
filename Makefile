.PHONY: all clean
.DEFAULT_GOAL = all

CC = gcc
CFLAGS += -g -O2 -std=c11 -pedantic -pedantic-errors
LDFLAGS +=

WFLAGS = -Wall -Wextra -Werror

OBJS = \
    usb_ids_knx.o \
    usb.o

PACKAGES = libusb-1.0

NOWARN =
$(NOWARN): WFLAGS =

PKG_CFLAGS = $(shell pkg-config --cflags $(PACKAGES))
PKG_LIBS = $(shell pkg-config --libs $(PACKAGES)) -lev

DEPS = $(OBJS:.o=.d)
ifneq ($(MAKECMDGOALS),clean)
  include $(DEPS)
endif

all: main.exe
clean:
	-rm -f $(OBJS) $(DEPS)

main.exe : $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(PKG_LIBS)

%.d : %.c
	$(CC) $(PKG_CFLAGS) -M -MP -MQ $@ -MQ $(<:%.c=%.o) -o $@ $<

%.o : %.c
	$(CC) $(PKG_CFLAGS) $(CFLAGS) $(WFLAGS) -c -o $@ $<
