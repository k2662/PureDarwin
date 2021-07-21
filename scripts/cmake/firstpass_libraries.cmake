function(add_firstpass_library original)
    get_property(insdir TARGET ${original} PROPERTY INSTALL_NAME_DIR)
    add_darwin_shared_library(${original}.firstpass NO_STANDARD_LIBRARIES INSTALL_NAME_DIR ${insdir})
    target_link_options(${original}.firstpass PRIVATE "LINKER:-undefined,suppress,-flat_namespace")

    set_property(TARGET ${original}.firstpass PROPERTY OUTPUT_NAME ${original})
    set_property(TARGET ${original}.firstpass PROPERTY SUFFIX ".firstpass_dylib")

    get_property(srcs TARGET ${original} PROPERTY SOURCES)
    set_property(TARGET ${original}.firstpass PROPERTY SOURCES ${srcs})
    get_property(defs TARGET ${original} PROPERTY COMPILE_DEFINITIONS)
    set_property(TARGET ${original}.firstpass PROPERTY COMPILE_DEFINITIONS ${defs})

    get_property(incs TARGET ${original} PROPERTY INCLUDE_DIRECTORIES)
    target_include_directories(${original}.firstpass PRIVATE ${incs})
    target_get_library_dependencies(${original} deplist)
    foreach(dep IN LISTS deplist)
        get_property(incs TARGET ${dep} PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
        target_include_directories(${original}.firstpass PRIVATE ${incs})
    endforeach()
endfunction()

function(target_link_firstpass_siblings target)
    list(GET ARGN 0 first_argn)
    if(first_argn STREQUAL "UPWARD")
        set(is_upward TRUE)
        list(REMOVE_AT ARGN 0)
    else()
        set(is_upward FALSE)
    endif()

    foreach(dep IN LISTS ARGN)
        if(is_upward)
            target_link_options(${target}.firstpass PRIVATE "LINKER:SHELL:-upward-library $<TARGET_FILE:${dep}.firstpass>")
            target_link_options(${target} PRIVATE "LINKER:SHELL:-upward-library $<TARGET_FILE:${dep}.firstpass>")
            add_dependencies(${target}.firstpass ${dep}.firstpass)
            add_dependencies(${target} ${dep}.firstpass)
        else()
            target_link_libraries(${target}.firstpass PRIVATE ${dep}.firstpass)
            target_link_libraries(${target} PRIVATE ${dep}.firstpass)
        endif()
    endforeach()
endfunction()

function(target_link_firstpass_libraries target)
    list(GET ARGN 0 first_argn)
    if(first_argn STREQUAL "UPWARD")
        set(is_upward TRUE)
        list(REMOVE_AT ARGN 0)
    else()
        set(is_upward FALSE)
    endif()

    foreach(dep IN LISTS ARGN)
        if(is_upward)
            target_link_options(${target} PRIVATE "LINKER:SHELL:-upward-library $<TARGET_FILE:${dep}.firstpass>")
            add_dependencies(${target} ${dep}.firstpass)
        else()
            target_link_libraries(${target} PRIVATE ${dep}.firstpass)
        endif()
    endforeach()
endfunction()