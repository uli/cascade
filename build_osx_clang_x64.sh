mkdir build
mkdir "build/osx_clang64"
pushd "build/osx_clang64"
cmake -G"Xcode" ../../  -DDIST_MACOSX=1

xcodebuild -project Project.xcodeproj -configuration Release
popd 