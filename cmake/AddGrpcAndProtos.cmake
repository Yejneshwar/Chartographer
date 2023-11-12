

include(FetchContent)

function(ADD_GRPC_AND_PROTOS PROTO_FOLDER_PATH PROTO_FILE_NAME)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FOLDER_PATH}/${PROTO_FILE_NAME}.proto")
        message("Proto file found ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FOLDER_PATH}/${PROTO_FILE_NAME}.proto")
    else()
        message(FATAL_ERROR "Proto file not found : ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FOLDER_PATH}/${PROTO_FILE_NAME}.proto")
    endif()

    set(CMAKE_CXX_STANDARD 20)
    set(ABSL_PROPAGATE_CXX_STD ON)
    set(ABSL_ENABLE_INSTALL ON)
    FetchContent_Declare(
      gRPC
      GIT_REPOSITORY https://github.com/grpc/grpc
      GIT_TAG        v1.59.2  # e.g v1.28.0
    )
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_MakeAvailable(gRPC)

    # Since FetchContent uses add_subdirectory under the hood, we can use
      # the grpc targets directly from this build.
      set(_PROTOBUF_LIBPROTOBUF libprotobuf)
      set(_REFLECTION grpc++_reflection)
      set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
      set(_GRPC_GRPCPP grpc++)
      if(CMAKE_CROSSCOMPILING)
        find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
      else()
        set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
      endif()

    message("Probobuf lib : ${_PROTOBUF_LIBPROTOBUF}")
    message("Probobuf protoc : ${_PROTOBUF_PROTOC}")
    message("Probobuf grpc++ : ${_GRPC_GRPCPP}")

    message("Grpc executable : ${_GRPC_CPP_PLUGIN_EXECUTABLE}")

    SET(PROTO_FILE_NAME "ChartoGraphMessenger")
    # Proto file
    get_filename_component(cm_proto "${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FOLDER_PATH}/${PROTO_FILE_NAME}.proto" ABSOLUTE)
    get_filename_component(cm_proto_path "${cm_proto}" PATH)

    # Generated sources
    set(cm_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_FILE_NAME}.pb.cc")
    set(cm_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_FILE_NAME}.pb.h")
    set(cm_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_FILE_NAME}.grpc.pb.cc")
    set(cm_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_FILE_NAME}.grpc.pb.h")
    add_custom_command(
          OUTPUT "${cm_proto_srcs}" "${cm_proto_hdrs}" "${cm_grpc_srcs}" "${cm_grpc_hdrs}"
          COMMAND ${_PROTOBUF_PROTOC}
          ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
            --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
            -I "${cm_proto_path}"
            --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
            "${cm_proto}"
          DEPENDS "${cm_proto}")


    set(MSG "Protoc path : ${_PROTOBUF_PROTOC} $'\\n'GRPC Plugin path : ${_GRPC_CPP_PLUGIN_EXECUTABLE}")
    add_custom_target(
      GenexMessages
      COMMAND ${CMAKE_COMMAND} -E echo ${MSG}
      VERBATIM
    )


    # cm_grpc_proto
    add_library(${PROTO_FILE_NAME}
      ${cm_grpc_srcs}
      ${cm_grpc_hdrs}
      ${cm_proto_srcs}
      ${cm_proto_hdrs})
    target_link_libraries(${PROTO_FILE_NAME}
      ${_REFLECTION}
      ${_GRPC_GRPCPP}
      ${_PROTOBUF_LIBPROTOBUF})
      add_dependencies(${PROTO_FILE_NAME} GenexMessages)
    # Include generated *.pb.h files
    target_include_directories(${PROTO_FILE_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
endfunction(ADD_GRPC_AND_PROTOS)