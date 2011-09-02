project(StormLib)
cmake_minimum_required(VERSION 2.6)

set(SRC_FILES
           src/adpcm/adpcm.cpp
           src/huffman/huff.cpp
           src/jenkins/lookup3.c
           src/lzma/C/LzFind.c
           src/lzma/C/LzmaDec.c
           src/lzma/C/LzmaEnc.c
           src/pklib/explode.c
           src/pklib/implode.c
           src/sparse/sparse.cpp
           src/FileStream.cpp
           src/SBaseCommon.cpp
           src/SBaseFileTable.cpp
           src/SCompression.cpp
           src/SFileAddFile.cpp
           src/SFileAttributes.cpp
           src/SFileCompactArchive.cpp
           src/SFileCreateArchive.cpp
           src/SFileExtractFile.cpp
           src/SFileFindFile.cpp
           src/SFileListFile.cpp
           src/SFileOpenArchive.cpp
           src/SFileOpenFileEx.cpp
           src/SFilePatchArchives.cpp
           src/SFileReadFile.cpp
           src/SFileVerify.cpp
)

set(TEST_SRC_FILES
           test/Test.cpp
)

add_definitions(-D_7ZIP_ST -DBZ_STRICT_ANSI)

if(WIN32)
    if(MSVC)
        message(STATUS "Using MSVC")
        add_definitions(-D_7ZIP_ST -DWIN32)
    else()
        message(STATUS "Using mingw")
    endif()
endif()

if(APPLE)
    message(STATUS "Using Mac OS X port")
    set(LINK_LIBS z bz2)
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL Linux)
    message(STATUS "Using Linux port")
    set(LINK_LIBS z bz2 tomcrypt)
endif()

add_library(StormLib SHARED ${SRC_FILES})
target_link_libraries(StormLib ${LINK_LIBS})

add_library(StormLib_static STATIC ${SRC_FILES})
target_link_libraries(StormLib_static ${LINK_LIBS})
set_target_properties(StormLib_static PROPERTIES OUTPUT_NAME StormLib)

add_executable(StormLib_test ${TEST_SRC_FILES})
target_link_libraries(StormLib_test StormLib_static)

if(APPLE)
    set_target_properties(StormLib PROPERTIES FRAMEWORK true)
    set_target_properties(StormLib PROPERTIES PUBLIC_HEADER "src/StormLib.h src/StormPort.h")
    set_target_properties(StormLib PROPERTIES LINK_FLAGS "-framework Carbon")
endif()

if(UNIX)
    set_target_properties(StormLib PROPERTIES SOVERSION 0)
endif()

install(TARGETS StormLib StormLib_static RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(FILES src/StormLib.h src/StormPort.h DESTINATION include)
