
function(cpsw_add_library NAME SRCS)
    if (CPSW_ENABLE_SHARED)
        add_library(
            ${NAME}-shared
            ${SRCS}
        )
        
        set_target_properties(
            ${NAME}-shared
            PROPERTIES OUTPUT_NAME ${NAME}
        )
    endif()

    if (CPSW_ENABLE_STATIC)
        add_library(
            ${NAME}-static
            ${SRCS}
        )
        
        set_target_properties(
            ${NAME}-static
            PROPERTIES OUTPUT_NAME ${NAME}
        )
    endif()
endfunction()

function(cpsw_add_test NAME SRCS RUN_OPTS)
    cmake_parse_arguments(
        arg NAME SRCS RUN_OPTS ${ARGN}
    )

    add_executable(
        ${arg_NAME} ${arg_SRCS}
    )
    
    target_link_libraries(
        ${arg_NAME} PUBLIC TstAux
    )
    
    foreach(OPTS ${arg_RUN_OPTS})
        add_test(
            NAME 
        )
    endforeach()
endfunction()