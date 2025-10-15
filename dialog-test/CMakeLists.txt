add_executable(dialog-test WIN32 dialog-test.cpp dialog-test_res.rc)
target_link_libraries(dialog-test PRIVATE comctl32 shlwapi)
