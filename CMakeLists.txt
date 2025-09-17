cmake_minimum_required(VERSION 3.16)
project(CapacityTester LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define PROGRAM macro as a C string literal
add_compile_definitions(PROGRAM="CapacityTester")

# Prefer Qt6, fallback to Qt5 if available
if(WIN32)
    set(QT_COMPONENTS Widgets Concurrent LinguistTools)
else()
    set(QT_COMPONENTS Widgets Concurrent DBus LinguistTools)
endif()

find_package(Qt6 COMPONENTS ${QT_COMPONENTS} QUIET)
if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    set(QT_PACKAGE_PREFIX Qt6)
else()
    find_package(Qt5 COMPONENTS ${QT_COMPONENTS} REQUIRED)
    set(QT_VERSION_MAJOR 5)
    set(QT_PACKAGE_PREFIX Qt5)
endif()

# Enable automoc and autorcc for Qt meta-object and resource handling
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Source and header files
file(GLOB SOURCES src/*.cpp)
file(GLOB HEADERS inc/*.hpp inc/*.h)

add_executable(capacity-tester ${SOURCES} ${HEADERS})

target_include_directories(capacity-tester PRIVATE inc)

target_link_libraries(capacity-tester
    ${QT_PACKAGE_PREFIX}::Widgets
    ${QT_PACKAGE_PREFIX}::Concurrent
)

if(UNIX AND NOT WIN32)
    target_link_libraries(capacity-tester ${QT_PACKAGE_PREFIX}::DBus udev)
    target_compile_definitions(capacity-tester PRIVATE HAVE_LIBUDEV)
endif()

# libusb support (optional)
# cmake .. -DLIBUSB1_INCLUDE_DIR="D:/libusb1/include" -DLIBUSB1_LIBRARY="D:/libusb1/lib/libusb-1.0.lib"
if(WIN32)
    if(LIBUSB1_INCLUDE_DIR AND LIBUSB1_LIBRARY)
        if(EXISTS "${LIBUSB1_INCLUDE_DIR}/libusb.h" AND EXISTS "${LIBUSB1_LIBRARY}")
            message(STATUS "libusb-1.0 found and will be linked!")
            target_include_directories(capacity-tester PRIVATE ${LIBUSB1_INCLUDE_DIR})
            target_link_libraries(capacity-tester ${LIBUSB1_LIBRARY})
            target_compile_definitions(capacity-tester PRIVATE HAVE_LIBUSB1)
        endif()
    endif()
else()
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBUSB1 libusb-1.0)
        if(LIBUSB1_FOUND)
            target_link_libraries(capacity-tester ${LIBUSB1_LIBRARIES})
            target_compile_definitions(capacity-tester PRIVATE HAVE_LIBUSB1)
        endif()
    endif()
endif()

# Windows-specific libraries
if(WIN32)
    target_link_libraries(capacity-tester setupapi)
endif()

# Resources
if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_resources(RESOURCES res/res.qrc res/lang.qrc)
    target_sources(capacity-tester PRIVATE ${RESOURCES})
else()
    qt5_add_resources(RESOURCES res/res.qrc res/lang.qrc)
    target_sources(capacity-tester PRIVATE ${RESOURCES})
endif()

# Translations TODO unfinished, need to move to res/

# Use wildcard to collect all .ts files
file(GLOB TS_FILES languages/*.ts)

# Generate .qm files from all .ts files
if(QT_VERSION_MAJOR EQUAL 6)
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# Copy .qm files to res/ before resources are built
#add_custom_target(copy_qm_files ALL
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QM_FILES} ${CMAKE_SOURCE_DIR}/res/
#    DEPENDS ${QM_FILES}
#    COMMENT "Copying .qm files to res/"
#)

# Fix for MSVC linker error C1900 (IL mismatch): disable /GL to avoid LTCG conflicts with prebuilt libs (libusb1)
if (MSVC)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

install(TARGETS capacity-tester DESTINATION bin)
