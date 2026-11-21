# Keep the vendor program and its redistribution notices together.
set(bh_version 1.1.0)
set(bh_url "https://github.com/krisgfx9/Wireless-Device-Battery-Monitor/releases/download/v${bh_version}/BatteryHub-${bh_version}-portable.zip")
set(bh_zip "${CMAKE_BINARY_DIR}/BatteryHub-${bh_version}.zip")
set(bh_dir "${CMAKE_BINARY_DIR}/vendor/BatteryHub-${bh_version}")
file(DOWNLOAD "${bh_url}" "${bh_zip}" TLS_VERIFY ON
     EXPECTED_HASH SHA256=7a97a1ff0aefa895386077093b23a714df549a08fad3b1b9c37ccf2100a27026)
file(MAKE_DIRECTORY "${bh_dir}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${bh_zip}" WORKING_DIRECTORY "${bh_dir}" RESULT_VARIABLE unpack_result)
if(NOT unpack_result EQUAL 0)
  message(FATAL_ERROR "Could not unpack BatteryHub")
endif()
# Build the GPL companion from its exact release sources plus the TaskFolders changes.
find_package(Git REQUIRED)
find_program(BH_COMPILER NAMES g++.exe HINTS "${TASKFOLDERS_MINGW_BIN}" "C:/msys64/mingw64/bin")
if(NOT BH_COMPILER)
  message(FATAL_ERROR "Set TASKFOLDERS_MINGW_BIN to the MinGW-w64 bin directory (g++, windres).")
endif()
get_filename_component(bh_bin "${BH_COMPILER}" DIRECTORY)
set(bh_inputs "${CMAKE_SOURCE_DIR}/vendor/BatteryHub")
set(bh_archive "${bh_inputs}/BatteryHub-1.1.0-source.zip")
set(bh_patch "${bh_inputs}/taskfolders.patch")
set(bh_src "${CMAKE_BINARY_DIR}/batteryhub-src")
file(MAKE_DIRECTORY "${bh_src}")
add_custom_command(OUTPUT "${bh_src}/BatteryHub.exe"
  COMMAND "${CMAKE_COMMAND}" "-DARCHIVE=${bh_archive}" "-DPATCH=${bh_patch}" "-DBIN=${bh_bin}"
      "-DGIT=${GIT_EXECUTABLE}" -P "${CMAKE_CURRENT_LIST_DIR}/BuildMonitor.cmake"
  WORKING_DIRECTORY "${bh_src}" DEPENDS "${bh_archive}" "${bh_patch}" "${CMAKE_CURRENT_LIST_DIR}/BuildMonitor.cmake" VERBATIM)
add_custom_target(battery_monitor DEPENDS "${bh_src}/BatteryHub.exe")
add_dependencies(TaskFolders battery_monitor)
configure_file("${CMAKE_CURRENT_LIST_DIR}/BatteryHub-sources.txt" "${bh_dir}/SOURCES.txt" COPYONLY)
set(bh_run_dir "${CMAKE_BINARY_DIR}/$<CONFIG>")
set_target_properties(TaskFolders PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${bh_run_dir}")
add_custom_target(stage_battery
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${bh_run_dir}/BatteryHub"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${bh_src}/BatteryHub.exe" "${bh_dir}/headsetcontrol.exe"
      "${bh_dir}/LICENSE.txt" "${bh_dir}/THIRD_PARTY_NOTICES.txt" "${bh_dir}/SOURCES.txt"
      "${bh_run_dir}/BatteryHub" VERBATIM)
add_dependencies(stage_battery battery_monitor)
add_dependencies(TaskFolders stage_battery)
install(DIRECTORY "${bh_dir}/" DESTINATION BatteryHub PATTERN "BatteryHub.exe" EXCLUDE)
install(FILES "${bh_src}/BatteryHub.exe" DESTINATION BatteryHub)
install(FILES "${bh_archive}" "${bh_patch}" DESTINATION BatteryHub/source)
