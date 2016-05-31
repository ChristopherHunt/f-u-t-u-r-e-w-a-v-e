app := midi_file_app.fw
objs := midi_file_app.o

app_libs := libmidifile.a

LDFLAGS += -lportmidi -lporttime

ifeq ($(UNAME_S),Darwin)
    LDFLAGS :=
endif


# LDFLAGS += -lportmidi -lporttime

include $(base_dir)/src/app.mk
