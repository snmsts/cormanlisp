# Builds CormanLisp.img from scratch (what makeimg.bat does).
#
#   cmake -DCLCONSOLE=<clconsole.exe> -DSOURCE_DIR=<top of tree> -DIMAGE=<output>
#         -P BuildImage.cmake

foreach(_var CLCONSOLE SOURCE_DIR IMAGE)
  if(NOT DEFINED ${_var})
    message(FATAL_ERROR "${_var} is not set")
  endif()
endforeach()

file(REMOVE "${IMAGE}")

# With no image to load the kernel starts bare and compile-sys.lisp builds
# the system up from the sources in Sys/. A Lisp error leaves clconsole
# waiting in its debug loop, hence the timeout.
file(TO_NATIVE_PATH "${SOURCE_DIR}/Sys/compile-sys.lisp" _script)
execute_process(
  COMMAND "${CLCONSOLE}" -execute "${_script}" -image ""
  WORKING_DIRECTORY "${SOURCE_DIR}"
  TIMEOUT 1800
  RESULT_VARIABLE _result)

if(NOT _result EQUAL 0)
  file(REMOVE "${IMAGE}")
  message(FATAL_ERROR "Building the image failed: ${_result}")
endif()
if(NOT EXISTS "${IMAGE}")
  message(FATAL_ERROR "clconsole finished but ${IMAGE} was not created")
endif()
file(SIZE "${IMAGE}" _size)
if(_size EQUAL 0)
  file(REMOVE "${IMAGE}")
  message(FATAL_ERROR "${IMAGE} is empty")
endif()
