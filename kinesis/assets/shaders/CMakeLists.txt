#find_program(GLSLC glslc HINTS 
#    /usr/bin 
#    /usr/local/bin 
#    $ENV{VULKAN_SDK}/Bin/ 
#    $ENV{VULKAN_SDK}/Bin32/)

find_program(GLSLANGVALIDATOR glslangValidator HINTS 
    /usr/bin 
    /usr/local/bin 
    $ENV{VULKAN_SDK}/Bin/ 
    $ENV{VULKAN_SDK}/Bin32/)

#if (NOT GLSLC)
#    message(FATAL_ERROR "glslc not found at ${GLSLC}")
#endif ()

if (NOT GLSLANGVALIDATOR)
    message(FATAL_ERROR "glslangValidator not found at ${GLSLANGVALIDATOR}")
endif ()

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/kinesis/assets/shaders/")

file(GLOB_RECURSE SHADERS CONFIGURE_DEPENDS
        ${SHADER_DIR}/*.frag
        ${SHADER_DIR}/*.vert
        ${SHADER_DIR}/*.geom
        ${SHADER_DIR}/*.comp
        ${SHADER_DIR}/*.tesc
        ${SHADER_DIR}/*.tese
)

file(GLOB_RECURSE GLSL_SHADERS CONFIGURE_DEPENDS
        ${SHADER_DIR}/*.glsl
)
message(${SHADER_DIR})
set(SPIRV_BIN "${SHADER_DIR}/bin")
file(MAKE_DIRECTORY ${SPIRV_BIN})

function(compile_shader shader)
    get_filename_component(FILE_NAME ${shader} NAME)
    set(SPIRV_OUTPUT "${SPIRV_BIN}/${FILE_NAME}.spv")

    add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            #COMMAND "${GLSLC}" ${shader} -o ${SPIRV_OUTPUT}
            COMMAND "${GLSLANGVALIDATOR}" -V ${shader} -o ${SPIRV_OUTPUT}
            COMMENT "Compiling ${shader} to SPIR-V"
            DEPENDS ${shader} ${GLSL_SHADERS}
            VERBATIM
    )

    list(APPEND SPIRV_SHADERS ${SPIRV_OUTPUT})
    set(SPIRV_SHADERS ${SPIRV_SHADERS} PARENT_SCOPE)
endfunction()

foreach (SHADER ${SHADERS})
    compile_shader(${SHADER})
endforeach ()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_SHADERS})