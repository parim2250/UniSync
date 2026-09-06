CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude -pthread
SRCDIR  = src
OBJDIR  = obj
BINDIR  = bin

all: $(BINDIR)/UniSync $(BINDIR)/UniSync-server $(BINDIR)/UniSync-client \
     $(BINDIR)/UniSync-file-receiver $(BINDIR)/UniSync-file-sender \
     $(BINDIR)/UniSync-discover

$(BINDIR)/UniSync: $(OBJDIR)/main.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/UniSync-server: $(OBJDIR)/server.o $(OBJDIR)/network.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/UniSync-client: $(OBJDIR)/client.o $(OBJDIR)/network.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/UniSync-file-receiver: $(OBJDIR)/file_receiver.o $(OBJDIR)/handler.o \
    $(OBJDIR)/network.o $(OBJDIR)/protocol.o $(OBJDIR)/progress.o \
    $(OBJDIR)/errors.o $(OBJDIR)/transfer.o $(OBJDIR)/logger.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/UniSync-file-sender: $(OBJDIR)/file_sender.o $(OBJDIR)/network.o \
    $(OBJDIR)/protocol.o $(OBJDIR)/progress.o $(OBJDIR)/errors.o \
    $(OBJDIR)/discovery.o $(OBJDIR)/transfer.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/UniSync-discover: $(OBJDIR)/discover_main.o $(OBJDIR)/discovery.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJDIR)/* $(BINDIR)/*

.PHONY: all clean