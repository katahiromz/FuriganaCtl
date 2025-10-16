# FuriganaCtl.dll
add_library(FuriganaCtl SHARED FuriganaCtl.cpp)
target_link_libraries(FuriganaCtl PRIVATE libBaseTextBox furigana_gdi)
target_compile_definitions(FuriganaCtl PRIVATE UNICODE _UNICODE FURIGANA_CTL_EXPORT)
set_target_properties(FuriganaCtl PROPERTIES PREFIX "")

# libFuriganaCtl.a
add_library(libFuriganaCtl STATIC FuriganaCtl.cpp)
target_link_libraries(FuriganaCtl PRIVATE libBaseTextBox furigana_gdi)
target_compile_definitions(libFuriganaCtl PRIVATE UNICODE _UNICODE)
set_target_properties(libFuriganaCtl PROPERTIES PREFIX "")
