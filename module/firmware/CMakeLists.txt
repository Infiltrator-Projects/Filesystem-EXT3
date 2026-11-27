# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)
project(Ext3BootReader LANGUAGES C)
set(COMMON_INCLUDE_DIR "" CACHE PATH "Pinned Common V2 include root")
if(NOT EXISTS "${COMMON_INCLUDE_DIR}/infiltratr/filesystem_module_v2.h")
  message(FATAL_ERROR "Set COMMON_INCLUDE_DIR to pinned Common V2 headers")
endif()
set(READER "${CMAKE_CURRENT_SOURCE_DIR}/../../native/filesystems/ext3/reader")
add_library(ext3-boot MODULE
  ../../module/ext3_common.c
  "${READER}/core/ext2_core.c"
  "${READER}/core/ext2_engine.c"
  "${READER}/module/ext2_module.c"
  runtime.c)
set_target_properties(ext3-boot PROPERTIES PREFIX "" OUTPUT_NAME "EXT3" SUFFIX ".so" C_VISIBILITY_PRESET hidden)
target_compile_features(ext3-boot PRIVATE c_std_11)
target_include_directories(ext3-boot PRIVATE "${COMMON_INCLUDE_DIR}")
target_compile_definitions(ext3-boot PRIVATE
  IFS_EXT3_FIRMWARE_BUILD=1 IFS_EXT3_READ_ONLY_BUILD=1 IFS_EXT3_FAMILY_BUILD=1)
# Firmware uses AMD64 SysV without CET, SIMD state assumptions,
# packed relative relocations or non-4-KiB load alignment. Hosted builds
# retain their separate native optimization profile.
target_compile_options(ext3-boot PRIVATE
  -Wall -Wextra -Wpedantic -Werror -ffreestanding -fno-builtin
  -fno-tree-loop-distribute-patterns -fno-stack-protector -mno-red-zone -fcf-protection=none -mgeneral-regs-only
  -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0)
target_link_options(ext3-boot PRIVATE
  -nostdlib -Wl,-z,nopack-relative-relocs,-z,max-page-size=4096,-Bsymbolic,-z,defs,-z,noexecstack,-z,relro,-z,now)
