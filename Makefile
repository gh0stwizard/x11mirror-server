CFLAGS ?= -O2
CPPFLAGS = -Wall -Wextra -std=c99 -pedantic -D_POSIX_C_SOURCE=199309L -D_XOPEN_SOURCE=500
PKG_CONFIG ?= pkg-config
INSTALL ?= install
RM ?= rm -f

MAJOR_VERSION = 0
MINOR_VERSION = 5
PATCH_VERSION = 0
VERSION = $(MAJOR_VERSION).$(MINOR_VERSION).$(PATCH_VERSION)

DESTDIR ?= /usr/local
BINDIR ?= $(DESTDIR)/bin
LIBDIR ?= $(DESTDIR)/lib
INCLUDEDIR ?= $(DESTDIR)/include
DATAROOTDIR ?= $(DESTDIR)/share
MANDIR ?= $(DATAROOTDIR)/man

ifndef ENABLE_DEBUG
export ENABLE_DEBUG = NO
endif

#----------------------------------------------------------#

# libmicrohttpd
DEFS_MHD = $(shell $(PKG_CONFIG) --cflags libmicrohttpd)
LIBS_MHD = $(shell $(PKG_CONFIG) --libs libmicrohttpd)

# ImageMagick
DEFS_IM = $(shell $(PKG_CONFIG) --cflags MagickWand)
LIBS_IM = $(shell $(PKG_CONFIG) --libs MagickWand)

ifeq ($(shell $(PKG_CONFIG) --max-version=7 MagickCore || echo 7),7)
DEFS_IM += -DIM_VERSION=7
else
DEFS_IM += -DIM_VERSION=6
endif

ifeq ($(ENABLE_DEBUG),YES)
DEFS_OPTIONS += -D_DEBUG
endif

#----------------------------------------------------------#

DEFS ?= $(DEFS_MHD) $(DEFS_IM) $(DEFS_OPTIONS)
DEFS += -DAPP_VERSION=$(VERSION)

LIBS ?= $(LIBS_MHD) $(LIBS_IM)
LDFLAGS ?= -Wl,--allow-multiple-definition

SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
TARGET = x11mirror-server

#----------------------------------------------------------#

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFS) -o $@ -c $<

clean:
	$(RM) $(TARGET) $(OBJECTS)

.PHONY: all clean
