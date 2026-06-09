# ==============================================================================
# Makefile for test_host.c
#
# Uses pkg-config to set the compile and linking flags automatically
# across Windows (MSYS2/MinGW), Linux, and macOS.
# ==============================================================================

CC ?= gcc
CFLAGS = -Wall -Wextra -O2
TARGET = test_host

# Determine correct package name dynamically
PKG_NAME := $(shell for pkg in hidapi hidapi-hidraw hidapi-libusb; do \
		pkg-config --exists $$pkg && echo $$pkg && break; \
	done)

# If no package is found by pkg-config, default to 'hidapi'
ifeq ($(PKG_NAME),)
    PKG_NAME := hidapi
endif

# Append compile and linking flags from pkg-config
CFLAGS += $(shell pkg-config --cflags $(PKG_NAME) 2>/dev/null)
LIBS = $(shell pkg-config --libs $(PKG_NAME) 2>/dev/null)

# Fallback linking if pkg-config failed to return any libraries
ifeq ($(strip $(LIBS)),)
    ifeq ($(OS),Windows_NT)
        LIBS = -lhidapi -lsetupapi
    else
        LIBS = -lhidapi
    endif
endif

# OS specific executable extension and clean commands
ifeq ($(OS),Windows_NT)
    EXE = .exe
    RM = del /F /Q
else
    EXE =
    RM = rm -f
endif

# Default Target
all: $(TARGET)$(EXE)

$(TARGET)$(EXE): test_host.c
	$(CC) $(CFLAGS) test_host.c -o $(TARGET)$(EXE) $(LIBS)

# Clean Target
clean:
	$(RM) $(TARGET)$(EXE)

.PHONY: all clean

