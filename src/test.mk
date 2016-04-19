include $(base_dir)/src/common.mk

ld_libs := $(addprefix $(base_dir)/lib/, $(test_libs))

$(test): $(ldlibs) $(objs)
	$(CXX) $(CXXFLAGS) -o $@ $(objs) $(LDFLAGS) $(ld_libs)
	cp $@ $(bin_test_dir)
	./$@ &> $@.log
	cp $@.log $(log_dir)
