ifeq ($(OS),Windows_NT)
echo 'ur on windows go home'
else
    UNAME_S := $(shell uname -s)
endif


.PHONY: all clean install midifile portmidi wildmidi install-midifile install-wildmidi install-portmidi

all: midifile portmidi wildmidi

install: install-midifile install-portmidi install-wildmidi

install-midifile:
	@echo "Building midiFile"
	$(MAKE) -s -C midifile clean
	$(MAKE) -s -C midifile library
	@echo "Done building midiFile"

install-portmidi:

install-wildmidi:
	@echo "Building wildmidi"
	cd wildmidi && cmake CMakeLists.txt;
	$(MAKE) -s -C wildmidi clean
	$(MAKE) -s -C wildmidi
	@echo "Done building wildmidi"

midifile:
	cp midifile/lib/*.a $(lib_dir)

portmidi:
	if [ "Darwin" == "$(UNAME)" ]; then \
		cp -f portmidi/portmidi_mac/217/lib/libportmidi.dylib $(lib_dir); \
	else \
		echo "This is not a Mac! Not setup to link PortMidi with Linux yet, ask Chris"; \
	fi

wildmidi:
ifeq ($(UNAME_S),Linux)
	cp wildmidi/wildmidi $(bin_dir)
	cp wildmidi/*.so* $(lib_dir)
endif
ifeq ($(UNAME_S),Darwin)
	cp wildmidi/wildmidi.app/Contents/MacOS/wildmidi $(bin_dir)
	cp wildmidi/*.dylib $(lib_dir)
endif
