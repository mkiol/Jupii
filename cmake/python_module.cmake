add_custom_command(
  OUTPUT python.tar.xz
  COMMAND sh -c "${tools_dir}/make_pymodules.sh ${CMAKE_BINARY_DIR}/external/pymodules ${PROJECT_BINARY_DIR}/python.tar.xz ${patches_dir}/yt_dlp.patch ${xz_path}"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  VERBATIM
)

add_library(python_module STATIC "${CMAKE_BINARY_DIR}/python.tar.xz")

list(APPEND deps python_module)
