# Generate files

# Find all relevant files
FILES := $(shell find src include man -type f)

.PHONY: all
all: $(FILES)

generate/%.sh:
	touch $@

include/configuration/label_information.h: generate/configuration__label_information.h.sh \
 generate/fensterchef.labels generate/fensterchef.data_types
	./generate/generate.sh $<

include/configuration/structure.h: generate/configuration__structure.h.sh \
 generate/fensterchef.labels generate/fensterchef.data_types
	./generate/generate.sh $<

include/configuration/label.h: generate/configuration__label.h.sh \
 generate/fensterchef.labels generate/fensterchef.data_types
	./generate/generate.sh $<

src/configuration/default.c: generate/configuration__default.c.sh \
 generate/fensterchef.labels generate/fensterchef.data_types
	./generate/generate.sh $<

man/fensterchef.5: generate/fensterchef.5.sh \
 include/configuration/action.h
	./generate/generate.sh $<
