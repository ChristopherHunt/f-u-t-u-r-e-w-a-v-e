app := midi_file_stats.fw
objs := midi_file_stats.o

app_libs := libmidifile.a

LDFLAGS += -lportmidi

ifndef base_dir
SHELL := /bin/bash
base_dir = $(shell echo $${PWD%%/src/*})
#$(error Setting base_dir to $(base_dir))
lib_dir := $(base_dir)/lib
log_dir := $(base_dir)/log
bin_dir := $(base_dir)/bin
bin_test_dir := $(bin_dir)/test
endif
include $(base_dir)/src/app.mk

.PHONY: test
test: mario forest
mario_args := --file songs/mario_overworld.mid
mario: $(app)
	./$(app) $(mario_args)
forest_args := --file songs/forest-temple.midi
forest: $(app)
	./$(app) $(forest_args)
