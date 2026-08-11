cmake_minimum_required(VERSION 3.20)
project(camera_image_quality_reference LANGUAGES CXX)

# The published reference layer builds from the C++20 standard library alone.
# No RAW decoder, compression library, color-management library, or private
# dataset configuration is part of this build.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_library(camera_iq_sfr STATIC
  code/src/roi.cpp
  code/src/sfr.cpp)
target_include_directories(camera_iq_sfr PUBLIC code/include)

add_library(camera_iq_colorimetry STATIC
  code/src/colorimetry.cpp)
target_include_directories(camera_iq_colorimetry PUBLIC code/include)

add_library(camera_iq_gamut_mapping STATIC
  code/src/gamut_mapping.cpp)
target_include_directories(camera_iq_gamut_mapping PUBLIC code/include)
target_link_libraries(camera_iq_gamut_mapping PUBLIC camera_iq_colorimetry)

add_library(camera_iq_flat_field STATIC
  code/src/roi.cpp
  code/src/flat_field_gate.cpp
  code/src/shading.cpp)
target_include_directories(camera_iq_flat_field PUBLIC code/include)

add_library(camera_iq_cam16_equation_audit STATIC
  code/src/cam16_equation_audit.cpp)
target_include_directories(camera_iq_cam16_equation_audit PUBLIC code/include)

# Spectroradiometer path. Kept as separate targets so the dependency graph is
# visible: the analysis needs the sampled-spectrum layer and the typed
# measurement; the closure needs only the measurement.
add_library(camera_iq_sampled_spectrum STATIC code/src/sampled_spectrum.cpp)
target_include_directories(camera_iq_sampled_spectrum PUBLIC code/include)

add_library(camera_iq_spectral_compare STATIC code/src/spectral_compare.cpp)
target_include_directories(camera_iq_spectral_compare PUBLIC code/include)
target_link_libraries(camera_iq_spectral_compare PUBLIC camera_iq_sampled_spectrum)

add_library(camera_iq_spectro_measurement STATIC code/src/spectro_measurement.cpp)
target_include_directories(camera_iq_spectro_measurement PUBLIC code/include)

add_library(camera_iq_spectro_analysis STATIC code/src/spectro_analysis.cpp)
target_link_libraries(camera_iq_spectro_analysis
  PUBLIC camera_iq_sampled_spectrum camera_iq_spectro_measurement)

add_library(camera_iq_spectro_colorimetry STATIC code/src/spectro_colorimetry.cpp)
target_link_libraries(camera_iq_spectro_colorimetry PUBLIC camera_iq_spectro_measurement)

# Camera spectral-fidelity path. Physical closure and the normalized
# Luther-condition metric are independent numerical modules. The ISO
# 17321-style SMI layer reuses the published colorimetry implementation.
add_library(camera_iq_spectral_closure STATIC code/src/spectral_closure.cpp)
target_include_directories(camera_iq_spectral_closure PUBLIC code/include)

add_library(camera_iq_spectral_quality STATIC code/src/spectral_quality.cpp)
target_include_directories(camera_iq_spectral_quality PUBLIC code/include)

add_library(camera_iq_spectral_smi STATIC code/src/spectral_smi.cpp)
target_include_directories(camera_iq_spectral_smi PUBLIC code/include)
target_link_libraries(camera_iq_spectral_smi PUBLIC camera_iq_colorimetry)

enable_testing()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_executable(test_sfr code/tests/test_sfr.cpp)
target_link_libraries(test_sfr PRIVATE camera_iq_sfr)
add_test(NAME test_sfr COMMAND test_sfr)

add_executable(test_colorimetry code/tests/test_colorimetry.cpp)
target_link_libraries(test_colorimetry PRIVATE camera_iq_colorimetry)
add_test(NAME test_colorimetry COMMAND test_colorimetry)

add_executable(test_gamut_mapping code/tests/test_gamut_mapping.cpp)
target_link_libraries(test_gamut_mapping PRIVATE camera_iq_gamut_mapping)
add_test(NAME test_gamut_mapping COMMAND test_gamut_mapping)

add_executable(test_flat_field code/tests/test_flat_field.cpp)
target_link_libraries(test_flat_field PRIVATE camera_iq_flat_field)
add_test(NAME test_flat_field COMMAND test_flat_field)

add_executable(test_cam16_equation_audit
  code/tests/test_cam16_equation_audit.cpp)
target_link_libraries(test_cam16_equation_audit
  PRIVATE camera_iq_cam16_equation_audit)
add_test(NAME test_cam16_equation_audit COMMAND test_cam16_equation_audit)

add_executable(test_sampled_spectrum code/tests/test_sampled_spectrum.cpp)
target_link_libraries(test_sampled_spectrum PRIVATE camera_iq_sampled_spectrum)
add_test(NAME test_sampled_spectrum COMMAND test_sampled_spectrum)

add_executable(test_spectral_compare code/tests/test_spectral_compare.cpp)
target_link_libraries(test_spectral_compare PRIVATE camera_iq_spectral_compare)
add_test(NAME test_spectral_compare COMMAND test_spectral_compare)

add_executable(test_spectro_measurement code/tests/test_spectro_measurement.cpp)
target_link_libraries(test_spectro_measurement PRIVATE camera_iq_spectro_measurement)
add_test(NAME test_spectro_measurement COMMAND test_spectro_measurement)

add_executable(test_spectro_analysis code/tests/test_spectro_analysis.cpp)
target_link_libraries(test_spectro_analysis PRIVATE camera_iq_spectro_analysis)
add_test(NAME test_spectro_analysis COMMAND test_spectro_analysis)

add_executable(test_spectro_colorimetry code/tests/test_spectro_colorimetry.cpp)
target_link_libraries(test_spectro_colorimetry PRIVATE camera_iq_spectro_colorimetry)
add_test(NAME test_spectro_colorimetry COMMAND test_spectro_colorimetry)

add_executable(test_spectral_closure code/tests/test_spectral_closure.cpp)
target_link_libraries(test_spectral_closure PRIVATE camera_iq_spectral_closure)
add_test(NAME test_spectral_closure COMMAND test_spectral_closure)

add_executable(test_spectral_quality code/tests/test_spectral_quality.cpp)
target_link_libraries(test_spectral_quality PRIVATE camera_iq_spectral_quality)
add_test(NAME test_spectral_quality COMMAND test_spectral_quality)

add_executable(test_spectral_smi code/tests/test_spectral_smi.cpp)
target_link_libraries(test_spectral_smi PRIVATE camera_iq_spectral_smi)
add_test(NAME test_spectral_smi COMMAND test_spectral_smi)

add_test(
  NAME test_repository_check
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/test_check_repository.py)
