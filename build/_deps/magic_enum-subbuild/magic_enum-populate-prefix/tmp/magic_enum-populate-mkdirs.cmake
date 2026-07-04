# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "D:/GitHub/NaviGator/build/_deps/magic_enum-src")
  file(MAKE_DIRECTORY "D:/GitHub/NaviGator/build/_deps/magic_enum-src")
endif()
file(MAKE_DIRECTORY
  "D:/GitHub/NaviGator/build/_deps/magic_enum-build"
  "D:/GitHub/NaviGator/build/_deps/magic_enum-subbuild/magic_enum-populate-prefix"
  "D:/GitHub/NaviGator/build/_deps/magic_enum-subbuild/magic_enum-populate-prefix/tmp"
  "D:/GitHub/NaviGator/build/_deps/magic_enum-subbuild/magic_enum-populate-prefix/src/magic_enum-populate-stamp"
  "D:/GitHub/NaviGator/build/_deps/magic_enum-subbuild/magic_enum-populate-prefix/src"
  "D:/GitHub/NaviGator/build/_deps/magic_enum-subbuild/magic_enum-populate-prefix/src/magic_enum-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/GitHub/NaviGator/build/_deps/magic_enum-subbuild/magic_enum-populate-prefix/src/magic_enum-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/GitHub/NaviGator/build/_deps/magic_enum-subbuild/magic_enum-populate-prefix/src/magic_enum-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
