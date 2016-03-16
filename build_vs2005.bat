mkdir build
mkdir "build/vs2005"
pushd "build/vs2005"
cmake -G"Visual Studio 8 2005" ../../  -DDIST_WIN32VS05=1 -DDIST_WINDOWS=1

"%VS80COMNTOOLS%\..\IDE\devenv.com"  Project.sln /build Release
popd 