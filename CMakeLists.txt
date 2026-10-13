cmake_minimum_required(VERSION 3.21)

# The kernel is only ever built one way (see the "Standard" configuration
# below), so that is the only configuration offered.
set(CMAKE_CONFIGURATION_TYPES Standard CACHE STRING "" FORCE)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Standard CACHE STRING "" FORCE)
endif()

project(CormanLisp LANGUAGES C CXX RC)

# The kernel is 32-bit x86 code for Windows with inline assembly in the
# Microsoft syntax: the Microsoft compiler, or clang (as clang-cl with the
# Microsoft libraries, or as clang with those of mingw-w64).
if(NOT WIN32 OR NOT (MSVC OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
  message(FATAL_ERROR "Corman Lisp needs the Microsoft compiler or clang, targeting Windows.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR
    "Corman Lisp is a 32-bit x86 program. Configure for x86, e.g.\n"
    "  cmake -G \"Visual Studio 17 2022\" -A Win32 ...")
endif()

option(CORMANLISP_SYSTEM_ZLIB "Use an installed zlib instead of downloading the sources" OFF)
option(CORMANLISP_BUILD_IMAGE "Build CormanLisp.img as part of the default target" ON)
# The IDE needs the MFC component of the compiler, and so far it is only
# built with the Microsoft compiler.
find_package(MFC QUIET)
set(_ide_default OFF)
if(MFC_FOUND AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(_ide_default ON)
endif()
option(CORMANLISP_BUILD_IDE "Build the IDE, CormanLisp.exe (needs MFC)" ${_ide_default})

# The executables locate Sys/, Modules/ and the image relative to their own
# directory, so they are placed at the top of the source tree.
set(CORMANLISP_RUNTIME_DIR "${CMAKE_SOURCE_DIR}")

#
# Compiler and linker settings.
#
# The garbage collector depends on the shape of the code the C++ compiler
# generates (proper stack frames, no clever register use), so everything is
# compiled without optimization. Most of the kernel is replaced by compiled
# Lisp code while the image is built, so this costs little.
#
if(MSVC)
  set(CMAKE_C_FLAGS "")
  set(CMAKE_CXX_FLAGS "/EHsc")
  set(CMAKE_C_FLAGS_STANDARD "/Od /Oy- /Zi")
  set(CMAKE_CXX_FLAGS_STANDARD "/Od /Oy- /Zi")
  set(CMAKE_EXE_LINKER_FLAGS_STANDARD "/DEBUG /INCREMENTAL:NO")
  set(CMAKE_SHARED_LINKER_FLAGS_STANDARD "/DEBUG /INCREMENTAL:NO")
  set(CMAKE_MODULE_LINKER_FLAGS_STANDARD "/DEBUG /INCREMENTAL:NO")
  set(CMAKE_STATIC_LINKER_FLAGS_STANDARD "")
  set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)

  add_compile_options(/W3 /GS- /arch:IA32 /fp:precise)
  # Lisp code is generated into the heap and run from there: no DEP.
  add_link_options(/NXCOMPAT:NO /DYNAMICBASE)
  # Options of the front ends only.
  set(_frontend_options /Ob1 /GF /Gy)
  set(_o2 /O2 /wd4996)
else()
  set(CMAKE_C_FLAGS "")
  set(CMAKE_CXX_FLAGS "")
  set(CMAKE_C_FLAGS_STANDARD "-O0 -g -fno-omit-frame-pointer")
  set(CMAKE_CXX_FLAGS_STANDARD "-O0 -g -fno-omit-frame-pointer")
  set(CMAKE_EXE_LINKER_FLAGS_STANDARD "")
  set(CMAKE_SHARED_LINKER_FLAGS_STANDARD "")
  set(CMAKE_MODULE_LINKER_FLAGS_STANDARD "")
  set(CMAKE_STATIC_LINKER_FLAGS_STANDARD "")
  # CormanLispServer.rc is not ASCII.
  set(CMAKE_RC_FLAGS "-c 1252")

  # -fms-extensions for the inline assembly; no SSE, as with /arch:IA32.
  add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-fms-extensions;-march=i686;-mno-sse;-Wall>")
  # No DEP, see above, and no DLLs of the compiler's runtime to carry along.
  add_link_options(-static -Wl,--disable-nxcompat -Wl,--dynamicbase)
  set(_frontend_options "")
  set(_o2 -O2)
  # CormanLispServer.dll and dlltemplate.dll are looked for by these names.
  set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()
add_compile_definitions(WIN32 NDEBUG)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # Old habits of the sources which clang reports at every use.
  add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-Wno-writable-strings;-Wno-missing-braces;-Wno-microsoft-exception-spec;-Wno-nonportable-include-path>")
endif()

function(cormanlisp_runtime_target target)
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "$<1:${CORMANLISP_RUNTIME_DIR}>")
endfunction()

# Embeds <target dir>/<target>.exe.manifest. It carries its own trustInfo, so
# the linker is told not to generate one.
function(cormanlisp_embed_manifest target)
  set(_manifest "${CMAKE_SOURCE_DIR}/${target}/${target}.exe.manifest")
  if(MSVC)
    target_sources(${target} PRIVATE "${_manifest}")
    target_link_options(${target} PRIVATE /MANIFEST /MANIFESTUAC:NO)
  else()
    # No linker support there: the manifest goes in as a resource.
    set(_rc "${CMAKE_CURRENT_BINARY_DIR}/${target}-manifest.rc")
    file(WRITE "${_rc}" "1 24 \"${_manifest}\"\n")
    target_sources(${target} PRIVATE "${_rc}")
    set_property(SOURCE "${_rc}" APPEND PROPERTY OBJECT_DEPENDS "${_manifest}")
  endif()
endfunction()

#
# zlib
#
if(CORMANLISP_SYSTEM_ZLIB)
  find_package(ZLIB REQUIRED)
  add_library(cormanlisp_zlib INTERFACE)
  target_link_libraries(cormanlisp_zlib INTERFACE ZLIB::ZLIB)
else()
  include(FetchContent)
  # Only the sources are wanted; SOURCE_SUBDIR names a directory without a
  # CMakeLists.txt so that zlib's own build is not pulled in.
  FetchContent_Declare(zlibsrc
    URL https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz
    URL_HASH SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR doc)
  FetchContent_MakeAvailable(zlibsrc)
  set(_zlib "${zlibsrc_SOURCE_DIR}")
  add_library(cormanlisp_zlib STATIC
    "${_zlib}/adler32.c"
    "${_zlib}/compress.c"
    "${_zlib}/crc32.c"
    "${_zlib}/deflate.c"
    "${_zlib}/gzclose.c"
    "${_zlib}/gzlib.c"
    "${_zlib}/gzread.c"
    "${_zlib}/gzwrite.c"
    "${_zlib}/infback.c"
    "${_zlib}/inffast.c"
    "${_zlib}/inflate.c"
    "${_zlib}/inftrees.c"
    "${_zlib}/trees.c"
    "${_zlib}/uncompr.c"
    "${_zlib}/zutil.c")
  target_include_directories(cormanlisp_zlib PUBLIC "${_zlib}")
  target_compile_definitions(cormanlisp_zlib PRIVATE _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE)
  # zlib has no part in the garbage collector's assumptions.
  target_compile_options(cormanlisp_zlib PRIVATE ${_o2})
endif()

#
# The kernel: CormanLispServer.dll, and the same objects as a static library
# for the stand-alone application stubs.
#
set(_server "${CMAKE_SOURCE_DIR}/CormanLispServer")
add_library(cormanlisp_kernel OBJECT
  "${_server}/distorm/src/decoder.c"
  "${_server}/distorm/src/distorm.c"
  "${_server}/distorm/src/instructions.c"
  "${_server}/distorm/src/insts.c"
  "${_server}/distorm/src/mnemonics.c"
  "${_server}/distorm/src/operands.c"
  "${_server}/distorm/src/prefix.c"
  "${_server}/distorm/src/textdefs.c"
  "${_server}/distorm/src/wstring.c"
  "${_server}/src/CharBuf.cpp"
  "${_server}/src/CoConnectionPoint.cpp"
  "${_server}/src/CoCormanLisp.cpp"
  "${_server}/src/CoEnumConnectionPoints.cpp"
  "${_server}/src/CoEnumConnections.cpp"
  "${_server}/src/Compx86.cpp"
  "${_server}/src/CormanLispServer.cpp"
  "${_server}/src/ExceptionFrame.cpp"
  "${_server}/src/Gc.cpp"
  "${_server}/src/Host.cpp"
  "${_server}/src/Lisp.cpp"
  "${_server}/src/Lispfunc.cpp"
  "${_server}/src/LispMath.cpp"
  "${_server}/src/LispThreadQueue.cpp"
  "${_server}/src/MemoryReport.cpp"
  "${_server}/src/OsWindows.cpp"
  "${_server}/src/Read.cpp"
  "${_server}/src/Stdafx.cpp"
  "${_server}/src/ThreadClasses.cpp"
  "${_server}/src/Unassemble.cpp"
  "${_server}/src/UserInfo.cpp"
  "${_server}/src/Write.cpp")
target_include_directories(cormanlisp_kernel PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${_server}/include")
target_compile_definitions(cormanlisp_kernel PRIVATE _WINDOWS _WINDLL _CormanLispServer)
target_link_libraries(cormanlisp_kernel PUBLIC cormanlisp_zlib)

set(_kernel_libs cormanlisp_zlib ws2_32 odbc32 odbccp32 userenv ole32 oleaut32 uuid)

add_library(CormanLispServer SHARED
  $<TARGET_OBJECTS:cormanlisp_kernel>
  "${_server}/res/CormanLispServer.rc"
  "${_server}/CormanLispServer.def")
target_include_directories(CormanLispServer PRIVATE "${_server}/res")
target_link_libraries(CormanLispServer PRIVATE ${_kernel_libs})
if(MSVC)
  target_link_options(CormanLispServer PRIVATE /SAFESEH:NO /SUBSYSTEM:WINDOWS /MANIFEST:NO)
else()
  # The names in the .def file are those of stdcall functions.
  target_link_options(CormanLispServer PRIVATE -Wl,--enable-stdcall-fixup)
endif()
cormanlisp_runtime_target(CormanLispServer)

add_library(CormanLispStatic STATIC $<TARGET_OBJECTS:cormanlisp_kernel>)
target_link_libraries(CormanLispStatic INTERFACE ${_kernel_libs})
# The kernel registers exception handlers of its own, which the table of
# safe handlers would not know about.
if(MSVC)
  target_link_options(CormanLispStatic INTERFACE /SAFESEH:NO)
endif()

#
# Front ends which load CormanLispServer.dll.
#
add_executable(clconsole clconsole/clconsole.cpp clconsole/clconsole.rc)
target_include_directories(clconsole PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_compile_definitions(clconsole PRIVATE _CONSOLE _MBCS)
target_compile_options(clconsole PRIVATE ${_frontend_options})
target_link_libraries(clconsole PRIVATE odbc32 odbccp32 ole32 oleaut32 uuid)
cormanlisp_runtime_target(clconsole)
cormanlisp_embed_manifest(clconsole)
# clconsole loads the kernel at run time.
add_dependencies(clconsole CormanLispServer)

add_executable(clboot WIN32 clboot/clboot.cpp clboot/clboot.rc)
target_include_directories(clboot PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_compile_definitions(clboot PRIVATE _WINDOWS)
target_compile_options(clboot PRIVATE ${_frontend_options})
target_link_libraries(clboot PRIVATE odbc32 odbccp32 ole32 oleaut32 uuid)
cormanlisp_runtime_target(clboot)
cormanlisp_embed_manifest(clboot)

#
# Stubs used by SAVE-APPLICATION and COMPILE-DLL.
#
add_executable(clconsoleapp clconsoleapp/clconsoleapp.cpp clconsoleapp/clconsoleapp.rc)
target_include_directories(clconsoleapp PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_compile_definitions(clconsoleapp PRIVATE _CONSOLE _MBCS)
target_compile_options(clconsoleapp PRIVATE ${_frontend_options})
target_link_libraries(clconsoleapp PRIVATE CormanLispStatic)
cormanlisp_runtime_target(clconsoleapp)
cormanlisp_embed_manifest(clconsoleapp)

add_executable(clbootapp WIN32 clbootapp/clbootapp.cpp clbootapp/clbootapp.rc)
target_include_directories(clbootapp PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_compile_definitions(clbootapp PRIVATE _WINDOWS _MBCS)
target_compile_options(clbootapp PRIVATE ${_frontend_options})
target_link_libraries(clbootapp PRIVATE CormanLispStatic)
cormanlisp_runtime_target(clbootapp)
cormanlisp_embed_manifest(clbootapp)

add_library(dlltemplate SHARED dlltemplate/dlltemplate.cpp dlltemplate/dlltemplate.def)
target_compile_options(dlltemplate PRIVATE ${_frontend_options})
# DllMain is the entry point, so the CRT startup code which would pull in
# the rest of the C runtime is not linked.
if(MSVC)
  target_link_libraries(dlltemplate PRIVATE vcruntime ucrt)
  target_link_options(dlltemplate PRIVATE /ENTRY:DllMain /SUBSYSTEM:WINDOWS /MANIFEST:NO)
else()
  target_link_options(dlltemplate PRIVATE -nostartfiles -Wl,--entry,_DllMain@12)
endif()
cormanlisp_runtime_target(dlltemplate)

#
# The IDE: CormanLisp.exe, an MFC application.
#
if(CORMANLISP_BUILD_IDE)
  if(NOT MFC_FOUND)
    message(FATAL_ERROR
      "CORMANLISP_BUILD_IDE is on but MFC was not found. Install the\n"
      "\"C++ MFC for x86\" component of the build tools, or configure with\n"
      "  -DCORMANLISP_BUILD_IDE=OFF")
  endif()
  set(_ide "${CMAKE_SOURCE_DIR}/CormanLispIDE")
  add_executable(CormanLisp WIN32
    "${_ide}/src/browser.cpp"
    "${_ide}/src/CharBuf.cpp"
    "${_ide}/src/CoCormanLispClient.cpp"
    "${_ide}/src/CommonLispFuncs.cpp"
    "${_ide}/src/CormanLisp.cpp"
    "${_ide}/src/CSDialog.cpp"
    "${_ide}/src/Dialogs.cpp"
    "${_ide}/src/DocOptions.cpp"
    "${_ide}/src/LispObjDisplay.cpp"
    "${_ide}/src/oleaccproxy.cpp"
    "${_ide}/src/PageSetupDlg.cpp"
    "${_ide}/src/PreferencesDialog.cpp"
    "${_ide}/src/SMDIChildWnd.cpp"
    "${_ide}/src/SMDIFrameWnd.cpp"
    "${_ide}/src/Stdafx.cpp"
    "${_ide}/src/ThreadClasses.cpp"
    "${_ide}/src/webbrowser.cpp"
    "${_ide}/CormanLisp.rc"
    "${_ide}/Version.rc")
  # MFC in a shared DLL.
  set_target_properties(CormanLisp PROPERTIES MFC_FLAG 2)
  target_include_directories(CormanLisp PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${_ide}/include"
    "${_ide}")
  target_compile_definitions(CormanLisp PRIVATE _WINDOWS _AFXDLL)
  target_link_libraries(CormanLisp PRIVATE CormanLispServer ole32)
  # The manifest is a resource in CormanLisp.rc.
  target_link_options(CormanLisp PRIVATE /MANIFEST:NO /NODEFAULTLIB:oleacc.lib)
  cormanlisp_runtime_target(CormanLisp)
endif()

#
# CormanLisp.img, built by the freshly built kernel from the sources in Sys/.
#
file(GLOB _sys_sources CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/Sys/*.lisp"
  "${CMAKE_SOURCE_DIR}/Sys/scmindent/*.lisp")
set(_image "${CORMANLISP_RUNTIME_DIR}/CormanLisp.img")
add_custom_command(OUTPUT "${_image}"
  COMMAND "${CMAKE_COMMAND}"
          "-DCLCONSOLE=$<TARGET_FILE:clconsole>"
          "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
          "-DIMAGE=${_image}"
          -P "${CMAKE_SOURCE_DIR}/cmake/BuildImage.cmake"
  DEPENDS clconsole CormanLispServer ${_sys_sources}
          "${CMAKE_SOURCE_DIR}/cmake/BuildImage.cmake"
  COMMENT "Building CormanLisp.img"
  VERBATIM)
if(CORMANLISP_BUILD_IMAGE)
  add_custom_target(image ALL DEPENDS "${_image}")
else()
  add_custom_target(image DEPENDS "${_image}")
endif()

#
# Tests: cmake --build build --config Standard, then
#        ctest --test-dir build -C Standard
#
enable_testing()
add_test(NAME lisp-tests
  COMMAND clconsole -execute test/run-tests.lisp
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
# A Lisp error outside the tests leaves clconsole in its debug loop.
set_tests_properties(lisp-tests PROPERTIES TIMEOUT 300)
