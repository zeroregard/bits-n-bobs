if(NOT TARGET protobuf::protoc)
  add_executable(protobuf::protoc IMPORTED GLOBAL)
  set_target_properties(protobuf::protoc PROPERTIES IMPORTED_LOCATION "$ENV{HOME}/src/soes-deps/protoc/bin/protoc")
endif()
