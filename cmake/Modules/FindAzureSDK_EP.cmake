#
# FindAzureSDK_EP.cmake
#
#
# The MIT License
#
# Copyright (c) 2018-2020 TileDB, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# This module finds the Azure C++ SDK, installing it with an ExternalProject if
# necessary. It then defines the imported target AzureSDK::AzureSDK.

# Include some common helper functions.
include(TileDBCommon)

# First check for a static version in the EP prefix.
find_library(AZURESDK_LIBRARIES
  NAMES
    libazure-storage-lite${CMAKE_STATIC_LIBRARY_SUFFIX}
	azure-storage-lite${CMAKE_STATIC_LIBRARY_SUFFIX}
  PATHS ${TILEDB_EP_INSTALL_PREFIX}
  PATH_SUFFIXES lib
  NO_DEFAULT_PATH
)

if (AZURESDK_LIBRARIES)
  set(AZURESDK_STATIC_EP_FOUND TRUE)
  find_path(AZURESDK_INCLUDE_DIR
    NAMES get_blob_request_base.h
    PATHS ${TILEDB_EP_INSTALL_PREFIX}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
  )
elseif(NOT TILEDB_FORCE_ALL_DEPS)
  set(AZURESDK_STATIC_EP_FOUND FALSE)
  # Static EP not found, search in system paths.
  find_library(AZURESDK_LIBRARIES
    NAMES
      libazure-storage-lite #*nix name
	  azure-storage-lite #windows name
    PATH_SUFFIXES lib bin
    ${TILEDB_DEPS_NO_DEFAULT_PATH}
  )
  find_path(AZURESDK_INCLUDE_DIR
    NAMES get_blob_request_base.h
    PATH_SUFFIXES include
    ${TILEDB_DEPS_NO_DEFAULT_PATH}
  )
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(AzureSDK
  REQUIRED_VARS AZURESDK_LIBRARIES AZURESDK_INCLUDE_DIR
)

if (NOT AZURESDK_FOUND)
  if (TILEDB_SUPERBUILD)

    set(DEPENDS)
    if (TARGET ep_curl)
      list(APPEND DEPENDS ep_curl)
    endif()
    if (TARGET ep_openssl)
      list(APPEND DEPENDS ep_openssl)
    endif()
    if (TARGET ep_zlib)
      list(APPEND DEPENDS ep_zlib)
    endif()

    if (WIN32)
#	  find_package(CURL 	REQUIRED)
      set(CFLAGS_DEF " /EHsc -I${TILEDB_EP_INSTALL_PREFIX}/include /Dazure_storage_lite_EXPORTS /DCURL_STATICLIB=1 /DWIN32 ${CMAKE_C_FLAGS}")
      set(CXXFLAGS_DEF " /EHsc -I${TILEDB_EP_INSTALL_PREFIX}/include /Dazure_storage_lite_EXPORTS /DCURL_STATICLIB=1 /DWIN32 ${CMAKE_CXX_FLAGS}")
#      set(CXXFLAGS_DEF "${CMAKE_CXX_FLAGS}" "/I${CURL_INCLUDE_DIR}")
	  set(CURL_INCLUDE_DIR "${TILEDB_EP_INSTALL_PREFIX}/include")
    else()
#      set(CFLAGS_DEF "${CMAKE_C_FLAGS} -fPIC")
#      set(CXXFLAGS_DEF "${CMAKE_CXX_FLAGS} -fPIC")
      #put our switch first in case other items are empty, leaving problematic blank space at beginning
      set(CFLAGS_DEF "-fPIC ${CMAKE_C_FLAGS}")
      set(CXXFLAGS_DEF "-fPIC ${CMAKE_CXX_FLAGS}")
    endif()

#    set(delimedgitcmd "") #TBD: necessary to extend outside following if()?
    if (WIN32)
        # needed for applying patches on windows
        find_package(Git REQUIRED)
		message("GIT_EXECUTABLE: ${GIT_EXECUTABLE}")
		#see comment on this answer - https://stackoverflow.com/a/45698220
		#and this - https://stackoverflow.com/a/62967602 (same thread, different answer/comment)
#"skipped patch" will occur if attempt to patch is not made to
#*from* repository root *and* we are attempting patch within subdirectory
#that is below root but *not* part of repository,
#*and*, if we did not specify the "--directory=<>"
#        set(PATCH ${GIT_EXECUTABLE} apply -p1 --verbose --directory=${TILEDB_EP_BASE}/src/ep_azuresdk)
        set(PATCH ${GIT_EXECUTABLE} apply -p1 --verbose)
#        set(PATCHB ${GIT_EXECUTABLE} apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk)
		set(strdelim "\"")
		#set(gitcmd ${strdelim} ${GIT_EXECUTABLE} ${strdelim})
        set(PATCHB "${GIT_EXECUTABLE} apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk")
		string(CONCAT delimedgitcmd ${strdelim} ${GIT_EXECUTABLE} ${strdelim})
		message("delimedgitcmd: ${delimedgitcmd}")
        set(PATCHB "${delimedgitcmd} apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk")
		message("PATCHB.A: ${PATCHB}")
		string(CONCAT PATCHB ${delimedgitcmd} " apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk")
		message("PATCHB.B: ${PATCHB}")
#<sigh>, so the following escapes the space(s) in a path and
#keeps cmake (elsewhere) from adding escaped quotes around the
#entire thing, but, is then, via PATCH_COMMAND apparently passed unfiltered to cmd.exe,
#which doesn't know what to do with the escaped version of the string, so no
#better off than the quotes...
#('cannot find path'), ...
		string(REPLACE " " "\\ " escapedgitcmd ${GIT_EXECUTABLE})
		message("escapedgitcmd: ${escapedgitcmd}")
		string(CONCAT PATCHB ${escapedgitcmd} " apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk")
#		string(CONCAT PATCHB ${escapedgitcmd} apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk)
#        set(PATCHB ${GIT_EXECUTABLE} apply -p1 --verbose --directory=../../../../externals/src/ep_azuresdk)
#	    string(CONCAT patchazurecmd "cd ${CMAKE_SOURCE_DIR} & " "${delimtedgitcmd}" " < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch")
#	    string(CONCAT patchazurecmd "cd ${CMAKE_SOURCE_DIR} & " ${delimtedgitcmd} " < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch")
#	    string(CONCAT patchazurecmd "cd ${CMAKE_SOURCE_DIR} & " "${PATCHB}" " < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch")
	    string(CONCAT patchazurecmd "cd ${CMAKE_SOURCE_DIR} & " ${PATCHB} " < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch")
		message("patchazurecmd: ${patchazurecmd}")
#	    string(CONCAT patchazurecmd "cd ${CMAKE_SOURCE_DIR} & " ${PATCHB} " < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch")
    else()
        find_package(Git REQUIRED)
		message("GIT_EXECUTABLE: ${GIT_EXECUTABLE}")
        set(PATCH patch -N -p1)
     	#set( patchazurecmd ${PATCH} "< ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch")
		string(CONCAT patchazurecmd ${PATCH} "< ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch")
		set(PATCHB PATCH)
    endif()
	
#	string(CONCAT patchcurlcmd "cd ${CMAKE_SOURCE_DIR} & " "${PATCHB}" " < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch")
    set(ORIGINCDIRPATCH ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk)
#	file(TO_NATIVE_PATH ${ORIGINCDIRPATCH} NATIVECURLINCDIRPATCH)
	message("ORIGINCDIRPATCH: ${ORIGINCDIRPATCH}")
#	message("NATIVECURLINCDIRPATCH: ${NATIVECURLINCDIRPATCH}")
    if(WIN32)
    ExternalProject_Add(ep_azuresdk
      PREFIX "externals"
      URL "https://github.com/Azure/azure-storage-cpplite/archive/v0.2.0.zip"
      URL_HASH SHA1=058975ccac9b60b522c9f7fd044a3d2aaec9f893
#	  UPDATE_COMMAND "cd"
#	  UPDATE_COMMAND "dir"
#	    UPDATE_COMMAND echo "cd ${CMAKE_SOURCE_DIR} & ${PATCHB} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch"
#	    UPDATE_COMMAND echo "cd ${CMAKE_SOURCE_DIR} & \"${PATCHB}\" < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch"
#	    UPDATE_COMMAND echo "cd ${CMAKE_SOURCE_DIR} & ${PATCHB} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch"
#	    UPDATE_COMMAND echo "${patchcurlcmd}"
#	    UPDATE_COMMAND echo ${patchcurlcmd}
#	    UPDATE_COMMAND echo diagupdatecommand && echo ${patchazurecmd} && echo ${delimedgitcmd} && echo ${PATCHB}
#		  && ${delimedgitcmd} < "nopatchfile"
#	    UPDATE_COMMAND echo "${patchazurecmd}"
#      UPDATE_COMMAND
#	    ${PATCH} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch
      CMAKE_ARGS
	    --verbose
	    -DVERBOSE=1
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTS=OFF
        -DBUILD_SAMPLES=OFF
        -DCMAKE_PREFIX_PATH=${TILEDB_EP_INSTALL_PREFIX}
        -DCMAKE_INSTALL_PREFIX=${TILEDB_EP_INSTALL_PREFIX}
#        -DCMAKE_CXX_FLAGS=-fPIC
#TBD: not windows vs linux diff's, fPIC 'ignored' by cl/vc, /D for win/cl/vc prob. doesn't hurt linux, but may want to consider avoiding...
#/DWIN32 definitely needs dealing with...
#        "-DCMAKE_CXX_FLAGS=-fPIC /EHsc -I${TILEDB_EP_INSTALL_PREFIX}/include /Dazure_storage_lite_EXPORTS /DWIN32"
        "-DCMAKE_CXX_FLAGS=-fPIC /EHsc -I${TILEDB_EP_INSTALL_PREFIX}/include /Dazure_storage_lite_EXPORTS /DCURL_STATICLIB=1 /DWIN32"
#        -DCURL_INCLUDE_DIR=${TILEDB_EP_BASE}/src/ep_curl/include
#        -DCURL_INCLUDE_DIR=${TILEDB_EP_BASE}/externals/src/ep_curl/include
        -DCURL_INCLUDE_DIR=${TILEDB_EP_INSTALL_PREFIX}/include
#        -DCMAKE_CXX_FLAGS=-fPIC -I${CURL_INCLUDE_DIR}
#        -DCMAKE_CXX_FLAGS=-fPIC -I../ep_curl/include
#/DWIN32 definitely needs dealing with (need to avoid for non-win32 linux etc. builds)...
#        "-DCMAKE_C_FLAGS=-fPIC -I${TILEDB_EP_INSTALL_PREFIX}/include /Dazure_storage_lite_EXPORTS /DWIN32"
        "-DCMAKE_C_FLAGS=-fPIC -I${TILEDB_EP_INSTALL_PREFIX}/include /Dazure_storage_lite_EXPORTS /DCURL_STATICLIB=1 /DWIN32"
	  #<sigh>, cmake, the buildtool that makes hard things easy, and easy things hard (impossible?).
      PATCH_COMMAND
	    #this won't work if build directory is outside of repository!!!
	    #cmd /c "cd ${CMAKE_SOURCE_DIR} & \"${PATCHB}\" < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch" &&
	    #cmd /c "cd ${CMAKE_SOURCE_DIR} & ${PATCHB} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch" &&
	    #cmd /c "${patchazurecmd}" &&
		echo starting to patch ep_azuresdk && #marker to hunt in .vcxproj
		echo changing to ${CMAKE_SOURCE_DIR} &&
		cd ${CMAKE_SOURCE_DIR} &&
		echo attempting to execute ${escapedgitcmd} &&
#following may work, looks ok... but turns out cmake/etc. does *not* refilter
#the correctly escaped path into the native world's syntax, so still fails...
#		${escapedgitcmd} apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch &&
#		"${GIT_EXECUTABLE}" apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch &&
		${GIT_EXECUTABLE} apply -p1 --verbose --unsafe-paths --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch &&
#		${GIT_EXECUTABLE} apply -p1 --check --unsafe-paths --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${NATIVECURLINCDIRPATCH}/curlincludedir.4gitwin.patch &&
#		${GIT_EXECUTABLE} apply -p1 --unsafe-paths --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${NATIVECURLINCDIRPATCH} &&
#        ${escapedgitcmd} apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/remove-uuid-dep.patch &&
#badcontext        ${GIT_EXECUTABLE} apply -p1 --unsafe-paths --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/remove-uuid-dep.patch &&
#        ${escapedgitcmd} apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/azurite-support.patch &&
        ${GIT_EXECUTABLE} apply -p1 --unsafe-paths --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/azurite-support.patch &&
##        ${PATCHB} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/remove-uuid-dep.patch &&
#        ${PATCHB} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/remove-uuid-dep.patch &&
#        ${PATCHB} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/azurite-support.patch &&
##        ${delimedgitcmd}  " < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch" &&
#        ${PATCHB}  " < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch" &&
        echo b4 patching base64 &&
        ${GIT_EXECUTABLE} apply -p1 --unsafe-paths --check --apply --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/azure-storage-lite-base64.patch &&
		echo b4 patching storage_url &&
        ${GIT_EXECUTABLE} apply -p1 --unsafe-paths --check --apply --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/azure-storage-lite-storage_url.patch &&
        echo done patching ep_azuresdk
      LOG_DOWNLOAD TRUE
      LOG_CONFIGURE TRUE
      LOG_BUILD TRUE
      LOG_INSTALL TRUE
      LOG_OUTPUT_ON_FAILURE ${TILEDB_LOG_OUTPUT_ON_FAILURE}
      DEPENDS ${DEPENDS}
    )
	else()
    ExternalProject_Add(ep_azuresdk
      PREFIX "externals"
      URL "https://github.com/Azure/azure-storage-cpplite/archive/v0.2.0.zip"
      URL_HASH SHA1=058975ccac9b60b522c9f7fd044a3d2aaec9f893
#	  UPDATE_COMMAND "cd"
#	  UPDATE_COMMAND "dir"
#	    UPDATE_COMMAND echo "cd ${CMAKE_SOURCE_DIR} & ${PATCHB} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch"
#	    UPDATE_COMMAND echo "cd ${CMAKE_SOURCE_DIR} & \"${PATCHB}\" < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch"
#	    UPDATE_COMMAND echo "cd ${CMAKE_SOURCE_DIR} & ${PATCHB} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch"
#	    UPDATE_COMMAND echo "${patchcurlcmd}"
#	    UPDATE_COMMAND echo ${patchcurlcmd}
#	    UPDATE_COMMAND echo diagupdatecommand && echo ${patchazurecmd} && echo ${delimedgitcmd} && echo ${PATCHB}
#		  && ${delimedgitcmd} < "nopatchfile"
#	    UPDATE_COMMAND echo "${patchazurecmd}"
#      UPDATE_COMMAND
#	    ${PATCH} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch
      CMAKE_ARGS
#	    --verbose
	    -DVERBOSE=1
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTS=OFF
        -DBUILD_SAMPLES=OFF
        -DCMAKE_PREFIX_PATH=${TILEDB_EP_INSTALL_PREFIX}
        -DCMAKE_INSTALL_PREFIX=${TILEDB_EP_INSTALL_PREFIX}
#        -DCMAKE_CXX_FLAGS=-fPIC
#TBD: not windows vs linux diff's, fPIC 'ignored' by cl/vc, /D for win/cl/vc prob. doesn't hurt linux, but may want to consider avoiding...
#/DWIN32 definitely needs dealing with...
#        "-DCMAKE_CXX_FLAGS=-fPIC /EHsc -I${TILEDB_EP_INSTALL_PREFIX}/include /Dazure_storage_lite_EXPORTS /DWIN32"
#        "-DCMAKE_CXX_FLAGS=-fPIC /EHsc -I${TILEDB_EP_INSTALL_PREFIX}/include /Dazure_storage_lite_EXPORTS /DCURL_STATICLIB=1 /DWIN32"
#        "-DCMAKE_CXX_FLAGS=-fPIC"
        -DCMAKE_CXX_FLAGS=${CXXFLAGS_DEF}
#        -DCURL_INCLUDE_DIR=${TILEDB_EP_BASE}/src/ep_curl/include
#        -DCURL_INCLUDE_DIR=${TILEDB_EP_BASE}/externals/src/ep_curl/include
        -DCURL_INCLUDE_DIR=${TILEDB_EP_INSTALL_PREFIX}/include
#        -DCMAKE_CXX_FLAGS=-fPIC -I${CURL_INCLUDE_DIR}
#        -DCMAKE_CXX_FLAGS=-fPIC -I../ep_curl/include
#/DWIN32 definitely needs dealing with (need to avoid for non-win32 linux etc. builds)...
#        "-DCMAKE_C_FLAGS=-fPIC -I${TILEDB_EP_INSTALL_PREFIX}/include /Dazure_storage_lite_EXPORTS /DWIN32"
#        "-DCMAKE_C_FLAGS=-fPIC -I${TILEDB_EP_INSTALL_PREFIX}/include /Dazure_storage_lite_EXPORTS /DCURL_STATICLIB=1 /DWIN32"
#        "-DCMAKE_C_FLAGS=-fPIC"
        -DCMAKE_C_FLAGS=${CFLAGS_DEF}
	  #<sigh>, cmake, the buildtool that makes hard things easy, and easy things hard (impossible?).
      PATCH_COMMAND
	    #this won't work if build directory is outside of repository!!!
	    #cmd /c "cd ${CMAKE_SOURCE_DIR} & \"${PATCHB}\" < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch" &&
	    #cmd /c "cd ${CMAKE_SOURCE_DIR} & ${PATCHB} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch" &&
	    #cmd /c "${patchazurecmd}" &&
		echo starting to patch ep_azuresdk && #marker to hunt in .vcxproj
		echo changing to ${CMAKE_SOURCE_DIR} &&
		cd ${CMAKE_SOURCE_DIR} &&
		echo attempting to execute ${escapedgitcmd} &&
#following may work, looks ok... but turns out cmake/etc. does *not* refilter
#the correctly escaped path into the native world's syntax, so still fails...
#		${escapedgitcmd} apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch &&
#		"${GIT_EXECUTABLE}" apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch &&
		${GIT_EXECUTABLE} apply -p1 --verbose --unsafe-paths --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch &&
#		${GIT_EXECUTABLE} apply -p1 --check --unsafe-paths --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${NATIVECURLINCDIRPATCH}/curlincludedir.4gitwin.patch &&
#		${GIT_EXECUTABLE} apply -p1 --unsafe-paths --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${NATIVECURLINCDIRPATCH} &&
#        ${escapedgitcmd} apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/remove-uuid-dep.patch &&
#badcontext        ${GIT_EXECUTABLE} apply -p1 --unsafe-paths --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/remove-uuid-dep.patch &&
        ${GIT_EXECUTABLE} apply -p1 --unsafe-paths --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/remove-uuid-dep.patch &&
#        ${escapedgitcmd} apply -p1 --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/azurite-support.patch &&
        ${GIT_EXECUTABLE} apply -p1 --unsafe-paths --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/azurite-support.patch &&
##        ${PATCHB} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/remove-uuid-dep.patch &&
#        ${PATCHB} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/remove-uuid-dep.patch &&
#        ${PATCHB} < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/azurite-support.patch &&
##        ${delimedgitcmd}  " < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch" &&
#        ${PATCHB}  " < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/curlincludedir.4gitwin.patch" &&
        echo b4 patching base64 &&
        ${GIT_EXECUTABLE} apply -p1 --unsafe-paths --check --apply --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/azure-storage-lite-base64.patch &&
		echo b4 patching storage_url &&
        ${GIT_EXECUTABLE} apply -p1 --unsafe-paths --check --apply --verbose --directory=${TILEDB_EP_SOURCE_DIR}/ep_azuresdk < ${TILEDB_CMAKE_INPUTS_DIR}/patches/ep_azuresdk/azure-storage-lite-storage_url.patch &&
        echo done patching ep_azuresdk
      LOG_DOWNLOAD TRUE
      LOG_CONFIGURE TRUE
      LOG_BUILD TRUE
      LOG_INSTALL TRUE
      LOG_OUTPUT_ON_FAILURE ${TILEDB_LOG_OUTPUT_ON_FAILURE}
      DEPENDS ${DEPENDS}
    )
	endif()

    list(APPEND TILEDB_EXTERNAL_PROJECTS ep_azuresdk)
    list(APPEND FORWARD_EP_CMAKE_ARGS
      -DTILEDB_AZURESDK_EP_BUILT=TRUE
    )
  else ()
    message(FATAL_ERROR "Could not find AZURESDK (required).")
  endif ()
endif ()

if (AZURESDK_FOUND AND NOT TARGET AzureSDK::AzureSDK)
  add_library(AzureSDK::AzureSDK UNKNOWN IMPORTED)
  set_target_properties(AzureSDK::AzureSDK PROPERTIES
    IMPORTED_LOCATION "${AZURESDK_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${AZURESDK_INCLUDE_DIR}"
  )
endif()

# If we built a static EP, install it if required.
if (AZURESDK_STATIC_EP_FOUND AND TILEDB_INSTALL_STATIC_DEPS)
  install_target_libs(AzureSDK::AzureSDK)
endif()
