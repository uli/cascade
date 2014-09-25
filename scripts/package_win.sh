#!/bin/sh
set -e
root="`pwd`"
version="`grep VERSION version.h|sed 's,.*"\(.*\)".*,\1,'`"
staging_dir=releases/$version/cascade_$version
staging_name=releases/$version/cascade_$version
rm -fr $staging_dir
mkdir -p $staging_dir
pushd $staging_dir
mkdir rec
mkdir roms
touch roms/"Put ROM files here"
mkdir save
mkdir scr
popd
cp -p $1 $staging_dir/CASCADE.exe
cp -p /usr/i686-pc-mingw32/sys-root/mingw/bin/{QtGui4,QtCore4,QtNetwork4,zlib1,libpng15-15,libstdc++-6,libgcc_s_sjlj-1}.dll $staging_dir/
cp -p ftd2xx_win32/ftd2xx.dll $staging_dir/
pushd $staging_dir
wine $root/bin/7za.exe a -sfx7zS2.sfx cascade_7z.exe *.exe *.dll
rm -f CASCADE.exe *.dll
mv cascade_7z.exe CASCADE.exe
popd
cp -p bin/{lha,unrar}.exe $staging_dir/
rm -f $staging_name.zip
pushd $staging_dir
cd ..
zip -r ${staging_name##*/}.zip ${staging_dir##*/}
popd
rm -fr $staging_dir
