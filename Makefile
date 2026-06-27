# ============================================================================
# Makefile — PortScannerUI
# ============================================================================
# Auto-detects the OS and compiles the appropriate platform layer.
#   make       — release build
#   make debug — debug build with -g
#   make run   — build and run
#   make clean — remove build artefacts
# ============================================================================

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -Wpedantic -Werror

SRCDIR   = src
OBJDIR   = obj
BINDIR   = bin

# --- OS detection ---
ifeq ($(OS),Windows_NT)
    PLATFORM_SRC = $(SRCDIR)/platform_win32.c
    LDFLAGS      = -lws2_32
else
    PLATFORM_SRC = $(SRCDIR)/platform_linux.c
    LDFLAGS      = -lpthread
endif

SRCS     = $(SRCDIR)/main.c $(SRCDIR)/scanner.c $(SRCDIR)/ui.c $(SRCDIR)/utils.c $(PLATFORM_SRC)
OBJS     = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TARGET   = $(BINDIR)/portscanner$(if $(filter Windows_NT,$(OS)),.exe,)

# ---- Release build ----------------------------------------------------------

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -O2 -c -o $@ $<

$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -O2 -o $@ $^ $(LDFLAGS)
	@echo "Build: $@"

all: $(TARGET)

# ---- Debug build ------------------------------------------------------------

DEBUG_OBJS = $(OBJS:$(OBJDIR)/%.o=$(OBJDIR)/%.debug.o)

$(OBJDIR)/%.debug.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -O0 -g -DDEBUG -c -o $@ $<

$(TARGET:%=%-debug): $(DEBUG_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -O0 -g -o $@ $^ $(LDFLAGS)
	@echo "Build: $@"

debug: $(TARGET:%=%-debug)

# ---- Directories ------------------------------------------------------------

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# ---- Run ---------------------------------------------------------------------

run: $(TARGET)
	$(TARGET)

# ---- Clean -------------------------------------------------------------------

clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all debug run clean
