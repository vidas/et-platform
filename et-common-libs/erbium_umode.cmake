############################
# Erbium Minion User Mode
############################

set(ERBIUM_UMODE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/erbium-umode)

################################################
# List the public interfaces and headers to be
# exposed to Erbium Minion User Mode here
################################################

# Listing of header only public interfaces
set(ERBIUM_UMODE_HDRS
    # Erbium ISA headers
    include/erbium/isa/atomic.h
    include/erbium/isa/atomic-impl.h
    include/erbium/isa/barriers.h
    include/erbium/isa/cacheops.h
    include/erbium/isa/cacheops-umode.h
    include/erbium/isa/esr_defines.h
    include/erbium/isa/fcc.h
    include/erbium/isa/flb.h
    include/erbium/isa/hart.h
    include/erbium/isa/layout.h
    include/erbium/isa/sync.h
    include/erbium/isa/tensors.h
    include/erbium/isa/utils.h
)

# Listing of public headers that expose services provided by
# the Erbium UMODE Library
set(ERBIUM_UMODE_LIB_HDRS
)

############################
# Create erbium-umode library
############################
# INTERFACE library: this target ships headers, a linker script, and
# the ERBIUM_LINKER_SCRIPT property only. The boot/crt/layout glue
# sources live in the consumer (erbium-examples/runtime/erbium/) so
# kernels can override _boot/_start by simply defining their own.
add_library(erbium-umode INTERFACE)
add_library(et-common-libs::erbium-umode ALIAS erbium-umode)
target_include_directories(erbium-umode
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${ERBIUM_UMODE_INSTALL_PREFIX}/include>
)

# Publish the default linker script's installed path as a custom
# target property so downstream code doesn't need to know the install
# layout. Consumers read it with
#     get_target_property(LD et-common-libs::erbium-umode ERBIUM_LINKER_SCRIPT)
set_target_properties(erbium-umode PROPERTIES
    ERBIUM_LINKER_SCRIPT "${ERBIUM_UMODE_INSTALL_PREFIX}/share/erbium.ld"
)
set_property(TARGET erbium-umode APPEND PROPERTY
    EXPORT_PROPERTIES ERBIUM_LINKER_SCRIPT)

# This macro preserves the directory structure as defined by the
# ERBIUM UMODE listing above
macro(InstallErbiumHdrsWithDirStruct HEADER_LIST)
    foreach(HEADER ${${HEADER_LIST}})
        string(REGEX MATCH "(.*)[/\]" DIR ${HEADER})
        install(FILES ${HEADER} DESTINATION ${ERBIUM_UMODE_INSTALL_PREFIX}/${DIR})
    endforeach(HEADER)
endmacro(InstallErbiumHdrsWithDirStruct)

InstallErbiumHdrsWithDirStruct(ERBIUM_UMODE_HDRS)
InstallErbiumHdrsWithDirStruct(ERBIUM_UMODE_LIB_HDRS)

# Ship the default linker script. Downstream kernels can use this
# out of the box, or pass their own via the usual LINKER_SCRIPT path.
install(FILES share/erbium/erbium.ld
    DESTINATION ${ERBIUM_UMODE_INSTALL_PREFIX}/share)

####################################################
# Install and export erbium-umode library and headers
####################################################

install(
    TARGETS erbium-umode
    EXPORT erbium-umodeTargets
    INCLUDES DESTINATION ${ERBIUM_UMODE_INSTALL_PREFIX}/include
)

install(
    EXPORT erbium-umodeTargets
    NAMESPACE et-common-libs::
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/cmake/et-common-libs/erbium-umode
    COMPONENT erbium-umode
)
