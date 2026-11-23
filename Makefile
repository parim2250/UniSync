CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude -pthread -D_GNU_SOURCE -O2
SRCDIR  = src
OBJDIR  = obj
BINDIR  = bin

# shared library code
LIB = util ui network sha256 crypto logger history security discovery protocol transfer progress errors handler
LIBOBJ = $(LIB:%=$(OBJDIR)/%.o)
APPOBJ = $(OBJDIR)/main.o $(OBJDIR)/file_sender.o $(OBJDIR)/file_receiver.o $(OBJDIR)/discover_main.o

all: $(BINDIR)/UniSync links $(BINDIR)/UniSync-server $(BINDIR)/UniSync-client

# the one program: UniSync receive | send | devices | history ...
$(BINDIR)/UniSync: $(APPOBJ) $(LIBOBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

# old command names keep working (they are symlinks to UniSync)
links: $(BINDIR)/UniSync
	@cd $(BINDIR) && ln -sf UniSync UniSync-file-receiver && ln -sf UniSync UniSync-file-sender && ln -sf UniSync UniSync-discover

# layer-1 socket demos
$(BINDIR)/UniSync-server: $(OBJDIR)/server.o $(OBJDIR)/network.o $(OBJDIR)/crypto.o $(OBJDIR)/sha256.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/UniSync-client: $(OBJDIR)/client.o $(OBJDIR)/network.o $(OBJDIR)/crypto.o $(OBJDIR)/sha256.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c include/*.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

test: all
	bash tests/run_tests.sh

install: all
	install -m 755 $(BINDIR)/UniSync /usr/local/bin/UniSync

clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all links clean test install
