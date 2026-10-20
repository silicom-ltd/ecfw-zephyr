# SPDX-License-Identifier: Apache-2.0

dt_comp_path(paths COMPATIBLE "ntc-thermistor")
foreach(path IN LISTS paths)
	dt_prop(rnom PATH ${path} PROPERTY "rnominal")
	dt_prop(tnom PATH ${path} PROPERTY "tnominal")
	dt_prop(bval PATH ${path} PROPERTY "b-const")
	dt_prop(pup PATH ${path} PROPERTY "rpullup")
	dt_prop(res PATH ${path} PROPERTY "resolution")
	dt_prop(tstart PATH ${path} PROPERTY "t-start")
	dt_prop(tstop PATH ${path} PROPERTY "t-stop")
	message(STATUS "found ntc-thermistor at ${path} rnominal: ${rnom} tnominal: ${tnom} b-const: ${bval} rpullup ${pup}
		resolution: ${res} t-start: ${tstart} t-stop ${tstop}")
endforeach()

add_custom_command(
	OUTPUT therm.h
	COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/generate_tbl.py --rnominal ${rnom} --tnominal ${tnom} --bconst ${bval} --rpullup ${pup} --res ${res} --tstart ${tstart} --tstop ${tstop}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
zephyr_library()

zephyr_library_sources(therm.h ntc_thermistor.c)
