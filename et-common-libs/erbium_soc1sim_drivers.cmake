#####################################
# Erbium peripheral drivers (soc1sim)
#####################################
# Header-only INTERFACE target. Mirrors et-common-libs::erbium-drivers
# but ships soc1sim-backed implementations that expose the same public
# `<erbium/drivers/*.h>` include paths. Today's surface is the
# fake-UART driver — it presents the same uart_*() U-mode API as the
# real Shakti UART on native erbium, backed by ring buffers in device
# memory polled by the host launcher.
#
# Consumers (erbium-examples kernels built with USE_SOC1SIM=ON, future
# soc1sim-side bring-up code) link et-common-libs::erbium-soc1sim-drivers
# and get the right headers on the include path.
#
# As with erbium-soc1sim umode, the source layout puts headers under
# `include/erbium-soc1sim/drivers/` and stages them under an injected
# `erbium/drivers/` prefix at build and install time so consumers see
# the same `#include <erbium/drivers/uart.h>` path regardless of
# backend.

set(ERBIUM_SOC1SIM_DRIVERS_INSTALL_PREFIX
    ${CMAKE_INSTALL_PREFIX}/erbium-soc1sim-drivers)

set(ERBIUM_SOC1SIM_DRIVERS_HDRS
    include/erbium-soc1sim/drivers/uart.h
)

# Stage headers under <build>/erbium-soc1sim-drivers-staged-include/erbium/drivers/
# so BUILD_INTERFACE consumers see the same `<erbium/drivers/...>`
# path as INSTALL_INTERFACE consumers.
set(ERBIUM_SOC1SIM_DRIVERS_STAGED_INCLUDE
    ${CMAKE_CURRENT_BINARY_DIR}/erbium-soc1sim-drivers-staged-include)
file(REMOVE_RECURSE ${ERBIUM_SOC1SIM_DRIVERS_STAGED_INCLUDE})
file(MAKE_DIRECTORY ${ERBIUM_SOC1SIM_DRIVERS_STAGED_INCLUDE}/erbium)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/include/erbium-soc1sim/drivers
     DESTINATION ${ERBIUM_SOC1SIM_DRIVERS_STAGED_INCLUDE}/erbium/)

add_library(erbium-soc1sim-drivers INTERFACE)
add_library(et-common-libs::erbium-soc1sim-drivers ALIAS erbium-soc1sim-drivers)
target_include_directories(erbium-soc1sim-drivers
    INTERFACE
        $<BUILD_INTERFACE:${ERBIUM_SOC1SIM_DRIVERS_STAGED_INCLUDE}>
        $<INSTALL_INTERFACE:${ERBIUM_SOC1SIM_DRIVERS_INSTALL_PREFIX}/include>
)
# The driver headers pull in `<erbium/isa/atomic.h>` and
# `<erbium/isa/utils.h>` from the erbium-soc1sim umode package.
target_link_libraries(erbium-soc1sim-drivers INTERFACE erbium-soc1sim)

# Install headers under `<prefix>/include/erbium/drivers/...` to match
# the consumer's `#include <erbium/drivers/...>` path.
macro(InstallErbiumSoc1simDriversHdrsWithDirStruct HEADER_LIST)
    foreach(HEADER ${${HEADER_LIST}})
        string(REGEX REPLACE "^include/erbium-soc1sim/" "" REL_HEADER ${HEADER})
        get_filename_component(REL_DIR ${REL_HEADER} DIRECTORY)
        install(FILES ${HEADER}
            DESTINATION ${ERBIUM_SOC1SIM_DRIVERS_INSTALL_PREFIX}/include/erbium/${REL_DIR})
    endforeach(HEADER)
endmacro(InstallErbiumSoc1simDriversHdrsWithDirStruct)

InstallErbiumSoc1simDriversHdrsWithDirStruct(ERBIUM_SOC1SIM_DRIVERS_HDRS)

install(
    TARGETS erbium-soc1sim-drivers
    EXPORT erbium-soc1sim-driversTargets
    INCLUDES DESTINATION ${ERBIUM_SOC1SIM_DRIVERS_INSTALL_PREFIX}/include
)

install(
    EXPORT erbium-soc1sim-driversTargets
    NAMESPACE et-common-libs::
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/cmake/et-common-libs/erbium-soc1sim-drivers
    COMPONENT erbium-soc1sim-drivers
)
