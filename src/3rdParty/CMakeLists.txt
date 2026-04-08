add_subdirectory(FastSignals)

# Build SalomeMesh for all Platforms since heavily patched
if (BUILD_SMESH)
    add_subdirectory(salomesmesh)
endif()

add_subdirectory(lazy_loader)

if(NOT FREECAD_USE_EXTERNAL_E57FORMAT)
    add_subdirectory(libE57Format)
endif()

if(BUILD_MATERIAL_EXTERNAL)
    add_subdirectory(lru-cache)
endif()

if (BUILD_ASSEMBLY AND NOT FREECAD_USE_EXTERNAL_ONDSELSOLVER)
    if( NOT EXISTS "${CMAKE_SOURCE_DIR}/src/3rdParty/OndselSolver/CMakeLists.txt" )
        message(FATAL_ERROR "The OndselSolver git submodule is not available. Please run
        git submodule update --init" )
    endif()
    add_subdirectory(OndselSolver)
endif()

if (BUILD_TRACY_FRAME_PROFILER)
    if( NOT EXISTS "${CMAKE_SOURCE_DIR}/src/3rdParty/tracy/CMakeLists.txt" )
        message(FATAL_ERROR "The Tracy git directory is not available. Please clone it manually." )
    endif()

    set(TRACY_STATIC OFF)
    add_subdirectory(tracy)
endif()

# Clipper2 for polygon clipping operations
if( NOT EXISTS "${CMAKE_SOURCE_DIR}/src/3rdParty/Clipper2/CPP/CMakeLists.txt" )
    message(FATAL_ERROR "The Clipper2 git submodule is not available. Please run
    git submodule update --init" )
endif()

# Configure Clipper2 options
set(CLIPPER2_UTILS OFF CACHE BOOL "Disable Clipper2 utilities" FORCE)
set(CLIPPER2_EXAMPLES OFF CACHE BOOL "Disable Clipper2 examples" FORCE)
set(CLIPPER2_TESTS OFF CACHE BOOL "Disable Clipper2 tests" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Clipper2 as static library" FORCE)
set(CLIPPER2_USINGZ "ON" CACHE STRING "Build Clipper2Z with Z-coordinate support" FORCE)

add_subdirectory(Clipper2/CPP)
