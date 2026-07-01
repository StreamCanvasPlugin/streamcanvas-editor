cmake_minimum_required(VERSION 3.16)
project(stream-canvas-editor VERSION 0.1 LANGUAGES C CXX)

include(cmake/CPM.cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/engine engine)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        model/EditorTitle.h
        model/EditorTitle.cpp
        model/TitleDocument.h
        model/TitleDocument.cpp
        model/UndoCommands.h
        model/UndoCommands.cpp
        ui/adaptivestack.h ui/adaptivestack.cpp
        ui/titleproperties.h ui/titleproperties.cpp
        ui/transformeditor.h ui/transformeditor.cpp
        ui/animationeditor.h ui/animationeditor.cpp
        ui/widgets/ColorUtils.h     ui/widgets/ColorUtils.cpp
        ui/widgets/HueSlider.h      ui/widgets/HueSlider.cpp
        ui/widgets/GradientSlider.h ui/widgets/GradientSlider.cpp
        ui/widgets/ColorPreview.h   ui/widgets/ColorPreview.cpp
        ui/widgets/ColorLineEdit.h  ui/widgets/ColorLineEdit.cpp
        ui/widgets/GradientEditor.h ui/widgets/GradientEditor.cpp
        ui/widgets/GradientHandlePainter.h
        ui/widgets/LinearGradientEditor.h ui/widgets/LinearGradientEditor.cpp
        ui/widgets/RadialGradientEditor.h ui/widgets/RadialGradientEditor.cpp
        ui/widgets/ColorWheel.h     ui/widgets/ColorWheel.cpp
        ui/widgets/ColorPicker.h    ui/widgets/ColorPicker.cpp
        ui/cornerradiuseditor.h ui/cornerradiuseditor.cpp
        ui/paddingeditor.h ui/paddingeditor.cpp
        icons.h icons.cpp
        ui/painteditor.h ui/painteditor.cpp
        ui/widgets/CanvasWidget.h ui/widgets/CanvasWidget.cpp
        ui/widgets/TitleTreeModel.h ui/widgets/TitleTreeModel.cpp
        ui/widgets/TitleTreeView.h ui/widgets/TitleTreeView.cpp
        ui/widgets/SelectionHandles.h ui/widgets/SelectionHandles.cpp
        ui/widgets/AnimationTimingEditor.h ui/widgets/AnimationTimingEditor.cpp
        ui/widgets/GraphicTimingEditor.h ui/widgets/GraphicTimingEditor.cpp
        ui/widgets/ScrubRuler.h ui/widgets/ScrubRuler.cpp
        ui/widgets/CornerRadiusButton.h ui/widgets/CornerRadiusButton.cpp
        ui/widgets/PaddingButton.h      ui/widgets/PaddingButton.cpp
        ui/widgets/PaintPickerWidget.h  ui/widgets/PaintPickerWidget.cpp
        ui/widgets/BrandColorSwatchGrid.h ui/widgets/BrandColorSwatchGrid.cpp
        ui/widgets/RibbonFormatSection.h ui/widgets/RibbonFormatSection.cpp
        ui/UiUtils.h ui/UiUtils.cpp
        resources/resources.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(${PROJECT_NAME} SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(${PROJECT_NAME}
            ${PROJECT_SOURCES}
        )
    endif()
endif()

CPMAddPackage(
    NAME qlementine-icons
    GITHUB_REPOSITORY oclero/qlementine-icons
    GIT_TAG master
)

CPMAddPackage(
    NAME SARibbon
    GITHUB_REPOSITORY czyt1988/SARibbon
    GIT_TAG master
    OPTIONS
        "SARIBBON_BUILD_EXAMPLES OFF"
        "SARIBBON_BUILD_STATIC_LIBS ON"
)

CPMAddPackage(
  NAME nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip
  DOWNLOAD_ONLY YES
)

if (nlohmann_json_ADDED)
  add_library(nlohmann_json INTERFACE IMPORTED)
  target_include_directories(nlohmann_json INTERFACE ${nlohmann_json_SOURCE_DIR}/include)
endif()

# Resolve app version from the latest git tag; fall back to project version.
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE APP_GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT APP_GIT_TAG OR APP_GIT_TAG STREQUAL "")
    set(APP_GIT_TAG "${PROJECT_VERSION}")
endif()
message(STATUS "App version: ${APP_GIT_TAG}")

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(${PROJECT_NAME} PRIVATE APP_VERSION="${APP_GIT_TAG}")
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Svg
    engine
    qlementine-icons
    SARibbonBar
)

# ── Windows: embed ICO into the executable via a resource script ──────────────
if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE resources/app.rc)
endif()

# ── macOS: embed ICNS into the app bundle ─────────────────────────────────────
if(APPLE)
    set(MACOS_ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.icns)
    target_sources(${PROJECT_NAME} PRIVATE ${MACOS_ICON_FILE})
    set_source_files_properties(${MACOS_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER br.com.teknetsys.stream_canvas_editor)
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE_ICON_FILE app.icns
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ── Linux: install desktop entry and icon for AppImage / system packaging ─────
if(UNIX AND NOT APPLE)
    install(FILES packaging/stream-canvas-editor.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/icons/app.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME stream-canvas-editor.png)
    install(FILES resources/icons/app.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME stream-canvas-editor.svg)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()
