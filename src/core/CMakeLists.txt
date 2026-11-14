add_library(looper_core STATIC
    GestureDetector.cpp
    LoopEngine.cpp
    Looper.cpp
    StateMachine.cpp
)

target_include_directories(looper_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(looper_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)
