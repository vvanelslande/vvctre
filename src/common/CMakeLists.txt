add_library(common STATIC
    alignment.h
    assert.h
    bit_field.h
    bit_set.h
    cityhash.cpp
    cityhash.h
    color.h
    common_funcs.h
    common_types.h
    fastmem_mapper.h
    file_util.cpp
    file_util.h
    hash.h
    logging/backend.cpp
    logging/backend.h
    logging/filter.cpp
    logging/filter.h
    logging/log.h
    logging/text_formatter.cpp
    logging/text_formatter.h
    math_util.h
    misc.cpp
    param_package.cpp
    param_package.h
    quaternion.h
    ring_buffer.h
    scope_exit.h
    string_util.cpp
    string_util.h
    swap.h
    texture.cpp
    texture.h
    thread.h
    thread_queue_list.h
    threadsafe_queue.h
    vector_math.h
)

create_target_directory_groups(common)

if(UNIX)
    target_sources(common PRIVATE fastmem_mapper_posix.cpp)
else()
    target_sources(common PRIVATE fastmem_mapper_generic.cpp)
endif()

if(ARCHITECTURE_x86_64)
    target_sources(common
        PRIVATE
            x64/cpu_detect.cpp
            x64/cpu_detect.h
            x64/xbyak_abi.h
            x64/xbyak_util.h
    )
endif()

target_link_libraries(common PUBLIC fmt ${PLATFORM_LIBRARIES} PRIVATE mbedtls whereami utf8cpp)
target_include_directories(common PRIVATE ${PROJECT_SOURCE_DIR}/externals/mbedtls/include)

if (MSVC)
    target_link_libraries(common PRIVATE Crypt32.lib)
endif()

if (ARCHITECTURE_x86_64)
    target_link_libraries(common PRIVATE xbyak)
endif()
