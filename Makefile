# libmicrohttpd
MHD_CFLAGS = $(shell pkg-config --cflags libmicrohttpd)
MHD_LIBS = $(shell pkg-config --libs libmicrohttpd)
# ImageMagick
IM_CFLAGS = $(shell pkg-config --cflags MagickWand)
IM_LIBS = $(shell pkg-config --libs MagickWand)
# user flags
LIBS ?= 
CFLAGS ?= -Wall -Wextra -std=c99 -pedantic
CFLAGS += -D_POSIX_C_SOURCE=199309L -D_XOPEN_SOURCE=500
# debug
CFLAGS += -D_DEBUG -g
LDFLAGS ?= -Wl,--allow-multiple-definition
BUILD_DIR ?= .

SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
TARGET = x11mirror-server

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $(BUILD_DIR)/$@ $(OBJECTS) $(LDFLAGS) $(LIBS) $(MHD_LIBS) $(IM_LIBS)

%.o: %.c
	$(CC) -c $(CFLAGS) $(MHD_CFLAGS) $(IM_CFLAGS) -o $@ $<

clean:
	$(RM) $(BUILD_DIR)/$(TARGET) $(OBJECTS)

.PHONY: all clean
