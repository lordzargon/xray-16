cmake_minimum_required(VERSION 3.22)

project(minilua C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_VERBOSE_MAKEFILE)
    set(CMAKE_EXECUTE_PROCESS_COMMAND_ECHO STDOUT)
endif()

separate_arguments(HOST_ACFLAGS)
separate_arguments(HOST_ALDFLAGS)

add_executable(minilua)

target_sources(minilua PRIVATE
	"${LUAJIT_DIR}/host/minilua.c"
)

set_target_properties(minilua PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
)

if (NOT MSVC)
	target_compile_options(minilua
		PRIVATE
		${HOST_ACFLAGS}
	)

	target_link_libraries(minilua
		PRIVATE
		$<$<BOOL:${LUA_USE_POSIX}>:m>
	)

	target_link_options(minilua
		PRIVATE
		${HOST_ALDFLAGS}
	)
else()
	target_compile_definitions(minilua PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
