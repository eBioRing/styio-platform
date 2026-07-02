add_library(styio_cloud_build_config INTERFACE)
target_include_directories(styio_cloud_build_config INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_compile_definitions(styio_cloud_build_config INTERFACE
  "PAFIO_PROJECT_ROOT=\"${CMAKE_SOURCE_DIR}\""
  "PAFIO_PROJECT_VERSION=\"${PROJECT_VERSION}-cloud-dev\""
  "STYIO_CLOUD_PROJECT_ROOT=\"${CMAKE_SOURCE_DIR}\""
)
target_link_libraries(styio_cloud_build_config INTERFACE tomlplusplus::tomlplusplus nlohmann_json::nlohmann_json)

function(styio_cloud_add_static_library target)
  add_library(${target} STATIC ${ARGN})
  target_link_libraries(${target} PUBLIC styio_cloud_build_config)
endfunction()

set(STYIO_CLOUD_FOUNDATION_SOURCES
    PafioCore/Paths.cpp
    PafioCore/Process.cpp
    PafioCore/Sha256.cpp
)

set(STYIO_CLOUD_MANIFEST_SOURCES
    PafioManifest/Lockfile.cpp
    PafioManifest/Manifest.cpp
)

set(STYIO_CLOUD_RESOLUTION_SOURCES
    PafioRegistryClient/Client.cpp
    PafioResolve/Resolver.cpp
    PafioSecurity/RegistrySecurity.cpp
)

set(STYIO_CLOUD_COMPILE_SOURCES
    PafioPlan/CompilePlan.cpp
    PafioToolchain/State.cpp
)

set(STYIO_CLOUD_CLOUD_SOURCES
    PafioCloud/Contract.cpp
    PafioCloud/Execution.cpp
    PafioCloud/Job.cpp
)

set(STYIO_CLOUD_SERVICE_SOURCES
    StyioCloudService/BeastServer.cpp
    StyioCloudService/Config.cpp
    StyioCloudService/Http.cpp
    StyioCloudService/Identity.cpp
    StyioCloudService/JobQueue.cpp
    StyioCloudService/ObjectStore.cpp
    StyioCloudService/PostgresStore.cpp
    StyioCloudService/Router.cpp
)

styio_cloud_add_static_library(styio_cloud_foundation ${STYIO_CLOUD_FOUNDATION_SOURCES})

styio_cloud_add_static_library(styio_cloud_manifest ${STYIO_CLOUD_MANIFEST_SOURCES})
target_link_libraries(styio_cloud_manifest PUBLIC styio_cloud_foundation)

styio_cloud_add_static_library(styio_cloud_resolution ${STYIO_CLOUD_RESOLUTION_SOURCES})
target_link_libraries(styio_cloud_resolution PUBLIC styio_cloud_foundation styio_cloud_manifest)

styio_cloud_add_static_library(styio_cloud_compile ${STYIO_CLOUD_COMPILE_SOURCES})
target_link_libraries(styio_cloud_compile PUBLIC styio_cloud_foundation styio_cloud_manifest styio_cloud_resolution)

styio_cloud_add_static_library(styio_cloud_cloud ${STYIO_CLOUD_CLOUD_SOURCES})
target_link_libraries(styio_cloud_cloud PUBLIC styio_cloud_compile)

styio_cloud_add_static_library(styio_cloud_service ${STYIO_CLOUD_SERVICE_SOURCES})
target_link_libraries(styio_cloud_service PUBLIC styio_cloud_cloud)

add_library(styio_cloud_kernel INTERFACE)
target_link_libraries(styio_cloud_kernel INTERFACE
  styio_cloud_foundation
  styio_cloud_manifest
  styio_cloud_resolution
  styio_cloud_compile
  styio_cloud_cloud
  styio_cloud_service
)

add_executable(styio-cloudd StyioCloudService/Daemon.cpp)
target_link_libraries(styio-cloudd PRIVATE styio_cloud_service)
