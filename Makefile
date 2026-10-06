CC ?= cc
AR ?= ar
CFLAGS ?= -O2 -Wall -Wextra -Werror
CPPFLAGS += -Iinclude
CORE := src/protocol.c src/ipv4_text.c src/stream.c src/host.c src/link.c src/discovery.c src/zlib.c
SOURCES := $(CORE) host/socket.c
HEADERS := $(wildcard include/amilan/*.h)
OBJECTS := $(patsubst %.c,build/%.o,$(SOURCES))
TESTS := protocol stream host discovery amiga zlib
.PHONY: all test test-sanitize check-amiga example clean
all: build/libamilan.a
build/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/libamilan.a: $(OBJECTS)
	$(AR) rcs $@ $^
build/test_protocol: tests/test_protocol.c src/protocol.c src/ipv4_text.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(filter %.c,$^)
build/test_stream: tests/test_stream.c tests/support.h $(filter-out src/discovery.c,$(CORE)) $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(filter %.c,$^)
build/test_host: tests/test_host.c tests/support.h $(SOURCES) $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(filter %.c,$^)
build/test_discovery: tests/test_discovery.c $(SOURCES) $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(filter %.c,$^)
build/test_amiga: tests/test_amiga.c amiga/socket.c src/protocol.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(filter %.c,$^)
build/test_zlib: tests/test_zlib.c src/zlib.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(filter %.c,$^)
# ctypes runs in the system Python process; sanitizer coverage uses test_zlib.
build/zlib.so: src/zlib.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) -O2 -Wall -Wextra -Werror -fPIC -shared -o $@ $<
test: $(addprefix build/test_,$(TESTS)) build/echo build/zlib.so
	@set -e; for t in $(TESTS); do ./build/test_$$t; done
	python3 tests/test_echo.py
	python3 tests/test_zlib.py
test-sanitize:
	$(MAKE) clean
	$(MAKE) test CFLAGS='-O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie'
M68K_CC ?= m68k-linux-gnu-gcc
M68K_AR ?= m68k-linux-gnu-ar
AMIGA_SRC := $(CORE) amiga/socket.c
AMIGA_OBJ := $(patsubst %.c,build/amiga/%.o,$(AMIGA_SRC))
build/amiga/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(M68K_CC) -Iinclude -m68020-60 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-pic -fno-common -fno-asynchronous-unwind-tables -c $< -o $@
build/amiga/libamilan.a: $(AMIGA_OBJ)
	$(M68K_AR) rcs $@ $^
check-amiga: build/amiga/libamilan.a
example: build/echo
build/echo: examples/echo.c build/libamilan.a $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< build/libamilan.a
clean:
	rm -rf build
