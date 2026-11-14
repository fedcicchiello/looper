add_executable(looper_tests
    main.cpp
    test_gesture.cpp
    test_state.cpp
    test_looper.cpp
)

target_link_libraries(looper_tests PRIVATE looper_core)

target_compile_options(looper_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

add_test(NAME looper_tests COMMAND looper_tests)
