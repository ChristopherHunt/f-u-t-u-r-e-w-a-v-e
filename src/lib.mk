include $(base_dir)/src/common.mk

$(lib): $(objs)
	ar rcs $@ $^
	cp $@ $(lib_dir)
