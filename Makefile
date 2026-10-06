CC ?= cc
AR ?= ar
CFLAGS ?= -O2 -Wall -Wextra -Werror
CPPFLAGS += -Iinclude
CORE := src/protocol.c src/ipv4_text.c src/stream.c src/host.c src/link.c
SOURCES := $(CORE) host/socket.c
OBJECTS := $(patsubst %.c,build/%.o,$(SOURCES))
.PHONY: all test clean
all: build/libamilan.a
build/%.o: %.c $(wildcard include/amilan/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/libamilan.a: $(OBJECTS)
	$(AR) rcs $@ $^
clean:
	rm -rf build

build/test_stream: tests/test_stream.c tests/support.h $(CORE) $(wildcard include/amilan/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_stream.c $(CORE)
test: build/test_stream build/test_host
	./build/test_stream
	./build/test_host

build/test_host: tests/test_host.c tests/support.h $(SOURCES) $(wildcard include/amilan/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_host.c $(SOURCES)
