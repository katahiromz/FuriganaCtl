add_library(furigana_gdi STATIC furigana_gdi.cpp char_judge.cpp)
target_compile_definitions(furigana_gdi PRIVATE UNICODE _UNICODE)
