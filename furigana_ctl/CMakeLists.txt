# furigana_ctl.dll
add_library(furigana_ctl SHARED furigana_ctl.cpp)
target_link_libraries(furigana_ctl PRIVATE libbase_textbox furigana_gdi)
target_compile_definitions(furigana_ctl PRIVATE UNICODE _UNICODE FURIGANA_CTL_EXPORT)
set_target_properties(furigana_ctl PROPERTIES PREFIX "")

# libfurigana_ctl.a
add_library(libfurigana_ctl STATIC furigana_ctl.cpp)
target_link_libraries(furigana_ctl PRIVATE libbase_textbox furigana_gdi)
target_compile_definitions(libfurigana_ctl PRIVATE UNICODE _UNICODE)
set_target_properties(libfurigana_ctl PROPERTIES PREFIX "")
