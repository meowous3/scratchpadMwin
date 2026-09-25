# The copy fallback must build the SAME curated set as the symlinks: base
# module files, the allowed submodules, and none of the denied ones.
# Inputs: QT_QML_DIR, OUT_DIR, GENERATOR
execute_process(COMMAND ${CMAKE_COMMAND}
                        -DQT_QML_DIR=${QT_QML_DIR} -DOUT_DIR=${OUT_DIR} -DMELO_FORCE_COPY=ON
                        -P ${GENERATOR}
                RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "generator failed (${rc})")
endif()
foreach(f QtQuick/qmldir QtQml/qmldir QtQuick/Shapes/qmldir)
    if(NOT EXISTS "${OUT_DIR}/${f}")
        message(FATAL_ERROR "missing ${f}")
    endif()
    if(IS_SYMLINK "${OUT_DIR}/${f}")
        message(FATAL_ERROR "${f} is a symlink, expected a copy")
    endif()
endforeach()
if(IS_SYMLINK "${OUT_DIR}/QtQuick/Shapes")
    message(FATAL_ERROR "QtQuick/Shapes is a symlink, expected a copied directory")
endif()
foreach(d QtQuick/Dialogs QtQuick/LocalStorage QtQuick/Controls QtQml/XmlListModel QtQml/StateMachine)
    if(EXISTS "${OUT_DIR}/${d}")
        message(FATAL_ERROR "${d} must not be exposed to plugins")
    endif()
endforeach()
