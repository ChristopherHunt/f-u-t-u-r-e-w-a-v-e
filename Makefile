SHELL := /bin/bash
base_dir := $(CURDIR)

lib_dir := $(base_dir)/lib
log_dir := $(base_dir)/log
bin_dir := $(base_dir)/bin
bin_test_dir := $(bin_dir)/test

ifeq ($(OS),Windows_NT)
echo 'ur on windows go home'
else
    UNAME_S := $(shell uname -s)
endif

export base_dir
export lib_dir
export log_dir
export bin_dir
export bin_test_dir
export LDFLAGS
export UNAME_S

CXXFLAGS += -I$(base_dir)/src/lib/

# Enumeration of all of the library directories to compile for the project
third_party_libs := src/lib
network_lib := src/lib/network
client_lib := src/lib/client
server_lib := src/lib/server

# Enumeration of all executables for this project
midi_file_app := src/app/midi_file_app
client_app := src/app/client_app
server_app := src/app/server_app

# Enumeration of all tests for this project
#test_example := src/test/test_example

# List containing all of the user libraries for the project
libraries := $(network_lib) $(client_lib) $(server_lib) $(third_party_libs)

# List containing all of the user applications for the project
apps := $(client_app) $(server_app) $(midi_file_app) 

# List containing all of the user tests for the project
#tests := $(test_example)

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

build: clean dirs $(libraries) $(apps) $(tests)

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
