cmake_minimum_required(VERSION 3.16)

project(behaviortree_py VERSION 4.7.1 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BEHAVIORTREE_PY_BUILD_TESTING "Build tests for BehaviorTree.PY" OFF)

option(BEHAVIORTREE_PY_STRICT_VERSION_CHECK "Fail configure if version strings drift across files" ON)

macro(_btpy_extract_version _label _content _regex _out_var)
  string(REGEX MATCH "${_regex}" _btpy_ver_match "${_content}")
  if(_btpy_ver_match)
    set(${_out_var} "${CMAKE_MATCH_1}")
  else()
    set(${_out_var} "")
  endif()
endmacro()

if(BEHAVIORTREE_PY_STRICT_VERSION_CHECK)
  set(_btpy_expected_version "${PROJECT_VERSION}")

  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml" _btpy_pyproject_toml)
  _btpy_extract_version(
    "pyproject.toml"
    "${_btpy_pyproject_toml}"
    "version[ \t]*=[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)\""
    _btpy_pyproject_version
  )

  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/behaviortree_py/_version.py" _btpy_version_py)
  _btpy_extract_version(
    "behaviortree_py/_version.py"
    "${_btpy_version_py}"
    "__version__[ \t]*=[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)\""
    _btpy_pkg_version
  )

  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/package.xml" _btpy_package_xml)
  _btpy_extract_version(
    "package.xml"
    "${_btpy_package_xml}"
    "<version>([0-9]+\\.[0-9]+\\.[0-9]+)</version>"
    _btpy_ros_version
  )

  if(NOT _btpy_pyproject_version OR NOT _btpy_pkg_version OR NOT _btpy_ros_version)
    message(WARNING
      "Could not parse all version strings for drift check. "
      "Parsed: pyproject.toml='${_btpy_pyproject_version}', "
      "behaviortree_py/_version.py='${_btpy_pkg_version}', "
      "package.xml='${_btpy_ros_version}'."
    )
  else()
    if(NOT _btpy_pyproject_version STREQUAL _btpy_expected_version)
      message(FATAL_ERROR
        "Version mismatch: project() VERSION='${_btpy_expected_version}' but pyproject.toml version='${_btpy_pyproject_version}'."
      )
    endif()
    if(NOT _btpy_pkg_version STREQUAL _btpy_expected_version)
      message(FATAL_ERROR
        "Version mismatch: project() VERSION='${_btpy_expected_version}' but behaviortree_py/_version.py __version__='${_btpy_pkg_version}'."
      )
    endif()
    if(NOT _btpy_ros_version STREQUAL _btpy_expected_version)
      message(FATAL_ERROR
        "Version mismatch: project() VERSION='${_btpy_expected_version}' but package.xml <version>='${_btpy_ros_version}'."
      )
    endif()
  endif()
endif()

find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(behaviortree_cpp CONFIG REQUIRED)

# Optional ROS/ament integration (thin wrapper; ROS-agnostic core)
find_package(ament_cmake QUIET)
if(ament_cmake_FOUND)
  find_package(ament_cmake_python REQUIRED)
endif()

pybind11_add_module(_core MODULE
  src/core/bindings.cpp
)

target_link_libraries(_core PRIVATE
  behaviortree_cpp::behaviortree_cpp
)

target_compile_definitions(_core PRIVATE
  BEHAVIORTREE_PY_VERSION="${PROJECT_VERSION}"
)

# Install location
set(BEHAVIORTREE_PY_PYTHON_INSTALL_DIR "" CACHE STRING
  "Python install dir relative to prefix (e.g. lib/python3/dist-packages). Overrides SKBUILD_PLATLIB_DIR if set."
)

set(_btpy_python_install_dir "")
if(BEHAVIORTREE_PY_PYTHON_INSTALL_DIR)
  set(_btpy_python_install_dir "${BEHAVIORTREE_PY_PYTHON_INSTALL_DIR}")
elseif(DEFINED PYTHON_INSTALL_DIR)
  # Provided by ament_cmake_python and paired with the generated PYTHONPATH hook.
  set(_btpy_python_install_dir "${PYTHON_INSTALL_DIR}")
elseif(DEFINED SKBUILD_PLATLIB_DIR)
  set(_btpy_python_install_dir "${SKBUILD_PLATLIB_DIR}")
else()
  # ROS on Debian/Ubuntu typically uses dist-packages.
  set(_btpy_python_install_dir "${CMAKE_INSTALL_LIBDIR}/python3/dist-packages")
endif()

install(DIRECTORY behaviortree_py
  DESTINATION "${_btpy_python_install_dir}"
)

install(TARGETS _core
  LIBRARY DESTINATION "${_btpy_python_install_dir}/behaviortree_py"
  RUNTIME DESTINATION "${_btpy_python_install_dir}/behaviortree_py"
  ARCHIVE DESTINATION "${_btpy_python_install_dir}/behaviortree_py"
)

if(ament_cmake_FOUND)
  ament_package()
endif()
