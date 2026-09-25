# Runs one QML suite on Windows with its results in a file, then prints the
# file, so ctest's log carries the per-case lines whatever the runner does
# with its console handles.
# Inputs: RUNNER, INPUT, OUT
file(REMOVE "${OUT}")
execute_process(COMMAND "${RUNNER}" -input "${INPUT}" -o "${OUT},txt"
                RESULT_VARIABLE rc)
if(EXISTS "${OUT}")
    file(READ "${OUT}" _txt)
    message("${_txt}")
else()
    message("qmltestrunner wrote no results to ${OUT}")
endif()
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "qmltestrunner failed (${rc})")
endif()
