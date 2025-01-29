# Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
# SPDX-License-Identifier: GNU General Public License v3.0 or later
# CMake 3.22 ships with Ubuntu 22.04
# CMake 3.27 and QtCreator 12 recommended
cmake_minimum_required(VERSION 3.22)

# Remove _cmake suffix when the .pro is removed.  Added here for clarity
project(QtAgOpenGPS
    VERSION 0.0
    HOMEPAGE_URL https://github.com/torriem/QtAgOpenGPS
    LANGUAGES CXX
)

# Configure project options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)


# Find required libraries
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Gui)
find_package(Qt6 REQUIRED COMPONENTS OpenGL)
find_package(Qt6 REQUIRED COMPONENTS Quick)
find_package(Qt6 REQUIRED COMPONENTS QuickWidgets)
find_package(Qt6 REQUIRED COMPONENTS Network)
find_package(Qt6 REQUIRED COMPONENTS Multimedia)


# Qt magic
qt_standard_project_setup()

# Add Qt policies
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

###### Handling Local QML ######
# Determine QML mode (local or compiled)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LOCAL_QML ON) # Enable local QML loading in Debug mode for easier development
else()
    set(LOCAL_QML OFF) # Use compiled QML resources in Release mode for production
endif()

# Handle RCC (Resource Compiler) only in compiled mode
if(NOT LOCAL_QML)
    set(CMAKE_AUTORCC ON) # Enable automatic RCC only if resources are compiled
    message(STATUS "CMAKE_AUTORCC is ON: Using compiled resources")
else()
    set(CMAKE_AUTORCC OFF) # Disable RCC for local QML loading
    message(STATUS "CMAKE_AUTORCC is OFF: Using local QML files")
endif()

# Add Android-specific handling for QML module
if(ANDROID)
    set(LOCAL_QML OFF) # Always disable LOCAL_QML on Android for compatibility
else()
    option(LOCAL_QML "Load QML from disc instead of compiling them in.")
endif()

# Additional messaging for clarity
if(LOCAL_QML)
    add_compile_definitions(LOCAL_QML)
    message("Local QML is ON! Using QML files directly from the source directory")

    # Set the QML import path to the source directory to load files directly
    set(QML_IMPORT_PATH ${CMAKE_SOURCE_DIR}/qml CACHE STRING "" FORCE)
else()
    message("Local QML is OFF. Compiling resources for faster production or Android compatibility.")
    message("For development details, visit:")
    message("https://github.com/torriem/QtAgOpenGPS/wiki/Building-QtAgOpenGPS-without-compiling-resources-with-QMake")

    # Notify the user that compiled resources are being used
    set(QML_IMPORT_PATH ${CMAKE_BINARY_DIR} CACHE STRING "" FORCE)
endif()

# Install resources for deployment only in compiled mode
if(NOT LOCAL_QML)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/qml DESTINATION ${CMAKE_INSTALL_PREFIX}/qml)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/qml/images DESTINATION ${CMAKE_INSTALL_PREFIX}/images)
endif()

# Find all QML and image files recursively only for compiled mode
if(NOT LOCAL_QML)
    file(GLOB_RECURSE QML_FILES
        "${CMAKE_SOURCE_DIR}/qml/*.qml"
        "${CMAKE_SOURCE_DIR}/qml/*.js"
        "${CMAKE_SOURCE_DIR}/images/*.*"
    )

    # Generate the .qrc file dynamically
    set(QRC_CONTENT "<!DOCTYPE RCC><RCC version=\"1.0\">\n<qresource prefix=\"/\">\n")
    foreach(QML_FILE ${QML_FILES})
        # Vérifiez que le fichier existe, qu'il n'est pas un répertoire et qu'il n'est pas vide
        if(EXISTS ${QML_FILE} AND NOT IS_DIRECTORY ${QML_FILE} AND NOT QML_FILE STREQUAL "")
            file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}" ${QML_FILE})
            # Vérifiez que le chemin relatif est valide
            if(REL_PATH AND NOT REL_PATH STREQUAL "")
                set(QRC_CONTENT "${QRC_CONTENT}    <file>${REL_PATH}</file>\n")
            else()
                message(WARNING "Invalid relative path for file: ${QML_FILE}")
            endif()
        else()
            message(WARNING "Skipped invalid file or directory: ${QML_FILE}")
        endif()
    endforeach()
    set(QRC_CONTENT "${QRC_CONTENT}</qresource>\n</RCC>\n")

    # Write the .qrc file to the build directory
    set(QRC_FILE "${CMAKE_SOURCE_DIR}/agopengps.qrc")
    if(QRC_CONTENT MATCHES "<file>")
        file(WRITE "${QRC_FILE}" "${QRC_CONTENT}")
        message(STATUS "Generated resource file: ${QRC_FILE}")
    else()
        message(FATAL_ERROR "No valid files found to generate the .qrc file.")
    endif()
endif()

###### end script of Local QML handling ######



# Create the executable
qt_add_executable(QtAgOpenGPS
    main.cpp
    $<$<NOT:$<BOOL:${LOCAL_QML}>>:agopengps.qrc>
    aogrenderer.cpp
    aogrenderer.h
    aogrenderer.cpp aogrenderer.h # moved renderer in here
    aogproperties.cpp
    aogproperty.h aogproperty.cpp
    aogrenderer.h aogrenderer.cpp
    aogsettings.h aogsettings.cpp
    btnenum.h
    common.h
    formgps.h formgps.cpp
    formgps_classcallbacks.cpp
    formgps_opengl.cpp
    formgps_position.cpp
    formgps_saveopen.cpp
    formgps_sections.cpp
    formgps_settings.cpp
    formgps_sim.cpp
    formgps_udpcomm.cpp
    formgps_ui.cpp
    formgps_ui_boundary.cpp
    formgps_ui_field.cpp
    formgps_ui_tracks.cpp
    formgps_ui_vehicle.cpp
    formheadache.h formheadache.cpp
    formheadland.h formheadland.cpp
    glutils.h glutils.cpp
    interfaceproperty.h
    properties.h
    qmlsectionbuttons.h qmlsectionbuttons.cpp
    qmlsettings.h qmlsettings.cpp
    qmlsettings_addkeys.cpp
    qmlutil.h
    setter.h
    testlists.cpp
    testnmea.cpp
    classes/cabcurve.h classes/cabcurve.cpp
    classes/cabline.h classes/cabline.cpp
    classes/cahrs.h classes/cahrs.cpp
    classes/cboundary.h classes/cboundary.cpp
    classes/cboundarylist.h classes/cboundarylist.cpp
    classes/ccamera.h classes/ccamera.cpp
    classes/ccontour.h classes/ccontour.cpp
    classes/cdubins.h classes/cdubins.cpp
    classes/cfence.cpp
    classes/cfielddata.h classes/cfielddata.cpp
    classes/cflag.h classes/cflag.cpp
    classes/cguidance.h classes/cguidance.cpp
    classes/chead.cpp
    classes/cheadline.h classes/cheadline.cpp
    classes/cmodulecomm.h classes/cmodulecomm.cpp
    classes/cnmea.h classes/cnmea.cpp
    classes/cpatches.h classes/cpatches.cpp
    classes/cpgn.h classes/cpgn.cpp
    classes/crecordedpath.h classes/crecordedpath.cpp
    classes/csection.h classes/csection.cpp
    classes/csim.h classes/csim.cpp
    classes/ctool.h classes/ctool.cpp
    classes/ctrack.h classes/ctrack.cpp
    classes/ctram.h classes/ctram.cpp
    classes/cturn.cpp
    classes/cturnlines.cpp
    classes/cvehicle.h classes/cvehicle.cpp
    classes/cworldgrid.h classes/cworldgrid.cpp
    classes/cyouturn.h classes/cyouturn.cpp
    classes/glm.h
    classes/vec2.h classes/vec2.cpp
    classes/vec3.h classes/vec3.cpp
    classes/vec4.h classes/vec4.cpp
    classes/vecfix2fix.h
    classes/qmlblockage.h classes/qmlblockage.cpp
    LICENSE
    props.py
    TODO.md
    parse_properties.py
    README.md
    android/AndroidManifest.xml
    android/build.gradle
    android/gradle.properties
    android/gradle/wrapper/gradle-wrapper.jar
    android/gradle/wrapper/gradle-wrapper.properties
    android/gradlew
    android/gradlew.bat
    android/res/values/libs.xml
    android/res/xml/qtprovider_paths.xml
)

#Add the QML module
qt_add_qml_module(QtAgOpenGPS
     URI AOGRenderer
     VERSION 1.0
     QML_FILES qml/MainWindow.qml
     SOURCES
        aogrenderer.cpp
        aogrenderer.h
     DEPENDENCIES Qt6::QtQuick
 )


if(NOT ANDROID)
    set_target_properties(QtAgOpenGPS PROPERTIES
        QT_QML_MODULE_INSTALL_QMLTYPES TRUE
        QT_QML_MODULE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()


# Where to look for header files
target_include_directories(QtAgOpenGPS PRIVATE
    .
    classes
)


# Libraries to link
target_link_libraries(QtAgOpenGPS PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickWidgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::OpenGL
    Qt6::Qml # Add this line to include the Qt Quick module
)

# Compile defines
target_compile_definitions(QtAgOpenGPS PRIVATE
    QT_DEPRECATED_WARNINGS
)

set_target_properties(QtAgOpenGPS PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
)

add_subdirectory(QtAgIO)

