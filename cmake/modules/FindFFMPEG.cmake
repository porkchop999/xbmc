# FindFFMPEG
# --------
# Finds FFmpeg libraries
#
# This module will first look for the required library versions on the system.
# If they are not found, it will fall back to downloading and building kodi's own version
#
# --------
# the following variables influence behaviour:
# ENABLE_INTERNAL_FFMPEG - if enabled, kodi's own version will always be built
#
# FFMPEG_PATH - use external ffmpeg not found in system paths
#               usage: -DFFMPEG_PATH=/path/to/ffmpeg_install_prefix
#
# WITH_FFMPEG - use external ffmpeg not found in system paths
#               WARNING: this option is for developers as it will _disable ffmpeg version checks_!
#               Consider using FFMPEG_PATH instead, which _does_ check library versions
#               usage: -DWITH_FFMPEG=/path/to/ffmpeg_install_prefix
#
# --------
# This module will define the following variables:
#
# FFMPEG_FOUND - system has FFmpeg
# FFMPEG_INCLUDE_DIRS - FFmpeg include directory
# FFMPEG_LIBRARIES - FFmpeg libraries
# FFMPEG_DEFINITIONS - pre-processor definitions
# FFMPEG_LDFLAGS - linker flags
#
# and the following imported targets::
#
# ffmpeg  - The FFmpeg libraries
# --------
#

# required ffmpeg library versions
set(REQUIRED_FFMPEG_VERSION 4.2)
set(_avcodec_ver ">=58.54.100")
set(_avfilter_ver ">=7.57.100")
set(_avformat_ver ">=58.29.100")
set(_avutil_ver ">=56.31.100")
set(_swscale_ver ">=5.5.100")
set(_swresample_ver ">=3.5.100")
set(_postproc_ver ">=55.5.100")


# Allows building with external ffmpeg not found in system paths,
# without library version checks
if(WITH_FFMPEG)
  set(FFMPEG_PATH ${WITH_FFMPEG})
  message(STATUS "Warning: FFmpeg version checking disabled")
  set(REQUIRED_FFMPEG_VERSION undef)
  unset(_avcodec_ver)
  unset(_avfilter_ver)
  unset(_avformat_ver)
  unset(_avutil_ver)
  unset(_swscale_ver)
  unset(_swresample_ver)
  unset(_postproc_ver)
endif()

# Allows building with external ffmpeg not found in system paths,
# with library version checks
if(FFMPEG_PATH)
  set(ENABLE_INTERNAL_FFMPEG OFF)
endif()

# external FFMPEG
if(NOT ENABLE_INTERNAL_FFMPEG OR KODI_DEPENDSBUILD)
  if(FFMPEG_PATH)
    list(APPEND CMAKE_PREFIX_PATH ${FFMPEG_PATH})
  endif()

  set(FFMPEG_PKGS libavcodec${_avcodec_ver}
                  libavfilter${_avfilter_ver}
                  libavformat${_avformat_ver}
                  libavutil${_avutil_ver}
                  libswscale${_swscale_ver}
                  libswresample${_swresample_ver}
                  libpostproc${_postproc_ver})

  if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_FFMPEG ${FFMPEG_PKGS} QUIET)
    string(REGEX REPLACE "framework;" "framework " PC_FFMPEG_LDFLAGS "${PC_FFMPEG_LDFLAGS}")
  endif()

  find_path(FFMPEG_INCLUDE_DIRS libavcodec/avcodec.h libavfilter/avfilter.h libavformat/avformat.h
                                libavutil/avutil.h libswscale/swscale.h libpostproc/postprocess.h
            PATH_SUFFIXES ffmpeg
            PATHS ${PC_FFMPEG_INCLUDE_DIRS}
            NO_DEFAULT_PATH)
  find_path(FFMPEG_INCLUDE_DIRS libavcodec/avcodec.h libavfilter/avfilter.h libavformat/avformat.h
                                libavutil/avutil.h libswscale/swscale.h libpostproc/postprocess.h)

  find_library(FFMPEG_LIBAVCODEC
               NAMES avcodec libavcodec
               PATH_SUFFIXES ffmpeg/libavcodec
               PATHS ${PC_FFMPEG_libavcodec_LIBDIR}
               NO_DEFAULT_PATH)
  find_library(FFMPEG_LIBAVCODEC NAMES avcodec libavcodec PATH_SUFFIXES ffmpeg/libavcodec)

  find_library(FFMPEG_LIBAVFILTER
               NAMES avfilter libavfilter
               PATH_SUFFIXES ffmpeg/libavfilter
               PATHS ${PC_FFMPEG_libavfilter_LIBDIR}
               NO_DEFAULT_PATH)
  find_library(FFMPEG_LIBAVFILTER NAMES avfilter libavfilter PATH_SUFFIXES ffmpeg/libavfilter)

  find_library(FFMPEG_LIBAVFORMAT
               NAMES avformat libavformat
               PATH_SUFFIXES ffmpeg/libavformat
               PATHS ${PC_FFMPEG_libavformat_LIBDIR}
               NO_DEFAULT_PATH)
  find_library(FFMPEG_LIBAVFORMAT NAMES avformat libavformat PATH_SUFFIXES ffmpeg/libavformat)

  find_library(FFMPEG_LIBAVUTIL
               NAMES avutil libavutil
               PATH_SUFFIXES ffmpeg/libavutil
               PATHS ${PC_FFMPEG_libavutil_LIBDIR}
               NO_DEFAULT_PATH)
  find_library(FFMPEG_LIBAVUTIL NAMES avutil libavutil PATH_SUFFIXES ffmpeg/libavutil)

  find_library(FFMPEG_LIBSWSCALE
               NAMES swscale libswscale
               PATH_SUFFIXES ffmpeg/libswscale
               PATHS ${PC_FFMPEG_libswscale_LIBDIR}
               NO_DEFAULT_PATH)
  find_library(FFMPEG_LIBSWSCALE NAMES swscale libswscale PATH_SUFFIXES ffmpeg/libswscale)

  find_library(FFMPEG_LIBSWRESAMPLE
               NAMES swresample libswresample
               PATH_SUFFIXES ffmpeg/libswresample
               PATHS ${PC_FFMPEG_libswresample_LIBDIR}
               NO_DEFAULT_PATH)
  find_library(FFMPEG_LIBSWRESAMPLE NAMES NAMES swresample libswresample PATH_SUFFIXES ffmpeg/libswresample)

  find_library(FFMPEG_LIBPOSTPROC
               NAMES postproc libpostproc
               PATH_SUFFIXES ffmpeg/libpostproc
               PATHS ${PC_FFMPEG_libpostproc_LIBDIR}
               NO_DEFAULT_PATH)
  find_library(FFMPEG_LIBPOSTPROC NAMES postproc libpostproc PATH_SUFFIXES ffmpeg/libpostproc)

  if((PC_FFMPEG_FOUND
      AND PC_FFMPEG_libavcodec_VERSION
      AND PC_FFMPEG_libavfilter_VERSION
      AND PC_FFMPEG_libavformat_VERSION
      AND PC_FFMPEG_libavutil_VERSION
      AND PC_FFMPEG_libswscale_VERSION
      AND PC_FFMPEG_libswresample_VERSION
      AND PC_FFMPEG_libpostproc_VERSION)
     OR WIN32)
    set(FFMPEG_VERSION ${REQUIRED_FFMPEG_VERSION})


    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(FFMPEG
                                      VERSION_VAR FFMPEG_VERSION
                                      REQUIRED_VARS FFMPEG_INCLUDE_DIRS
                                                    FFMPEG_LIBAVCODEC
                                                    FFMPEG_LIBAVFILTER
                                                    FFMPEG_LIBAVFORMAT
                                                    FFMPEG_LIBAVUTIL
                                                    FFMPEG_LIBSWSCALE
                                                    FFMPEG_LIBSWRESAMPLE
                                                    FFMPEG_LIBPOSTPROC
                                                    FFMPEG_VERSION
                                      FAIL_MESSAGE "FFmpeg ${REQUIRED_FFMPEG_VERSION} not found, please consider using -DENABLE_INTERNAL_FFMPEG=ON")

  else()
    message(STATUS "FFmpeg ${REQUIRED_FFMPEG_VERSION} not found, falling back to internal build")
    unset(FFMPEG_INCLUDE_DIRS)
    unset(FFMPEG_INCLUDE_DIRS CACHE)
    unset(FFMPEG_LIBRARIES)
    unset(FFMPEG_LIBRARIES CACHE)
    unset(FFMPEG_DEFINITIONS)
    unset(FFMPEG_DEFINITIONS CACHE)
  endif()

  if(FFMPEG_FOUND)
    set(FFMPEG_LDFLAGS ${PC_FFMPEG_LDFLAGS} CACHE STRING "ffmpeg linker flags")

    # check if ffmpeg libs are statically linked
    set(FFMPEG_LIB_TYPE SHARED)
    foreach(_fflib IN LISTS FFMPEG_LIBRARIES)
      if(${_fflib} MATCHES ".+\.a$" AND PC_FFMPEG_STATIC_LDFLAGS)
        set(FFMPEG_LDFLAGS ${PC_FFMPEG_STATIC_LDFLAGS} CACHE STRING "ffmpeg linker flags" FORCE)
        set(FFMPEG_LIB_TYPE STATIC)
        break()
      endif()
    endforeach()

    set(FFMPEG_LIBRARIES ${FFMPEG_LIBAVCODEC} ${FFMPEG_LIBAVFILTER}
                         ${FFMPEG_LIBAVFORMAT} ${FFMPEG_LIBAVUTIL}
                         ${FFMPEG_LIBSWSCALE} ${FFMPEG_LIBSWRESAMPLE}
                         ${FFMPEG_LIBPOSTPROC} ${FFMPEG_LDFLAGS})
    list(APPEND FFMPEG_DEFINITIONS -DFFMPEG_VER_SHA=\"${FFMPEG_VERSION}\")

    if(NOT TARGET ffmpeg)
      add_library(ffmpeg ${FFMPEG_LIB_TYPE} IMPORTED)
      set_target_properties(ffmpeg PROPERTIES
                                   FOLDER "External Projects"
                                   IMPORTED_LOCATION "${FFMPEG_LIBRARIES}"
                                   INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
                                   INTERFACE_LINK_LIBRARIES "${FFMPEG_LDFLAGS}"
                                   INTERFACE_COMPILE_DEFINITIONS "${FFMPEG_DEFINITIONS}")
    endif()
  endif()
endif()

# Internal FFMPEG
if(NOT FFMPEG_FOUND)

  set(WITH_FFMPEG_OPTIONS)

  # todo: cmake 3.8 has ternary genrator expressions
  # $<IF:DAV1D_FOUND,--enable-libdav1d,--disable-libdav1d>

  if(ENABLE_VAAPI)
    list(APPEND WITH_FFMPEG_OPTIONS --enable-vaapi)
  else()
    list(APPEND WITH_FFMPEG_OPTIONS --disable-vaapi)
  endif()

  if(ENABLE_VDPAU)
    list(APPEND WITH_FFMPEG_OPTIONS --enable-vdpau)
  else()
    list(APPEND WITH_FFMPEG_OPTIONS --disable-vdpau)
  endif()

  if(ENABLE_DAV1D)
    list(APPEND WITH_FFMPEG_OPTIONS --enable-libdav1d)
  else()
    list(APPEND WITH_FFMPEG_OPTIONS --disable-libdav1d)
  endif()

  include(${WITH_KODI_DEPENDS}/packages/ffmpeg/package.cmake)
  set(FFMPEG_VER ${PKG_VERSION})
  add_depends_for_targets("HOST")

  add_custom_target(ffmpeg ALL DEPENDS ffmpeg-host)

  if(NOT DAV1D_FOUND)
    message(STATUS "dav1d not found, internal ffmpeg build will be missing AV1 support!")
  endif()

  if(ENABLE_INTERNAL_DAV1D)
    add_dependencies(ffmpeg dav1d)
  endif()

  find_program(BASH_COMMAND bash)
  if(NOT BASH_COMMAND)
    message(FATAL_ERROR "Internal FFmpeg requires bash.")
  endif()
  file(WRITE ${CMAKE_BINARY_DIR}/${CORE_BUILD_DIR}/ffmpeg/ffmpeg-link-wrapper
"#!${BASH_COMMAND}
if [[ $@ == *${APP_NAME_LC}.bin* || $@ == *${APP_NAME_LC}${APP_BINARY_SUFFIX}* || $@ == *${APP_NAME_LC}.so* || $@ == *${APP_NAME_LC}-test* ]]
then
  avformat=`PKG_CONFIG_PATH=${INSTALL_PREFIX_HOST}/lib/pkgconfig ${PKG_CONFIG_EXECUTABLE} --libs --static libavcodec`
  avcodec=`PKG_CONFIG_PATH=${INSTALL_PREFIX_HOST}/lib/pkgconfig ${PKG_CONFIG_EXECUTABLE} --libs --static libavformat`
  avfilter=`PKG_CONFIG_PATH=${INSTALL_PREFIX_HOST}/lib/pkgconfig ${PKG_CONFIG_EXECUTABLE} --libs --static libavfilter`
  avutil=`PKG_CONFIG_PATH=${INSTALL_PREFIX_HOST}/lib/pkgconfig ${PKG_CONFIG_EXECUTABLE} --libs --static libavutil`
  swscale=`PKG_CONFIG_PATH=${INSTALL_PREFIX_HOST}/lib/pkgconfig ${PKG_CONFIG_EXECUTABLE} --libs --static libswscale`
  swresample=`PKG_CONFIG_PATH=${INSTALL_PREFIX_HOST}/lib/pkgconfig ${PKG_CONFIG_EXECUTABLE} --libs --static libswresample`
  gnutls=`PKG_CONFIG_PATH=${DEPENDS_PATH}/lib/pkgconfig/ ${PKG_CONFIG_EXECUTABLE}  --libs-only-l --static --silence-errors gnutls`
  $@ $avcodec $avformat $avcodec $avfilter $swscale $swresample -lpostproc $gnutls
else
  $@
fi")
  file(COPY ${CMAKE_BINARY_DIR}/${CORE_BUILD_DIR}/ffmpeg/ffmpeg-link-wrapper
       DESTINATION ${CMAKE_BINARY_DIR}/${CORE_BUILD_DIR}
       FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
  set(FFMPEG_LINK_EXECUTABLE "${CMAKE_BINARY_DIR}/${CORE_BUILD_DIR}/ffmpeg-link-wrapper <CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>" PARENT_SCOPE)
  set(FFMPEG_CREATE_SHARED_LIBRARY "${CMAKE_BINARY_DIR}/${CORE_BUILD_DIR}/ffmpeg-link-wrapper <CMAKE_CXX_COMPILER> <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> <SONAME_FLAG><TARGET_SONAME> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>" PARENT_SCOPE)

  set(FFMPEG_INCLUDE_DIRS ${INSTALL_PREFIX_HOST}/include)
  list(APPEND FFMPEG_DEFINITIONS -DFFMPEG_VER_SHA=\"${FFMPEG_VER}\"
                                 -DUSE_STATIC_FFMPEG=1)
  set(FFMPEG_FOUND 1)
  set_target_properties(ffmpeg PROPERTIES FOLDER "External Projects")
endif()

mark_as_advanced(FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARIES FFMPEG_LDFLAGS FFMPEG_DEFINITIONS FFMPEG_FOUND)
