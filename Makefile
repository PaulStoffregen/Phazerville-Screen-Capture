#OS = LINUX
OS = WINDOWS
#OS = MACOSX

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
WXCONFIG = ~/wxwidgets/3.2.4.mingw.teensy/bin/wx-config
CPPFLAGS = -O2 -Wall `$(WXCONFIG) --cppflags` -D$(OS)
LIBS = `$(WXCONFIG) --libs` -lhid -lsetupapi -static -static-libgcc -static-libstdc++

endif

OBJS = gui.o usb.o
HEADERS = gui.h usb.h wx_all.h

all: $(TARGET)

%.o: %.cpp $(HEADERS)
	$(CXX) -c $(CPPFLAGS) -o $@ $<

phazerville_screencapture: $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LIBS)

phazerville_screencapture.exe: $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LIBS)
	-~/teensy/td/cp_win32.sh $@


clean:
	rm -f *.o phazerville_screencapture phazerville_screencapture.exe
