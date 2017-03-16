CFLAGS_MHD = $(shell pkgconf --cflags libmicrohttpd)
LIBS_MHD = $(shell pkgconf --libs libmicrohttpd)
LIBS ?= 
CFLAGS ?= -Wall -Wextra -std=c99 -pedantic
CFLAGS += -D_DEBUG -g
LDFLAGS ?= -Wl,--allow-multiple-definition
BUILD_DIR ?= .
SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
TARGET = x11mirror-server

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $(BUILD_DIR)/$@ $(OBJECTS) $(LDFLAGS) $(LIBS) $(LIBS_MHD)

%.o: %.c
	$(CC) -c $(CFLAGS) $(CFLAGS_MHD) -o $@ $<

clean:
	$(RM) $(BUILD_DIR)/$(TARGET) $(OBJECTS)

.PHONY: all clean
