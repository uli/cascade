mkdir build
mkdir "build/vs2013"
pushd "build/vs2013"
cmake -G"Visual Studio 12" ../../  -DDIST_WIN32VS13=1 -DDIST_WINDOWS=1

"%VS120COMNTOOLS%\..\IDE\devenv.com"  Project.sln /build Release
popd 