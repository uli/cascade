cmake_minimum_required(VERSION 2.8)
set (projectName cascade)

if(DIST_WIN32VS05)
set(QT_QMAKE_EXECUTABLE "c:/Qt/4.8.1-win32vs05/bin/qmake.exe")

find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})
endif()

if(DIST_WIN32VS13)
set(CMAKE_PREFIX_PATH "c:/Qt/5.5/msvc2013")

find_package (Qt5Widgets REQUIRED) 
endif()

if(DIST_MACOSX)
set(CMAKE_PREFIX_PATH "/Volumes/developer/Qt/5.5/clang_64/")

find_package (Qt5Widgets REQUIRED)	
endif()


if(DIST_WINDOWS)
add_definitions(-DDIST_WINDOWS)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR} )
if(DIST_WIN32VS05)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/vs2005)
endif()

file(GLOB ${projectName}_SRC
    "*.h"
    "*.h"
    "*.cpp"
) 

set(CMAKE_AUTOMOC TRUE)
 
if(DIST_WINDOWS)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/os_sdl.cpp)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/os_linux.cpp)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/os_serial_linux.cpp)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/os_serial_macosx.cpp)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/iface_kl_ftdi.cpp)
endif()

if(DIST_MACOSX)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/os_sdl.cpp)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/os_serial_win32.cpp)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/os_win32.cpp)

#list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/os_linux.cpp)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/os_serial_linux.cpp)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/iface_kl_ftdi.cpp)
endif()


message(${${projectName}_SRC})
add_executable(${projectName} ${${projectName}_SRC})

if(DIST_WINDOWS)
	target_link_libraries(${projectName} ${CMAKE_CURRENT_SOURCE_DIR}/ftd2xx_win32/ftd2xx.lib )
	if(DIST_WIN32VS05)
		target_link_libraries(${projectName} Qt4::QtGui )
	else()
		qt5_use_modules(${projectName} Widgets)
	endif()
elseif(DIST_MACOSX)
	qt5_use_modules(${projectName} Widgets)	
endif()


