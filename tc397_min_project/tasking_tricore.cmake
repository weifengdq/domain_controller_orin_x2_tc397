set(CMAKE_SYSTEM_NAME Generic)

find_program(COMPILER_C cctc PATHS ENV TOOLCHAIN_ROOT PATH_SUFFIXES bin)
find_program(COMPILER_ASM astc PATHS ENV TOOLCHAIN_ROOT PATH_SUFFIXES bin)

set(CMAKE_C_COMPILER ${COMPILER_C})
set(CMAKE_CXX_COMPILER ${COMPILER_C})
set(CMAKE_ASM_COMPILER ${COMPILER_ASM})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

add_compile_options(
  --misrac-version=2012 
  -D__CPU__=tc39xb 
  --iso=99 
  --c++14 
  --language=+volatile 
  --exceptions --anachronisms 
  --fp-model=3 
  -O0 
  --tradeoff=4 
  --compact-max-size=200 -g 
  -Wc-w544 -Wc-w557 
  -Ctc39xb 
  -Y0 -N0 -Z0
)

add_link_options(
  -d${PROJECT_SOURCE_DIR}/Lcf_Tasking_Tricore_Tc.lsl 
  -Wl-Oc -Wl-OL -Wl-Ot -Wl-Ox -Wl-Oy 
  -Wl-mc -Wl-mf -Wl-mi -Wl-mk -Wl-ml -Wl-mm -Wl-md -Wl-mr -Wl-mu --no-warnings= -Wl--error-limit=42 
  --fp-model=3 -lrt --lsl-core=vtc --exceptions --strict --anachronisms --force-c++ -Ctc39xb
  --format=ihex
)