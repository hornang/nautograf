set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Force the system ninja path so vcpkg's parallel-configure does not pick up
# the broken ninja injected by the gnome SDK snap into PATH.
# find_program(NINJA ...) in vcpkg_cmake_configure respects the CMake cache,
# so pre-seeding it here bypasses the PATH search.
set(NINJA "/usr/bin/ninja" CACHE STRING "Path to ninja" FORCE)
