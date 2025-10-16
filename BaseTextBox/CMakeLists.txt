# BaseTextBox.dll
add_library(BaseTextBox SHARED BaseTextBox.cpp)
target_compile_definitions(BaseTextBox PRIVATE UNICODE _UNICODE BASE_TEXTBOX_EXPORT)
set_target_properties(BaseTextBox PROPERTIES PREFIX "")

# libBaseTextBox.a
add_library(libBaseTextBox STATIC BaseTextBox.cpp)
target_compile_definitions(libBaseTextBox PRIVATE UNICODE _UNICODE)
set_target_properties(libBaseTextBox PROPERTIES PREFIX "")
