#
# Copyright (c) 2021 Microchip Technology Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
add_subdirectory(drivers)

zephyr_get(MEC172X_SPI_GEN)
zephyr_get(MEC172X_SPI_CFG)
zephyr_get(BOARD_DIR SYSBUILD GLOBAL)

if(DEFINED ENV{MEC172X_SPI_GEN})
  # Grab it from environment variable if defined
  set(MEC172X_SPI_GEN $ENV{MEC172X_SPI_GEN})
else()
  # Else find the tool in PATH

  if(CMAKE_HOST_APPLE)
    # CMAKE_HOST_UNIX is also true for Apple,
    # but there is no tool for Apple. So
    # we need to skip it.
    message(NOTICE "There is no SPI image generation tool for MacOS.")
  elseif(CMAKE_HOST_UNIX)
    set(MEC172X_SPI_GEN_FILENAME mec172x_spi_gen_lin_x86_64)
  elseif(CMAKE_HOST_WIN32)
    set(MEC172X_SPI_GEN_FILENAME mec172x_spi_gen.exe)
  endif()

  find_file(MEC172X_SPI_GEN_FINDFILE ${MEC172X_SPI_GEN_FILENAME})
  if(MEC172X_SPI_GEN_FINDFILE STREQUAL MEC172X_SPI_GEN_FINDFILE-NOTFOUND)
    message(WARNING "Microchip SPI Image Generation tool (${MEC172X_SPI_GEN_FILENAME}) is not available. SPI Image will not be generated.")
  else()
    set(MEC172X_SPI_GEN ${MEC172X_SPI_GEN_FINDFILE})
  endif()

  unset(MEC172X_SPI_GEN_FINDFILE)
  unset(MEC172X_SPI_GEN_FILENAME)
endif()

if(DEFINED MEC172X_SPI_GEN)
  if(DEFINED MEC172X_SPI_CFG)
    set(ENV{MEC172X_SPI_CFG} "${MEC172X_SPI_CFG}")
  endif()
  if(DEFINED ENV{MEC172X_SPI_CFG})
    set(MEC172X_SPI_CFG_FILE $ENV{MEC172X_SPI_CFG})
  elseif(NOT DEFINED MEC172X_SPI_CFG)
    set(MEC172X_SPI_CFG_FILE ${BOARD_DIR}/support/spi_cfg.txt)
  endif()

  #  if(DEFINED CONFIG_BOOTLOADER_MCUBOOT)
  #  set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
  #    COMMAND dd if=${PROJECT_BINARY_DIR}/zephyr.bin of=${PROJECT_BINARY_DIR}/snip_zephyr.bin bs=1 skip=1024
  #    COMMAND cp ${PROJECT_BINARY_DIR}/snip_zephyr.bin ${PROJECT_BINARY_DIR}/zephyr.bin
  #  )
  #endif()

  set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
    COMMAND ${MEC172X_SPI_GEN}
    -i ${MEC172X_SPI_CFG_FILE}
    -o ${PROJECT_BINARY_DIR}/${SPI_IMAGE_NAME}
  )
  if(DEFINED CONFIG_MCUBOOT)
    set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
      COMMAND cp ${BOARD_DIR}/keys/* ${PROJECT_BINARY_DIR}/
      COMMAND dd if=${PROJECT_BINARY_DIR}/${SPI_IMAGE_NAME} of=${PROJECT_BINARY_DIR}/EcRegionPointer.bin bs=1 count=16
      COMMAND dd if=${PROJECT_BINARY_DIR}/${SPI_IMAGE_NAME} of=${PROJECT_BINARY_DIR}/EcRegion.bin bs=1 skip=4096
      COMMAND export FLASH_USED=`readelf -s ${PROJECT_BINARY_DIR}/${KERNEL_ELF_NAME} | grep _flash_used | awk -e '{print $$2 }' | sed -r 's/^/x/g' | awk -e '{print and(strtonum("0" $$1) +0xF, 0xFFFFFFF0) +768 +258080 }'`
      COMMAND echo $$FLASH_USED
      #COMMAND export FLASH_END=$$(expr 258048 + $$FLASH_USED)
      COMMAND dd if=${PROJECT_BINARY_DIR}/EcRegion.bin of=${PROJECT_BINARY_DIR}/zephyr.bin bs=1 count=$$FLASH_USED
      )
  endif()

  if(DEFINED CONFIG_BOOTLOADER_MCUBOOT)
    set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
      COMMAND export FLASH_USED=`readelf -s ${PROJECT_BINARY_DIR}/${KERNEL_ELF_NAME} | grep _flash_used | awk -e '{print $$2 }' | sed -r 's/^/x/g' | awk -e '{print and(strtonum("0" $$1) +0xF, 0xFFFFFFF0) +768 }'`
      COMMAND echo $$FLASH_USED
      COMMAND dd if=${PROJECT_BINARY_DIR}/${SPI_IMAGE_NAME} of=${PROJECT_BINARY_DIR}/EcRegion.bin bs=1 count=$$FLASH_USED
      #      COMMAND dd if=/dev/zero of=${PROJECT_BINARY_DIR}/zeros.bin bs=1 count=512
      #      COMMAND cat ${PROJECT_BINARY_DIR}/EcRegion.bin >> ${PROJECT_BINARY_DIR}/zeros.bin
      #COMMAND cp ${PROJECT_BINARY_DIR}/zeros.bin ${PROJECT_BINARY_DIR}/zephyr.bin
      COMMAND cp ${PROJECT_BINARY_DIR}/EcRegion.bin ${PROJECT_BINARY_DIR}/zephyr.bin
    )
  else()
    set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
      COMMAND dd if=${PROJECT_BINARY_DIR}/${SPI_IMAGE_NAME} of=${PROJECT_BINARY_DIR}/EcRegionPointer.bin bs=1 count=16
      COMMAND dd if=${PROJECT_BINARY_DIR}/${SPI_IMAGE_NAME} of=${PROJECT_BINARY_DIR}/EcRegion.bin bs=1 skip=4096
    )
  endif()

  unset(MEC172X_SPI_GEN)
  unset(MEC172X_SPI_CFG)
endif()

#set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
	#  COMMAND dd if=${PROJECT_BINARY_DIR}/${SPI_IMAGE_NAME} bs=1 of=${PROJECT_BINARY_DIR}/EcRegionPointer.bin count=16
	#  COMMAND dd if=${PROJECT_BINARY_DIR}/${SPI_IMAGE_NAME} bs=1 of=${PROJECT_BINARY_DIR}/EcRegion.bin skip=4096
	#  COMMAND cp ${PROJECT_BINARY_DIR}/EcRegion.bin ${PROJECT_BINARY_DIR}zephyr.bin
	#  COMMAND cp ${PROJECT_BINARY_DIR}/spi_image.bin ${PROJECT_BINARY_DIR}zephyr.bin
	#)
