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

# ===================== GDAL (vcpkg) =====================
set(GDAL_ROOT "E:/Program/vcpkg/vcpkg-2025.10.17/installed/x64-windows")
set(GDAL_INCLUDE_DIR "${GDAL_ROOT}/include")
set(GDAL_LIB_DIR     "${GDAL_ROOT}/lib")
set(GDAL_BIN_DIR     "${GDAL_ROOT}/bin")
set(GDAL_DATA_DIR    "${GDAL_ROOT}/share/gdal")
set(PROJ_DATA_DIR    "${GDAL_ROOT}/share/proj")
find_library(GDAL_LIBRARY gdal HINTS "${GDAL_LIB_DIR}")

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
    src/PreProcessManager.cpp
    src/PreProcessDock.cpp
    src/GdalRasterLoader.cpp
    src/GdalVectorLoader.cpp
    src/ImageViewerWidget.cpp
    src/VectorViewerWidget.cpp
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
    src/PreProcessManager.h
    src/PreProcessDock.h
    src/GdalInit.h
    src/GdalRasterLoader.h
    src/GdalVectorLoader.h
    src/GdalVectorData.h
    src/ImageViewerWidget.h
    src/VectorViewerWidget.h
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
    ${GDAL_INCLUDE_DIR}
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
    ${GDAL_LIBRARY}
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

        # Copy GDAL DLL + transitive runtime dependencies (vcpkg x64-windows)
        set(GDAL_RUNTIME_DLLS
            gdal.dll
            proj_9.dll geos.dll geos_c.dll geotiff.dll tiff.dll
            sqlite3.dll libpng16.dll jpeg62.dll openjp2.dll
            netcdf.dll hdf5.dll hdf5_cpp.dll hdf5_hl.dll hdf5_hl_cpp.dll
            libcurl.dll libexpat.dll libxml2.dll
            libssl-3-x64.dll libcrypto-3-x64.dll
            zlib1.dll zstd.dll liblzma.dll libwebp.dll libsharpyuv.dll
            freexl-1.dll json-c.dll iconv-2.dll charset-1.dll
            Lerc.dll aec.dll szip.dll deflate.dll bz2.dll
            brotlicommon.dll brotlidec.dll brotlienc.dll
            spatialite.dll minizip.dll pcre2-8.dll
        )
        foreach(_dll ${GDAL_RUNTIME_DLLS})
            if(EXISTS "${GDAL_BIN_DIR}/${_dll}")
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${GDAL_BIN_DIR}/${_dll}"
                        "$<TARGET_FILE_DIR:${target}>/"
                    COMMENT "Copying ${_dll}"
                )
            endif()
        endforeach()

        # Copy GDAL/PROJ data directories so runtime can resolve drivers and CRS
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${GDAL_DATA_DIR}"
                "$<TARGET_FILE_DIR:${target}>/gdal-data/"
            COMMENT "Copying GDAL data..."
        )
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PROJ_DATA_DIR}"
                "$<TARGET_FILE_DIR:${target}>/proj-data/"
            COMMENT "Copying PROJ data..."
        )
    endfunction()
    copy_dlls_to_output(PicModelViewerTool)
endif()

# Set OSG plugin path for runtime
set_target_properties(PicModelViewerTool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
