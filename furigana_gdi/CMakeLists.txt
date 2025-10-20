add_library(furigana_gdi STATIC furigana_gdi.cpp char_judge.cpp)
target_compile_definitions(furigana_gdi PRIVATE UNICODE _UNICODE)
target_link_libraries(furigana_gdi kernel32 user32 gdi32)
