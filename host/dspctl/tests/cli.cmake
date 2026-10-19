# dspctl end to end on the mock backend: run with -DDSPCTL=<binary> -DWORK=<dir>.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

function(expect name expected_exit pattern)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT code EQUAL expected_exit)
    message(FATAL_ERROR "${name}: exit ${code}, expected ${expected_exit}\n${out}${err}")
  endif()
  if(NOT "${out}${err}" MATCHES "${pattern}")
    message(FATAL_ERROR "${name}: output does not match '${pattern}':\n${out}${err}")
  endif()
  message(STATUS "ok   ${name}")
endfunction()

expect("list" 0 "c66x_0 +4d80800000.dsp +mock +running.*c71x_0 +64800000.dsp"
       ${DSPCTL} --backend mock list)
expect("echo" 0 "^hello dsp\n$" ${DSPCTL} --backend mock echo c66x_1 hello dsp)
expect("status" 0 "firmware +mock c71x_0" ${DSPCTL} --backend mock status c71x_0)
expect("trace" 0 "course runtime running" ${DSPCTL} --backend mock trace c66x_0)

string(REPEAT "0123456789abcdef" 64 data)
file(WRITE "${WORK}/in.bin" "${data}")
expect("run copy" 0 "copy: 1024 bytes in, 1024 bytes out" ${DSPCTL} --backend mock run c66x_0 copy
       "${WORK}/in.bin" "${WORK}/out.bin")
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${WORK}/in.bin" "${WORK}/out.bin" RESULT_VARIABLE same)
if(NOT same EQUAL 0)
  message(FATAL_ERROR "run copy: output differs from input")
endif()
expect("run gain" 0 "gain_q15: 1024 bytes in" ${DSPCTL} --backend mock run c71x_0 gain_q15
       "${WORK}/in.bin" "${WORK}/gain.bin" 16384)

# 512 samples through two taps of 0.5 give 511 outputs (the bit-exact values
# are checked in test_runtime)
expect("run fir" 0 "fir_q15: 1024 bytes in, 1022 bytes out" ${DSPCTL} --backend mock run c66x_0 fir_q15
       "${WORK}/in.bin" "${WORK}/fir.bin" 16384,16384)
expect("numeric algorithm id" 0 "0x1: 1024 bytes in, 1024 bytes out" ${DSPCTL} --backend mock run c66x_0 0x1
       "${WORK}/in.bin" "${WORK}/num.bin" "")
expect("run without data" 0 "copy: no data, [0-9]+ DSP cycles" ${DSPCTL} --backend mock run c66x_0 copy - -)
expect("unknown numeric algorithm" 1 "unknown algorithm" ${DSPCTL} --backend mock run c66x_0 0xfa000006
       "${WORK}/in.bin" "${WORK}/x.bin" 0a0b)
expect("odd hexadecimal parameters" 2 "even number of hexadecimal digits" ${DSPCTL} --backend mock run c66x_0 7
       "${WORK}/in.bin" "${WORK}/x.bin" abc)
expect("fir needs coefficients" 2 "comma-separated Q15 coefficients" ${DSPCTL} --backend mock run c66x_0 fir_q15
       "${WORK}/in.bin" "${WORK}/fir.bin")

expect("lost response" 1 "did not answer.*timeout" ${CMAKE_COMMAND} -E env SDOS_MOCK_FAULT=drop
       ${DSPCTL} --backend mock --timeout 50 run c66x_0 copy "${WORK}/in.bin" "${WORK}/x.bin")
execute_process(COMMAND ${DSPCTL} --backend mock --json list OUTPUT_VARIABLE listed RESULT_VARIABLE code)
string(JSON third GET "${listed}" dsps 2 device)
string(JSON state GET "${listed}" dsps 0 state)
if(NOT code EQUAL 0 OR NOT third STREQUAL "64800000.dsp" OR NOT state STREQUAL "running")
  message(FATAL_ERROR "json list: unexpected output ${listed}")
endif()
message(STATUS "ok   json list")
execute_process(COMMAND ${DSPCTL} --json --backend mock status c66x_1 OUTPUT_VARIABLE status RESULT_VARIABLE code)
string(JSON firmware GET "${status}" identity firmware)
string(JSON requests GET "${status}" stats requests)
string(JSON fwstate GET "${status}" stats state)
if(NOT code EQUAL 0 OR NOT firmware STREQUAL "mock c66x_1" OR NOT requests EQUAL 2 OR NOT fwstate STREQUAL "ok")
  message(FATAL_ERROR "json status: unexpected output ${status}")
endif()
message(STATUS "ok   json status")

# Health line, safe state, and supervision (docs/safety.md).
expect("health" 0 "c66x_0 ok, heartbeat [0-9]+" ${DSPCTL} --backend mock health c66x_0)
expect("health of a faulted dsp" 3 "faulted.*fault access at 0xa7100000 \\(info 0x10\\)"
       ${CMAKE_COMMAND} -E env SDOS_MOCK_FAULT=faulted ${DSPCTL} --backend mock health c66x_0)
expect("status of a faulted dsp" 0 "state +faulted, fault access"
       ${CMAKE_COMMAND} -E env SDOS_MOCK_FAULT=faulted ${DSPCTL} --backend mock status c66x_0)
expect("run in the safe state" 1 "firmware faulted" ${CMAKE_COMMAND} -E env SDOS_MOCK_FAULT=faulted
       ${DSPCTL} --backend mock run c66x_0 copy "${WORK}/in.bin" "${WORK}/x.bin")
expect("supervise restarts a faulted dsp" 0 "reports a fault.*restart 1 succeeded.*trace follows"
       ${CMAKE_COMMAND} -E env SDOS_MOCK_FAULT=faulted ${DSPCTL} --backend mock supervise c66x_0 --restart
       --interval 10 --stale 100 --count 3)
expect("supervise detects a stopped heartbeat" 3 "the heartbeat stopped"
       ${CMAKE_COMMAND} -E env SDOS_MOCK_FAULT=hang ${DSPCTL} --backend mock supervise c71x_0 --interval 10
       --stale 50)
expect("budget above the watchdog limit" 1 "at most 2000000 us" ${DSPCTL} --backend mock --budget-us 2000001
       run c66x_0 copy "${WORK}/in.bin" "${WORK}/x.bin")
execute_process(COMMAND ${DSPCTL} --json --backend mock health c71x_0 OUTPUT_VARIABLE health RESULT_VARIABLE code)
string(JSON hstate GET "${health}" state)
if(NOT code EQUAL 0 OR NOT hstate STREQUAL "ok")
  message(FATAL_ERROR "json health: unexpected output ${health}")
endif()
message(STATUS "ok   json health")

expect("usage" 2 "usage: dspctl" ${DSPCTL})
expect("unknown dsp" 2 "unknown DSP.*c66x_0 c66x_1 c71x_0" ${DSPCTL} --backend mock status remoteproc0)
expect("load refuses a running dsp" 1 "load needs c66x_0 offline" ${DSPCTL} --backend mock load c66x_0
       "${WORK}/in.bin")
