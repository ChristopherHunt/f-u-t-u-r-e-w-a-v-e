CXX := g++
CXXFLAGS := -std=c++11 -g -Wall -D__LINUX_ALSA__
LDFLAGS := -lasound -lpthread
OBJS := midiout.o playmidi.o
EXES := midiout playmidi

all: $(EXES)

midiout: midiout.o
	$(CXX) $(CXXFLAGS) -o $@ $^ RtMidi.o $(LDFLAGS)

playmidi: playmidi.o
	$(CXX) $(CXXFLAGS) -o $@ $^ RtMidi.o $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^

.PHONY: clean cleanall
clean:
	rm -fv $(OBJS) *~
cleanall: clean
	rm -fv $(EXES)
