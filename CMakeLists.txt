cmake_minimum_required (VERSION 3.8)

project ("smol-atlas")

add_executable (smol-atlas)
target_compile_definitions(smol-atlas PRIVATE _CRT_SECURE_NO_WARNINGS)
target_sources(smol-atlas PRIVATE
	src/smol-atlas.h
	test/main.cpp
    external/mapbox-shelf-pack-cpp/include/mapbox/shelf-pack.hpp
)
target_compile_features(smol-atlas PRIVATE cxx_std_17)

if (WIN32)
    # RelWithDebInfo uses /Ob1 only :/
    target_compile_options(smol-atlas PRIVATE /Ob2)
endif()
