include $(base_dir)/src/common.mk

ld_libs := $(addprefix $(base_dir)/lib/, $(app_libs))

$(app): $(objs)
	$(CXX) $(CXXFLAGS) -o $@ $(objs) $(ld_libs) $(LDFLAGS)
	cp $@ $(bin_dir)
