OS = LINUX
#OS = WINDOWS
#OS = MACOS

ifeq ($(OS), LINUX)
TARGET = phazerville_screencapture
CXX = g++
WXCONFIG = ~/wxwidgets/3.2.6.gtk3.teensy/bin/wx-config
CPPFLAGS = -O2 -Wall `$(WXCONFIG) --cppflags` -D$(OS)
LIBS = `$(WXCONFIG) --libs` -ludev -lX11

else ifeq ($(OS), MACOS)
TARGET = phazerville_screencapture.app
CXX = g++
WXCONFIG = ~/wxwidgets/3.2.6.macos.teensy/bin/wx-config
ARCH = -mmacosx-version-min=10.10 -arch x86_64 -arch arm64
CPPFLAGS = -O2 -Wall `$(WXCONFIG) --cppflags` $(ARCH) -D$(OS) -Wno-c++11-extensions
LIBS = `$(WXCONFIG) --libs` $(ARCH) -framework IOKit -framework CoreFoundation

else ifeq ($(OS), WINDOWS)
TARGET = phazerville_screencapture.exe
CXX = i686-w64-mingw32-g++
WINDRES = i686-w64-mingw32-windres
WXCONFIG = ~/wxwidgets/3.2.6.mingw.teensy/bin/wx-config
CPPFLAGS = -O2 -Wall `$(WXCONFIG) --cppflags` -D$(OS)
LIBS = `$(WXCONFIG) --libs` -lhid -lsetupapi -static -static-libgcc -static-libstdc++

endif

OBJS = gui.o usb.o
HEADERS = gui.h usb.h wx_all.h

all: $(TARGET)

%.o: %.cpp $(HEADERS)
	$(CXX) -c $(CPPFLAGS) -o $@ $<

phazerville_screencapture: $(OBJS)
	$(CXX) -s $(OBJS) -o $@ $(LIBS)

phazerville_screencapture.exe: resource.o $(OBJS)
	$(CXX) -s resource.o $(OBJS) -o $@ $(LIBS)
	-pjrcwinsigntool $@
	-~/teensy/td/cp_win32.sh $@

resource.o: icon/resource.rc icon/icon.ico
	$(WINDRES) -o resource.o icon/resource.rc

phazerville_screencapture.app: phazerville_screencapture $(OBJS) Info.plist
	mkdir -p $@/Contents/MacOS
	strip phazerville_screencapture
	cp phazerville_screencapture $@/Contents/MacOS
	mkdir -p $@/Contents/Resources/English.lproj
	cp Info.plist $@/Contents
	cp icon/icon.png $@/Contents/Resources
	/bin/echo -n 'APPL????' > $@/Contents/PkgInfo
	-pjrcmacsigntool $@
	-scp -r phazerville_screencapture.app macair:

clean:
	rm -f *.o phazerville_screencapture phazerville_screencapture.exe
	rm -f phazerville_screencapture.exe.sign*
	rm -rf phazerville_screencapture.app phazerville_screencapture.zip
