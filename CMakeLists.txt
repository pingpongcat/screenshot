cmake_minimum_required(VERSION 3.10)
project(screenshotApp)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find packages for the required libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(DRM REQUIRED libdrm)
pkg_check_modules(JPEG REQUIRED libjpeg)

# Include directories for the found packages
include_directories(${CAIRO_INCLUDE_DIRS} ${DRM_INCLUDE_DIRS} ${JPEG_INCLUDE_DIRS})

# Add the executable
add_executable(screenshotApp main.cpp)

# Link the executable with the required libraries
target_link_libraries(screenshotApp ${CAIRO_LIBRARIES} ${DRM_LIBRARIES} ${JPEG_LIBRARIES})

# Include directories might also need to be specified for the linker
target_include_directories(screenshotApp PUBLIC ${CAIRO_INCLUDE_DIRS} ${DRM_INCLUDE_DIRS} ${JPEG_INCLUDE_DIRS})

# Linker flags
target_link_options(screenshotApp PUBLIC ${CAIRO_LDFLAGS_OTHER} ${DRM_LDFLAGS_OTHER} ${JPEG_LDFLAGS_OTHER})
