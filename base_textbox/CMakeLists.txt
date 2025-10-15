# base_textbox.dll
add_library(base_textbox SHARED base_textbox.cpp)
target_compile_definitions(base_textbox PRIVATE UNICODE _UNICODE BASE_TEXTBOX_EXPORT)
set_target_properties(base_textbox PROPERTIES PREFIX "")
