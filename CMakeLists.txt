cmake_minimum_required(VERSION 3.20)
project(PicModelViewerTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ===================== Qt6 =====================
set(Qt6_DIR "D:/qt/6.10.1/msvc2022_64/lib/cmake/Qt6")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Xml LinguistTools Svg)

# ===================== OSG =====================
set(OSG_DIR "E:/Program/osg/osg")
set(OSG_INCLUDE_DIR "${OSG_DIR}/include")
set(OSG_LIB_DIR "${OSG_DIR}/lib")
set(OSG_BIN_DIR "${OSG_DIR}/bin")

find_library(OSG_LIBRARY_DEBUG NAMES osgd PATHS ${OSG_LIB_DIR} NO_DEFAULT_PATH)
find_library(OSG_LIBRARY_RELEASE NAMES osg PATHS ${OSG_LIB_DIR} NO_DEFAULT_PATH)
find_library(OSGDB_LIBRARY_DEBUG NAMES osgDBd PATHS ${OSG_LIB_DIR} NO_DEFAULT_PATH)
find_library(OSGDB_LIBRARY_RELEASE NAMES osgDB PATHS ${OSG_LIB_DIR} NO_DEFAULT_PATH)
find_library(OSGGA_LIBRARY_DEBUG NAMES osgGAd PATHS ${OSG_LIB_DIR} NO_DEFAULT_PATH)
find_library(OSGGA_LIBRARY_RELEASE NAMES osgGA PATHS ${OSG_LIB_DIR} NO_DEFAULT_PATH)
find_library(OSGUTIL_LIBRARY_DEBUG NAMES osgUtild PATHS ${OSG_LIB_DIR} NO_DEFAULT_PATH)
find_library(OSGUTIL_LIBRARY_RELEASE NAMES osgUtil PATHS ${OSG_LIB_DIR} NO_DEFAULT_PATH)
find_library(OSGVIEWER_LIBRARY_DEBUG NAMES osgViewerd PATHS ${OSG_LIB_DIR} NO_DEFAULT_PATH)
find_library(OSGVIEWER_LIBRARY_RELEASE NAMES osgViewer PATHS ${OSG_LIB_DIR} NO_DEFAULT_PATH)
find_library(OPENTHREADS_LIBRARY_DEBUG NAMES OpenThreadsd PATHS ${OSG_LIB_DIR} NO_DEFAULT_PATH)
find_library(OPENTHREADS_LIBRARY_RELEASE NAMES OpenThreads PATHS ${OSG_LIB_DIR} NO_DEFAULT_PATH)

# Select debug/release based on generator type using generator expressions
set(OSG_LIBRARY "$<IF:$<CONFIG:Debug>,${OSG_LIBRARY_DEBUG},${OSG_LIBRARY_RELEASE}>")
set(OSGDB_LIBRARY "$<IF:$<CONFIG:Debug>,${OSGDB_LIBRARY_DEBUG},${OSGDB_LIBRARY_RELEASE}>")
set(OSGGA_LIBRARY "$<IF:$<CONFIG:Debug>,${OSGGA_LIBRARY_DEBUG},${OSGGA_LIBRARY_RELEASE}>")
set(OSGUTIL_LIBRARY "$<IF:$<CONFIG:Debug>,${OSGUTIL_LIBRARY_DEBUG},${OSGUTIL_LIBRARY_RELEASE}>")
set(OSGVIEWER_LIBRARY "$<IF:$<CONFIG:Debug>,${OSGVIEWER_LIBRARY_DEBUG},${OSGVIEWER_LIBRARY_RELEASE}>")
set(OPENTHREADS_LIBRARY "$<IF:$<CONFIG:Debug>,${OPENTHREADS_LIBRARY_DEBUG},${OPENTHREADS_LIBRARY_RELEASE}>")

# ===================== Assimp =====================
set(ASSIMP_DIR "E:/Program/assimp/assimp-6.0.4")
set(ASSIMP_INCLUDE_DIR "${ASSIMP_DIR}/include")
set(ASSIMP_LIB_DIR "${ASSIMP_DIR}/lib")
set(ASSIMP_BIN_DIR "${ASSIMP_DIR}/bin")

find_library(ASSIMP_LIBRARY_DEBUG NAMES assimp-vc143-mtd PATHS ${ASSIMP_LIB_DIR} NO_DEFAULT_PATH)
find_library(ASSIMP_LIBRARY_RELEASE NAMES assimp-vc143-mt PATHS ${ASSIMP_LIB_DIR} NO_DEFAULT_PATH)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ASSIMP_LIBRARY ${ASSIMP_LIBRARY_DEBUG})
else()
    set(ASSIMP_LIBRARY ${ASSIMP_LIBRARY_RELEASE})
endif()
# For multi-config generators, use generator expression
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set(ASSIMP_LIBRARY "$<IF:$<CONFIG:Debug>,${ASSIMP_LIBRARY_DEBUG},${ASSIMP_LIBRARY_RELEASE}>")
endif()

# ===================== Sources =====================
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/OSGWidget.cpp
    src/ModelLoader.cpp
    src/AssimpToOSGConverter.cpp
    src/ModelInfoDock.cpp
    src/SceneTreeDock.cpp
    src/I18nManager.cpp
    src/WelcomeWidget.cpp
    src/ModelConverter.cpp
    src/BatchConvertDialog.cpp
    src/NodeEditorDock.cpp
)

set(HEADERS
    src/MainWindow.h
    src/OSGWidget.h
    src/ModelLoader.h
    src/AssimpToOSGConverter.h
    src/ModelInfoDock.h
    src/SceneTreeDock.h
    src/I18nManager.h
    src/WelcomeWidget.h
    src/ModelConverter.h
    src/BatchConvertDialog.h
    src/NodeEditorDock.h
    src/EditCommand.h
)

# ===================== Translations =====================
set(TS_FILES
    translations/ModelViewer_zh_CN.ts
    translations/ModelViewer_en.ts
)

qt_create_translation(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR} ${TS_FILES})
qt_add_translation(QM_FILES ${TS_FILES})

# ===================== Resources =====================
set(RESOURCES
    resources/resources.qrc
)

# ===================== Executable =====================
qt_add_executable(PicModelViewerTool
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
    ${QM_FILES}
)

target_include_directories(PicModelViewerTool PRIVATE
    ${OSG_INCLUDE_DIR}
    ${ASSIMP_INCLUDE_DIR}
    src
)

target_link_libraries(PicModelViewerTool PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Xml
    Qt6::Svg
    ${OSG_LIBRARY}
    ${OSGDB_LIBRARY}
    ${OSGGA_LIBRARY}
    ${OSGUTIL_LIBRARY}
    ${OSGVIEWER_LIBRARY}
    ${OPENTHREADS_LIBRARY}
    ${ASSIMP_LIBRARY}
)

# ===================== Windows specific =====================
if(WIN32)
    target_compile_definitions(PicModelViewerTool PRIVATE
        _USE_MATH_DEFINES
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    # Copy DLLs to output directory after build
    set(OSG_PLUGIN_DIR "${OSG_BIN_DIR}/osgPlugins-3.6.5")

    # Function to copy DLLs
    function(copy_dlls_to_output target)
        # Copy OSG DLLs - they are directly in the bin directory
        file(GLOB OSG_DLLS "${OSG_BIN_DIR}/*.dll")
        foreach(DLL ${OSG_DLLS})
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" "$<TARGET_FILE_DIR:${target}>/"
                COMMENT "Copying ${DLL}"
            )
        endforeach()

        # Copy OSG plugins
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${OSG_PLUGIN_DIR}"
                "$<TARGET_FILE_DIR:${target}>/osgPlugins-3.6.5/"
            COMMENT "Copying OSG plugins..."
        )

        # Copy Assimp DLL
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ASSIMP_BIN_DIR}/assimp-vc143-mt.dll"
                "$<TARGET_FILE_DIR:${target}>/"
            COMMENT "Copying Assimp DLL"
        )
    endfunction()
    copy_dlls_to_output(PicModelViewerTool)
endif()

# Set OSG plugin path for runtime
set_target_properties(PicModelViewerTool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
