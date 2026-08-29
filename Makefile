# UniSync Makefile — Layer 1
# Builds: bin/unisync, bin/unisync-server, bin/unisync-client

CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude
SRCDIR  = src
OBJDIR  = obj
BINDIR  = bin

# ── Targets ──────────────────────────────────────────────

all: $(BINDIR)/UniSync $(BINDIR)/UniSync-server $(BINDIR)/UniSync-client

# Main entry point (your original "UniSync v0.1 initialized")
$(BINDIR)/UniSync: $(OBJDIR)/main.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

# TCP server
$(BINDIR)/UniSync-server: $(OBJDIR)/server.o $(OBJDIR)/network.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

# TCP client
$(BINDIR)/UniSync-client: $(OBJDIR)/client.o $(OBJDIR)/network.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

# ── Pattern Rule: compile any .c in src/ to .o in obj/ ──

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Directory creation ───────────────────────────────────

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# ── Cleanup ──────────────────────────────────────────────

clean:
	rm -rf $(OBJDIR)/* $(BINDIR)/*

.PHONY: all clean