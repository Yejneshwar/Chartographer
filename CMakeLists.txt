cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("ChartoGrapher")

if(NOT isWorkspace)
	message("Adding GRPC and Protos...")
	include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/AddGrpcAndProtos.cmake)
	ADD_GRPC_AND_PROTOS("ProtoFiles" "ChartographMessenger")
endif()

set(GUI_PROJECT_NAME "ChartoGrapher")

FetchContent_Declare(
	${GUI_PROJECT_NAME}
	GIT_REPOSITORY https://github.com/Yejneshwar/AbstractEngine
	GIT_TAG        master
	BINARY_DIR     "${CMAKE_CURRENT_BINARY_DIR}/AbstractEngine"
	GIT_PROGRESS	TRUE
)

cmake_policy(SET CMP0076 NEW) #https://cmake.org/cmake/help/latest/policy/CMP0076.html The target_sources() command converts relative paths to absolute.
FetchContent_MakeAvailable(${GUI_PROJECT_NAME})

cmake_policy(SET CMP0079 NEW) #https://cmake.org/cmake/help/latest/policy/CMP0079.html target_link_libraries() allows use with targets in other directories.
#Creates a variable named TEST_GUI_SRC

target_sources(${GUI_PROJECT_NAME} PRIVATE "Chartographer.cpp")
target_compile_definitions(${GUI_PROJECT_NAME} PRIVATE LOG_MIN_LEVEL=LOG_DEBUG)
target_link_libraries(${GUI_PROJECT_NAME} grpc++)
target_link_libraries(${GUI_PROJECT_NAME} ChartoGraphMessenger)

set_target_properties(${GUI_PROJECT_NAME}
PROPERTIES 
RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${GUI_PROJECT_NAME} PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.