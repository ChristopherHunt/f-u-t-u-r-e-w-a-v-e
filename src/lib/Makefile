.PHONY: all clean install midifile wildmidi install-midifile install-wildmidi

all: midifile wildmidi

install: install-midifile install-wildmidi

install-midifile: 
	@echo "Building midiFile"
	$(MAKE) -s -C midifile clean
	$(MAKE) -s -C midifile library
	@echo "Done building midiFile"

install-wildmidi:
	@echo "Building wildmidi"
	$(shell cd wildmidi && cmake CMakeLists.txt)
	$(MAKE) -s -C wildmidi clean
	$(MAKE) -s -C wildmidi
	@echo "Done building wildmidi"

midifile:
	cp midifile/lib/*.a $(lib_dir)

wildmidi:
	cp wildmidi/wildmidi.app/Contents/MacOS/wildmidi $(bin_dir)
	cp wildmidi/*.dylib $(lib_dir)
