file(MAKE_DIRECTORY "${TEST_DIRECTORY}")
set(output "${TEST_DIRECTORY}/preview.png")
set(environment "QT_QPA_PLATFORM=offscreen" "LD_PRELOAD=${FAILURE_LIBRARY}")

file(REMOVE "${output}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=QT_QPA_PLATFORMTHEME
    ${environment} BADOING_TEST_THEME_FAILURE=native
    "${PET_EXECUTABLE}" --render-preview "${output}"
    RESULT_VARIABLE result ERROR_VARIABLE error TIMEOUT 15)
if(NOT result EQUAL 0 OR NOT EXISTS "${output}" OR NOT error MATCHES "using Qt's built-in theme")
    message(FATAL_ERROR "A crashing native theme must fall back and render successfully (${result}): ${error}")
endif()

foreach(override IN ITEMS environment argument long-argument)
    set(theme_environment "QT_QPA_PLATFORMTHEME=generic")
    set(theme_arguments)
    if(override STREQUAL "argument")
        set(theme_environment --unset=QT_QPA_PLATFORMTHEME)
        set(theme_arguments -platformtheme generic)
    elseif(override STREQUAL "long-argument")
        set(theme_environment --unset=QT_QPA_PLATFORMTHEME)
        set(theme_arguments --platformtheme generic)
    endif()
    file(REMOVE "${output}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${theme_environment}
        ${environment} BADOING_TEST_THEME_FAILURE=all
        "${PET_EXECUTABLE}" ${theme_arguments} --render-preview "${output}"
        RESULT_VARIABLE result ERROR_VARIABLE error TIMEOUT 15)
    if(NOT result EQUAL 0 OR NOT EXISTS "${output}" OR error MATCHES "using Qt's built-in theme")
        message(FATAL_ERROR "An explicit theme must bypass automatic selection (${override}, ${result}): ${error}")
    endif()
endforeach()

file(REMOVE "${output}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=QT_QPA_PLATFORMTHEME
    ${environment} BADOING_TEST_THEME_FAILURE=all
    "${PET_EXECUTABLE}" --render-preview "${output}"
    RESULT_VARIABLE result ERROR_VARIABLE error TIMEOUT 15)
if(NOT result EQUAL 1 OR EXISTS "${output}" OR NOT error MATCHES "Built-in theme also failed")
    message(FATAL_ERROR "If both themes fail, startup must stop cleanly (${result}): ${error}")
endif()
