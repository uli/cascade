#QMAKE_RULES = noftdi
QMAKE_PC = /mnt/qt-4.7.4/bin/qmake
QMAKE_WM8650 = ../buildroot-cs/output/host/usr/bin/qmake
QMAKE_WIN32 = qmake -spec win32-g++-cross

SUBDIRS = wm8650 pc win32 scripts

all: $(SUBDIRS) patch
.PHONY: all clean $(SUBDIRS) patch win_demo

$(SUBDIRS):
	$(MAKE) -C $@ QMAKE_RULES="$(QMAKE_RULES)"

wm8650: wm8650/Makefile
pc: pc/Makefile
win32: win32/Makefile
pc/Makefile: hiscanemu.pro Makefile
	mkdir -p pc ; $(QMAKE_PC) CONFIG+="$(QMAKE_RULES) debug" -o $@
wm8650/Makefile: hiscanemu.pro Makefile
	mkdir -p wm8650 ; $(QMAKE_WM8650) CONFIG+="$(QMAKE_RULES) copyprot" -o $@
win32/Makefile: hiscanemu.pro Makefile
	mkdir -p win32 ; $(QMAKE_WIN32) CONFIG+="$(QMAKE_RULES) debug noftdi" -o $@
	sed -i 's,/usr/include,/usr/i686-pc-mingw32/sys-root/mingw/include,g' win32/Makefile*
	sed -i 's,/usr/lib64,/usr/i686-pc-mingw32/sys-root/mingw/lib,g' win32/Makefile*
	sed -i 's,i686-pc-mingw32-moc,moc,g' win32/Makefile*
	sed -i 's,-lQt\([A-Za-z]*\)d,-lQt\1d4,g' win32/Makefile*
	sed -i 's,-lQt\([A-Za-z]*\)\([^A-Za-z0-9]\),-lQt\14\2,g' win32/Makefile*

clean:
	for i in $(SUBDIRS) ; do $(MAKE) -C $$i clean ; done

win_dist:
	rm -f win32/Makefile
	$(MAKE) win32/Makefile QMAKE_RULES="noftdi"
	$(MAKE) -C win32 clean
	$(MAKE) -C win32 release
	sh scripts/package_win.sh win32/release/hiscanemu.exe
