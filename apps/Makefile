CROSS_COMPILE ?=

CC := $(CROSS_COMPILE)gcc

CFLAGS += -Wall -O2
LDFLAGS +=

SRCS := $(wildcard *.c)

TARGETS := $(SRCS:.c=)

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean