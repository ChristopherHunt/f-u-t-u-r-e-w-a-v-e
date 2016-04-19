UNAME_S := $(shell uname -s)

.PHONY: all clean install midifile wildmidi install-midifile install-wildmidi install-portmidi

all: midifile wildmidi

install: install-midifile install-wildmidi

install-midifile: 
	@echo "Building midiFile"
	$(MAKE) -s -C midifile clean
	$(MAKE) -s -C midifile library
	@echo "Done building midiFile"

install-portmidi:
	@echo "Building portmidi"
	#ifeq ($(UNAME_S),Linux)
	#	CCFLAGS += -D LINUX
	#endif
	#ifeq ($(UNAME_S),Darwin)
	#	CCFLAGS += -D OSX
	#endif
	@echo "Done building portmidi"

install-wildmidi:
	@echo "Building wildmidi"
	$(shell cd wildmidi && cmake CMakeLists.txt)
	$(MAKE) -s -C wildmidi clean
	$(MAKE) -s -C wildmidi
	@echo "Done building wildmidi"

midifile:
	cp midifile/lib/*.a $(lib_dir)

portmidi:

wildmidi:
	cp wildmidi/wildmidi.app/Contents/MacOS/wildmidi $(bin_dir)
	cp wildmidi/*.dylib $(lib_dir)
