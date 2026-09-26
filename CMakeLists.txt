cmake_minimum_required(VERSION 3.16)

project(nxusb-client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort LinguistTools)
find_package(SDL2 REQUIRED)

set(PROJECT_SOURCES
    src/buttons.h
    src/buttons.cpp
    src/main.cpp
    src/nxusb_main.cpp
    src/nxusb_main.h
    src/nxusb_main.ui
    src/nxserial.h
    src/nxserial.cpp
    src/joy_handler.h
    src/joy_handler.cpp
    src/nxleds_widget.h
    src/nxleds_widget.cpp
    src/gamepad_widget.h
    src/gamepad_widget.cpp
    src/vibro_widget.h
    src/vibro_widget.cpp

    # Resources
    res/nxusb.qrc
)

if(WIN32 AND NOT EMSCRIPTEN)
    list(APPEND PROJECT_SOURCES res/nxusb.rc)
endif()

qt_create_translation(QM_FILES
    lang/nxusb_client_ru_RU.ts
    ${PROJECT_SOURCES}
)

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(nxusb-client
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${QM_FILES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET nxusb-client APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(nxusb-client SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(nxusb-client
            ${PROJECT_SOURCES}
            ${QM_FILES}
        )
    endif()
endif()

if(CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
    target_compile_definitions(nxusb-client PRIVATE -DDEBUG_BUILD)
endif()

target_include_directories(nxusb-client PRIVATE src)
target_link_libraries(nxusb-client PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::SerialPort)

if(TARGET SDL2::SDL2)
    target_link_libraries(nxusb-client PRIVATE SDL2::SDL2)
    target_include_directories(nxusb-client PRIVATE ${SDL2_INCLUDE_DIRS})
else()
    string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)
    target_include_directories(nxusb-client PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(nxusb-client PRIVATE ${SDL2_LIBRARIES})
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.nxusb-client)
endif()

set_target_properties(nxusb-client PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS nxusb-client
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(nxusb-client)
endif()

