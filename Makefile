# C compiler
CC := gcc

# Packages
PACKAGES := x11 xcb xcb-image xcb-randr xcb-icccm xcb-keysyms xcb-event xcb-render freetype2 fontconfig

# Packages only used within tests and not the end build
TEST_PACKAGES := xcb-errors

# Compiler flags
DEBUG_FLAGS := -DDEBUG -g -fsanitize=address -pg
C_FLAGS := -Iinclude -std=c99 $(shell pkg-config --cflags $(PACKAGES)) -Wall -Wextra -Wpedantic -Werror -Wno-format-zero-length
RELEASE_FLAGS := -O3

# Libraries
C_LIBS := $(shell pkg-config --libs $(PACKAGES))

# Installation paths
LICENSE := /usr/share/licenses/fensterchef
BINARY := /usr/bin/fensterchef
MANUAL_PAGE_1 := /usr/share/man/man1/fensterchef.1.gz
MANUAL_PAGE_5 := /usr/share/man/man5/fensterchef.5.gz

# Find all source files
SOURCES := $(shell find src -name '*.c')
# Get all corresponding object paths
OBJECTS := $(patsubst src/%.c,build/%.o,$(SOURCES))
# Get all include files
INCLUDES := $(shell find include -name '*.h')

# Find all test sources
TEST_SOURCES := $(shell test -d tests && find tests -name '*.c')
# Get the corresponding output binaries
TESTS := $(patsubst tests/%.c,build/tests/%,$(TEST_SOURCES))
# All objects but the main object
OBJECTS_WITHOUT_MAIN := $(filter-out build/main.o,$(OBJECTS))
# Get the corresponding depency files
TEST_DEPENDENCIES := $(patsubst %,%.d,$(TESTS))

# Get dependencies
DEPENDENCIES := $(patsubst %.o,%.d,$(OBJECTS))

# Sandbox parameters
SANDBOX_DISPLAY := 8
SANDBOX := Xephyr :$(SANDBOX_DISPLAY) +extension RANDR -br -ac -noreset -screen 800x600

.PHONY: default
default: build

# Generate files
generate/configuration_parser_label.h.sh: generate/fensterchef.labels \
 generate/fensterchef.data_types
	touch $@

generate/configuration_parser_label_information.h.sh: generate/fensterchef.labels \
 generate/fensterchef.data_types
	touch $@

generate/configuration_structure.h.sh: generate/fensterchef.labels \
 generate/fensterchef.data_types
	touch $@

generate/default_configuration.c.sh: generate/fensterchef.labels \
 generate/fensterchef.data_types
	touch $@

generate/fensterchef.5.sh: include/action.h
	touch $@

generate/data_type.h.sh: generate/fensterchef.data_types
	touch $@

generate/data_type.c.sh: generate/fensterchef.data_types
	touch $@

src/%: generate/%.sh
	./generate/generate.sh $<

include/bits/%: generate/%.sh
	./generate/generate.sh $<

include/%: generate/%.sh
	./generate/generate.sh $<

man/%: generate/%.sh
	./generate/generate.sh $<

# Include dependencies (.d files) generated by gcc
-include $(DEPENDENCIES)
# Build each object from corresponding source file
build/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(DEBUG_FLAGS) $(C_FLAGS) -c $< -o $@ -MMD

# Build the main executable from all object files
build/fensterchef: $(OBJECTS)
	mkdir -p $(dir $@)
	$(CC) $(DEBUG_FLAGS) $(C_FLAGS) $(OBJECTS) -o $@ $(C_LIBS)

# Tests
.PHONY: tests

# Include dependencies (.d files) generated by the compiler
-include $(TEST_DEPENDENCIES)
# Build the test objects
build/tests/%.o: tests/%.c
	mkdir -p $(dir $@)
	$(CC) $(DEBUG_FLAGS) $(shell pkg-config --cflags $(TEST_PACKAGES)) $(C_FLAGS) -c $< -o $@ -MMD

# Build the test executables
build/tests/%: build/tests/%.o $(OBJECTS_WITHOUT_MAIN)
	$(CC) $(DEBUG_FLAGS) $(C_FLAGS) $(OBJECTS_WITHOUT_MAIN) $< -o $@ $(C_LIBS) $(shell pkg-config --libs $(TEST_PACKAGES))

tests: $(INCLUDES) $(TESTS)

# Manual pages
release/fensterchef.1.gz: man/fensterchef.1
	mkdir -p release
	gzip --best -c man/fensterchef.1 >$@

release/fensterchef.5.gz: man/fensterchef.5
	mkdir -p release
	gzip --best -c man/fensterchef.5 >$@

# Release executable
release/fensterchef: $(SOURCES) $(INCLUDES)
	mkdir -p release
	gcc $(RELEASE_FLAGS) $(C_FLAGS) $(SOURCES) -o release/fensterchef $(C_LIBS)

# Functions
.PHONY: build sandbox release install uninstall clean

build: build/fensterchef

sandbox: build
	$(SANDBOX) &
	# wait for x server to start
	sleep 1
	DISPLAY=:$(SANDBOX_DISPLAY) gdb -ex run --args ./build/fensterchef --verbose
	pkill Xephyr

release: release/fensterchef.1.gz release/fensterchef.5.gz release/fensterchef

install: release
	install -Dm644 -t "/usr/share/licenses/fensterchef" LICENSE.txt
	install -Dt "/usr/bin" "release/fensterchef"
	install -Dm644 -t "/usr/share/man/man1" release/fensterchef.1.gz
	install -Dm644 -t "/usr/share/man/man5" release/fensterchef.5.gz

uninstall:
	rm -r $(LICENSE)
	rm $(BINARY)
	rm $(MANUAL_PAGE_1)
	rm $(MANUAL_PAGE_5)

clean:
	rm -rf build
	rm -rf release
