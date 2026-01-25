cmake_minimum_required(VERSION 3.10)
project(libtnc C)
set(CMAKE_C_STANDARD 11)

include(GNUInstallDirs)

set(TNC_SOURCES
    src/buffer.c
    src/ax25.c
    src/kiss.c
    src/tnc2.c
    src/hldc.c
    src/crc.c
    src/tcp.c
    src/udp.c
    src/line.c
    src/conf.c
    src/common.c
)
add_library(tnc STATIC ${TNC_SOURCES})
target_include_directories(tnc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

install(TARGETS tnc
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Tests
add_executable(tnc_test test/main_test.c test/test.c)
target_link_libraries(tnc_test tnc m)
