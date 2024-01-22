OS = LINUX
#OS = MACOSX
#OS = WINDOWS

ifeq ($(OS), LINUX)
TARGET = phazerville_screencapture
CXX = g++
WXCONFIG = ~/wxwidgets/3.1.4.gtk3.teensy/bin/wx-config
CPPFLAGS = -O2 -Wall `$(WXCONFIG) --cppflags` -D$(OS)
LIBS = `$(WXCONFIG) --libs` -ludev -lX11

else ifeq ($(OS), MACOSX)
TARGET = phazerville_screencapture.app
CXX = g++
WXCONFIG = ~/wxwidgets/3.1.0.mac64.teensy/bin/wx-config
CPPFLAGS = -O2 -Wall `$(WXCONFIG) --cppflags` -D$(OS)

else ifeq ($(OS), WINDOWS)
TARGET = phazerville_screencapture.exe
CXX = i686-w64-mingw32-g++
WXCONFIG = ~/wxwidgets/3.1.0.mingw.teensy/bin/wx-config
CPPFLAGS = -O2 -Wall `$(WXCONFIG) --cppflags` -D$(OS)

endif

OBJS = gui.o usb.o
HEADERS = gui.h usb.h wx_all.h

all: $(TARGET)

%.o: %.cpp $(HEADERS)
	$(CXX) -c $(CPPFLAGS) -o $@ $<

phazerville_screencapture: $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LIBS)

clean:
	rm -f *.o phazerville_screencapture
