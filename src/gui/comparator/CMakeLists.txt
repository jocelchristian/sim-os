add_executable(
    comparator
    ${CMAKE_SOURCE_DIR}/src/gui/comparator/Application.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/comparator/main.cpp
)
set_target_properties(comparator PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_include_directories(comparator PRIVATE "${CMAKE_SOURCE_DIR}/src/")
target_link_libraries(comparator PRIVATE sim-gui sim-util)
target_compile_definitions(comparator PRIVATE $<$<CONFIG:Debug>:DEBUG>)
target_compile_features(comparator PRIVATE cxx_std_23)
target_compile_options(comparator
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wconversion
        $<$<CONFIG:Debug>: -ggdb -O0 -fno-omit-frame-pointer>
        $<$<AND:$<CONFIG:Debug>,$<BOOL:${ENABLE_SANITIZERS}>>: -fsanitize=undefined,address,leak>
        $<$<CONFIG:Release>: -O3>)
target_link_options(comparator
    PRIVATE
        $<$<CONFIG:Debug>:>
        $<$<AND:$<CONFIG:Debug>,$<BOOL:${ENABLE_SANITIZERS}>>: -fsanitize=undefined,address,leak>
)
