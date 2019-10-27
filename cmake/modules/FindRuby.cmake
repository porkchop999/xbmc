# - Try to find ruby
# Once done this will define
#
# RUBY_FOUND - system has RUBY
# RUBY_INCLUDE_DIRS - the ruby include directory
# RUBY_LIBRARIES - The ruby libraries

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_RUBY ruby>=2.6 QUIET)
endif()

find_library(RUBY_LIBRARY NAMES ruby PATHS ${PC_RUBY_LIBDIR})
find_path(RUBY_INCLUDE_DIR NAMES ruby.h PATHS ${PC_RUBY_INCLUDE_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Ruby REQUIRED_VARS RUBY_INCLUDE_DIR RUBY_LIBRARY)
if(RUBY_FOUND)
  set(RUBY_INCLUDE_DIRS ${RUBY_INCLUDE_DIR})
  set(RUBY_LIBRARIES ${RUBY_LIBRARY})
  list(APPEND RUBY_DEFINITIONS -DHAS_RUBY=1)
endif()

mark_as_advanced(RUBY_INCLUDE_DIRS PYTHON_LIBRARIES)
