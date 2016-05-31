app := server_app.fw
objs := server_app.o

app_libs := server.a libmidifile.a network.a 

LDFLAGS += -lportmidi

include $(base_dir)/src/app.mk
