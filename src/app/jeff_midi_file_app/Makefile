app := midi_file_app.fw
objs := midi_file_app.o

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
args := songs/forest-temple.midi
test: $(app)
	./$(app) $(args)
