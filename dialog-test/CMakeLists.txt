add_executable(dialog-test WIN32 dialog-test.cpp dialog-test_res.rc)
target_compile_definitions(dialog-test PRIVATE UNICODE _UNICODE)
target_link_libraries(dialog-test PRIVATE comctl32 shlwapi)
