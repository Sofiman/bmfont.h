CC=gcc
CPPFLAGS=-Iinclude
CLFAGS=-Wall -Wextra -Wvla -Wextra -fsanitize=address -g
LDFLAGS=
LDLIBS=-fsanitize=address

BUILD=bin
TARGET=bmfont_codegen
CFILES=$(shell find src -name "*.c")
OFILES=$(CFILES:%.c=$(BUILD)/%.o)

SRCDIRS := $(shell find . -type d -not -path "*$(BUILD)*")
$(shell mkdir -p $(SRCDIRS:%=$(BUILD)/%))

$(TARGET): $(OFILES)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(BUILD)/%.o: %.c
	$(CC) -c -o $@ $< $(CPPFLAGS) $(CFLAGS)

clean:
	$(RM) $(TARGET) $(OFILES)
	$(RM) -rf $(BUILD)

.PHONY: clean
