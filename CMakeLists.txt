cmake_minimum_required(VERSION 3.15...3.30)
project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Development.SABIModule is what STABLE_ABI links against; it only exists when
# scikit-build-core builds an abi3 wheel (wheel.py-api in pyproject.toml).
find_package(Python 3.12 REQUIRED
  COMPONENTS Interpreter Development.Module
  OPTIONAL_COMPONENTS Development.SABIModule)
find_package(nanobind CONFIG REQUIRED)
find_package(Threads REQUIRED)

# SDK backend: the prebuilt unitree_sdk2 and its bundled CycloneDDS from
# UNITREE_SDK2_ROOT (-D flag, env var, or the third_party/unitree_sdk2
# submodule). -DUSE_STUB_SDK=ON builds against the header-only stub SDK
# instead: no hardware, no DDS, synthetic data.
option(USE_STUB_SDK "TEST ONLY: build against the bundled stub SDK (no hardware/DDS)" OFF)

if(USE_STUB_SDK)
  message(WARNING "unitree-sdk2-bind: building against the STUB SDK "
                  "(TEST ONLY: no hardware, synthetic data).")
  set(SDK_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/stub_sdk/include)
  set(SDK_LIBRARIES "")
  set(SDK_RPATH "")
  set(BIND_IS_STUB 1)
else()
  if(NOT UNITREE_SDK2_ROOT)
    if(DEFINED ENV{UNITREE_SDK2_ROOT})
      set(UNITREE_SDK2_ROOT $ENV{UNITREE_SDK2_ROOT})
    else()
      set(UNITREE_SDK2_ROOT ${CMAKE_SOURCE_DIR}/third_party/unitree_sdk2)
    endif()
  endif()

  # Map the host arch onto the SDK's prebuilt lib subdirs (x86_64 / aarch64).
  set(_arch ${CMAKE_SYSTEM_PROCESSOR})
  if(_arch STREQUAL "amd64")
    set(_arch "x86_64")
  elseif(_arch STREQUAL "arm64")
    set(_arch "aarch64")
  endif()

  set(_sdk_lib_dir ${UNITREE_SDK2_ROOT}/lib/${_arch})
  set(_dds_lib_dir ${UNITREE_SDK2_ROOT}/thirdparty/lib/${_arch})

  set(SDK_INCLUDE_DIRS
      ${UNITREE_SDK2_ROOT}/include
      ${UNITREE_SDK2_ROOT}/thirdparty/include
      ${UNITREE_SDK2_ROOT}/thirdparty/include/ddscxx)

  find_library(UNITREE_SDK2_LIB unitree_sdk2 PATHS ${_sdk_lib_dir} NO_DEFAULT_PATH)
  find_library(DDSC_LIB ddsc PATHS ${_dds_lib_dir} NO_DEFAULT_PATH)
  find_library(DDSCXX_LIB ddscxx PATHS ${_dds_lib_dir} NO_DEFAULT_PATH)

  if(NOT UNITREE_SDK2_LIB OR NOT DDSC_LIB OR NOT DDSCXX_LIB)
    message(FATAL_ERROR
        "Cannot find the unitree_sdk2 / CycloneDDS prebuilt libs under "
        "'${UNITREE_SDK2_ROOT}' for arch '${_arch}'. Point UNITREE_SDK2_ROOT at "
        "a unitree_sdk2 checkout, or pass -DUSE_STUB_SDK=ON for a hardware-free "
        "test build.")
  endif()

  message(STATUS "unitree-sdk2-bind: SDK root  = ${UNITREE_SDK2_ROOT} (${_arch})")
  message(STATUS "unitree-sdk2-bind: sdk lib   = ${UNITREE_SDK2_LIB}")
  message(STATUS "unitree-sdk2-bind: ddsc/ddscxx = ${DDSC_LIB} ; ${DDSCXX_LIB}")

  set(SDK_LIBRARIES ${UNITREE_SDK2_LIB} ${DDSC_LIB} ${DDSCXX_LIB} ${CMAKE_DL_LIBS})
  # auditwheel vendors the DDS shared objects next to the extension ($ORIGIN);
  # the SDK tree path keeps editable builds loadable without LD_LIBRARY_PATH.
  set(SDK_RPATH "$ORIGIN" ${_dds_lib_dir})
  set(BIND_IS_STUB 0)
endif()

# STABLE_ABI: abi3 wheel. NB_STATIC: self-contained extension.
nanobind_add_module(_core STABLE_ABI NB_STATIC src/bindings.cpp)

target_include_directories(_core PRIVATE ${SDK_INCLUDE_DIRS})
target_link_libraries(_core PRIVATE ${SDK_LIBRARIES} Threads::Threads)
target_compile_definitions(_core PRIVATE UNITREE_BIND_IS_STUB=${BIND_IS_STUB})

# The SDK's os.hpp uses SCHED_DEADLINE, which manylinux2014's kernel headers
# predate. Its Linux value is fixed; redefining it on newer glibc is identical.
if(NOT USE_STUB_SDK)
  target_compile_definitions(_core PRIVATE SCHED_DEADLINE=6)
endif()

if(SDK_RPATH)
  set_target_properties(_core PROPERTIES
      BUILD_WITH_INSTALL_RPATH TRUE
      INSTALL_RPATH "${SDK_RPATH}")
endif()

install(TARGETS _core LIBRARY DESTINATION unitree_sdk2_bind)

# Install the bundled DDS shared objects next to the extension so the $ORIGIN
# RUNPATH entry resolves them and the wheel is self-contained without
# auditwheel; a plain or git-dependency build would otherwise bake in an
# absolute path to the build tree. Install the resolved file under its SONAME
# (libddsc.so.0): that is the name the extension's NEEDED entries ask for, and
# in the SDK tree it is a symlink, which install(FILES) would copy as a
# dangling link.
if(NOT USE_STUB_SDK)
  file(GLOB _dds_soname_links
       "${_dds_lib_dir}/libddsc.so.*"
       "${_dds_lib_dir}/libddscxx.so.*")
  if(NOT _dds_soname_links)
    message(FATAL_ERROR
        "No versioned libddsc/libddscxx found under '${_dds_lib_dir}'. The wheel "
        "would not be self-contained; refusing to build a silently broken one.")
  endif()
  foreach(_dds_link IN LISTS _dds_soname_links)
    get_filename_component(_dds_soname "${_dds_link}" NAME)
    get_filename_component(_dds_real "${_dds_link}" REALPATH)
    install(FILES "${_dds_real}" DESTINATION unitree_sdk2_bind
            RENAME "${_dds_soname}")
  endforeach()
endif()
