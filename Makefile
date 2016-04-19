SHELL := /bin/bash
base_dir := $(CURDIR)

lib_dir := $(base_dir)/lib
log_dir := $(base_dir)/log
bin_dir := $(base_dir)/bin
bin_test_dir := $(bin_dir)/test

export base_dir
export lib_dir
export log_dir
export bin_dir
export bin_test_dir
export LDFLAGS

CXXFLAGS += -I$(base_dir)/src/lib/

# Enumeration of all of the library directories to compile for the project
third_party_libs := src/lib

# Enumeration of all executables for this project
midi_file_app := src/app/midi_file_app
#buffer_app := src/app/buffer_app

# Enumeration of all tests for this project
#buffer_test := src/test/buffer_test

# List containing all of the user libraries for the project
libraries := $(third_party_libs)

# List containing all of the user applications for the project
apps := $(midi_file_app)

# List containing all of the user tests for the project
#tests := $(buffer_test)

# List of all directories to build from
dirs := $(libraries) $(apps) $(tests)

.PHONY: all build dirs debug run test $(dirs) $(apps) $(libraries) $(tests)

all: build

install:
	$(MAKE) -C src/lib install

dirs:
	mkdir -p $(lib_dir)
	mkdir -p $(bin_dir)
	mkdir -p $(bin_test_dir)
	mkdir -p $(log_dir)

debug:
	$(eval LDFLAGS += -g)
	$(MAKE) build

build: dirs $(libraries) $(apps) $(tests)

$(apps):
	$(MAKE) -s -C $@

$(libraries):
	$(MAKE) -s -C $@

$(tests):
	$(MAKE) -s -C $@

run:
	#cd bin && ./buffer_app

clean:
	$(RM) -rf $(lib_dir)
	$(RM) -rf $(bin_test_dir)
	$(RM) -rf $(bin_dir)
	$(RM) -rf $(log_dir)
	-for DIR in $(dirs); do $(MAKE) -s -C $${DIR} clean; done
