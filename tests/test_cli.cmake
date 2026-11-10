if(NOT DEFINED PET_EXECUTABLE OR NOT DEFINED TEST_DIRECTORY)
    message(FATAL_ERROR "PET_EXECUTABLE and TEST_DIRECTORY are required")
endif()

file(MAKE_DIRECTORY "${TEST_DIRECTORY}")
set(preview "${TEST_DIRECTORY}/preview with spaces.png")
file(REMOVE "${preview}")
execute_process(COMMAND "${PET_EXECUTABLE}" --render-preview "${preview}"
    RESULT_VARIABLE result ERROR_VARIABLE error TIMEOUT 15)
if(NOT result EQUAL 0 OR NOT EXISTS "${preview}")
    message(FATAL_ERROR "Preview export failed (${result}): ${error}")
endif()
file(READ "${preview}" signature LIMIT 8 HEX)
if(NOT signature STREQUAL "89504e470d0a1a0a")
    message(FATAL_ERROR "Preview output is not a PNG: ${signature}")
endif()

execute_process(COMMAND "${PET_EXECUTABLE}" --render-preview "${preview}" --reset-settings
    RESULT_VARIABLE result ERROR_VARIABLE error TIMEOUT 15)
if(NOT result EQUAL 2)
    message(FATAL_ERROR "Preview combined with reset must exit 2, got ${result}: ${error}")
endif()

execute_process(COMMAND "${PET_EXECUTABLE}" --render-preview "${TEST_DIRECTORY}"
    RESULT_VARIABLE result ERROR_VARIABLE error TIMEOUT 15)
if(NOT result EQUAL 1)
    message(FATAL_ERROR "A directory used as a preview file must exit 1, got ${result}: ${error}")
endif()
