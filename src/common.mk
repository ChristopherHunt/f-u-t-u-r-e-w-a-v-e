CXXFLAGS := -O3 -Wall -std=c++0x

includes += -I$(base_dir)/src/lib/

to_build := $(app) $(lib) $(test)

.PHONY: default clean

default: $(to_build)

clean:
	$(RM) *.o
	$(RM) $(to_build)

%.o: %.c
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(includes) -o $@ -c $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(includes) -o $@ -c $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(includes) -o $@ -c $<
