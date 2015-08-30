ARCH=x86_64-linux
CXXFLAGS=-fPIC

ifeq (,1)
	SLIB = ./libvispictureit.so
else
	SLIB = ./pictureit.vis
endif

OBJS = pictureit.o mrfft.o

DEFINES += -DHAS_GL

DEBUG_LEVEL     = -g
EXTRA_CCFLAGS   = -Wall
CXXFLAGS        = $(DEBUG_LEVEL) $(EXTRA_CCFLAGS)
CCFLAGS         = $(CXXFLAGS)

ifeq ($(findstring osx,$(ARCH)), osx)
	LDFLAGS += -framework OpenGL
else
	LDFLAGS += -lGL
endif

$(SLIB): $(OBJS)
ifeq ($(findstring osx,$(ARCH)), osx)
	$(CXX) $(CXXFLAGS) $(CCFLAGS) $(LDFLAGS) -bundle -o $(SLIB) $(OBJS) -lSOIL
else
	$(CXX) $(CXXFLAGS) $(CCFLAGS) $(LDFLAGS) -shared -o $(SLIB) $(OBJS) -lSOIL
endif

include ../../../Makefile.include
