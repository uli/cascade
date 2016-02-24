cmake_minimum_required(VERSION 2.8)
set (projectName cascade)

set(QT_QMAKE_EXECUTABLE "c:/Qt/4.8.1-win32vs05/bin/qmake.exe")

find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})
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
 QT4_WRAP_CPP(MOC_SOURCES ui.h)
 list (APPEND ${projectName}_SRC ${MOC_SOURCES})
 
if(DIST_WIN32VS05)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/os_sdl.cpp)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/os_linux.cpp)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/os_serial_linux.cpp)
list(REMOVE_ITEM ${projectName}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/iface_kl_ftdi.cpp)
endif()


message(${${projectName}_SRC})
add_executable(${projectName} ${${projectName}_SRC})
if(DIST_WINDOWS)
target_link_libraries(${projectName} ${CMAKE_CURRENT_SOURCE_DIR}/ftd2xx_win32/ftd2xx.lib )
target_link_libraries(${projectName} Qt4::QtGui )
endif()

