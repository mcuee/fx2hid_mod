# ==============================================================================
# Makefile for test_host.c
#
# Works on Windows (MinGW/MSYS2), Linux, and macOS.
# Requires the HIDAPI library to be installed.
# ==============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = test_host

# OS Detection
ifeq ($(OS),Windows_NT)
    # Windows settings (MinGW/MSYS2)
    # setupapi is required on Windows for HIDAPI
    LIBS = -lhidapi -lsetupapi
    EXE = .exe
    RM = del /F /Q
else
    # Linux and macOS settings
    EXE =
    RM = rm -f
    UNAME_S := $(shell uname -s)
    
    # Try using pkg-config to find hidapi if available, otherwise fallback
    PKG_OK := $(shell pkg-config --exists hidapi-hidraw && echo yes || (pkg-config --exists hidapi-libusb && echo yes || echo no))
    ifeq ($(PKG_OK),yes)
        # Use pkg-config to get cflags and libs
        # Prefers hidapi-hidraw on Linux, falls back to libusb if needed
        PKG_CONFIG_NAME = $(shell pkg-config --exists hidapi-hidraw && echo hidapi-hidraw || echo hidapi-libusb)
        CFLAGS += $(shell pkg-config --cflags $(PKG_CONFIG_NAME))
        LIBS = $(shell pkg-config --libs $(PKG_CONFIG_NAME))
    else
        # Fallback linking directly against hidapi
        LIBS = -lhidapi
        
        # Homebrew paths for macOS (Apple Silicon fallback)
        ifeq ($(UNAME_S),Darwin)
            CFLAGS += -I/opt/homebrew/include
            LIBS += -L/opt/homebrew/lib
        endif
    endif
endif

# Default Target
all: $(TARGET)$(EXE)

$(TARGET)$(EXE): test_host.c
	$(CC) $(CFLAGS) test_host.c -o $(TARGET)$(EXE) $(LIBS)

# Clean Target
clean:
	$(RM) $(TARGET)$(EXE)

.PHONY: all clean
