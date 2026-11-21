CC = gcc
AR = ar
CFLAGS = -std=c17 -O2 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS = -Iinclude -Iapp -finput-charset=UTF-8 -fexec-charset=UTF-8
LIB_OBJECTS = build/math_utils.o build/timing.o build/factorization.o build/analysis.o
APP_OBJECTS = build/main.o build/input_output.o
ifeq ($(OS),Windows_NT)
SHELL = cmd.exe
.SHELLFLAGS = /C
LDLIBS = -lshell32
MKDIR = if not exist build mkdir build
RUN_TEST = build\test_core.exe
else
LDLIBS = -lm
MKDIR = mkdir -p build
RUN_TEST = ./build/test_core.exe
endif

.PHONY: all test
all: build/rsa_analyzer.exe build/rsa_core.dll

build:
	$(MKDIR)

build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build/%.o: app/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build/librsa.a: $(LIB_OBJECTS)
	$(AR) rcs $@ $^

build/rsa_analyzer.exe: $(APP_OBJECTS) build/librsa.a
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

build/rsa_core.dll: build/gui_bridge.o build/input_output.o $(LIB_OBJECTS)
	$(CC) -shared -Wl,--export-all-symbols $(CFLAGS) $^ $(LDLIBS) -o $@

build/test_core.exe: tests/test_core.c build/librsa.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@

build/math_probe.exe: tests/math_probe.c build/librsa.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@

test: build/test_core.exe
	$(RUN_TEST)

-include $(LIB_OBJECTS:.o=.d) $(APP_OBJECTS:.o=.d)
