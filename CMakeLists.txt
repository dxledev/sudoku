cmake_minimum_required(VERSION 3.21)
project(QuietSudoku VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets)

add_library(sudoku_core
    src/core/puzzle.cpp
    src/core/game.cpp
    src/core/storage.cpp
    src/core/theme.cpp
    src/core/theme_source.cpp
)
target_include_directories(sudoku_core PUBLIC src)
target_link_libraries(sudoku_core PUBLIC Qt6::Core Qt6::Gui)

add_library(sudoku_ui
    src/ui/board.cpp
    src/ui/theme_watcher.cpp
    src/ui/window.cpp
)
target_link_libraries(sudoku_ui PUBLIC sudoku_core Qt6::Widgets)

add_executable(sudoku src/main.cpp src/cli.cpp)
target_link_libraries(sudoku PRIVATE sudoku_ui)

foreach(target sudoku_core sudoku_ui sudoku)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE sudoku_core Qt6::Test)
    add_test(NAME core COMMAND core_tests)
    add_executable(ui_tests tests/ui_tests.cpp)
    target_link_libraries(ui_tests PRIVATE sudoku_ui Qt6::Test)
    add_test(NAME ui COMMAND ui_tests)
    set_tests_properties(ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME cli COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.sh $<TARGET_FILE:sudoku>)
    add_test(NAME install COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/install_test.sh
        $<TARGET_FILE_DIR:sudoku> ${CMAKE_CURRENT_SOURCE_DIR}/scripts/install-desktop.sh)
endif()

include(GNUInstallDirs)
install(TARGETS sudoku RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/io.github.quiet_sudoku.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/io.github.quiet_sudoku.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
