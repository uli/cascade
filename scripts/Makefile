CXXFLAGS += -I..
LDFLAGS += ../os_linux.cpp ../os_serial_linux.cpp
SCRIPTS = dumpser
ifeq "$(findstring noftdi,$(QMAKE_RULES))" ""
CXXFLAGS += $(shell pkg-config --cflags libusb-1.0)
LDFLAGS += -lftdi
SCRIPTS += dumpftdi dumpserftdi
endif
all: $(SCRIPTS) iddump.exe
clean:
	rm -f $(SCRIPTS)
iddump.exe: iddump.cpp ../win32hwid.cpp
	i686-pc-mingw32-g++ -static -DVMP_DISABLE -I.. -o iddump.exe iddump.cpp ../win32hwid.cpp -liphlpapi
