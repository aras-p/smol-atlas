cmake_minimum_required (VERSION 3.8)

project ("smol-atlas")

add_executable (smol-atlas)
target_compile_definitions(smol-atlas PRIVATE _CRT_SECURE_NO_WARNINGS)
target_sources(smol-atlas PRIVATE
	src/smol-atlas.cpp
	src/smol-atlas.h
	test/main.cpp
	test/smol-atlas-test.cpp
	external/stb_rect_pack.h
    external/mapbox-shelf-pack-cpp/include/mapbox/shelf-pack.hpp
	external/andrewwillmott_rectallocator/RectAllocator.cpp
	external/andrewwillmott_rectallocator/RectAllocator.h
)
target_compile_features(smol-atlas PRIVATE cxx_std_17)

if (WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
	target_link_libraries(smol-atlas "${CMAKE_SOURCE_DIR}/external/etagere/etagere.dll.lib")
	add_custom_command(TARGET smol-atlas POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/external/etagere/etagere.dll" $<TARGET_FILE_DIR:smol-atlas>)
	target_compile_definitions(smol-atlas PRIVATE HAVE_ETAGERE=1)
elseif (APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
	target_link_libraries(smol-atlas "${CMAKE_SOURCE_DIR}/external/etagere/etagere.dylib")
	target_compile_definitions(smol-atlas PRIVATE HAVE_ETAGERE=1)
endif()
