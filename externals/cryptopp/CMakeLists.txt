include(TestBigEndian)
include(CheckCXXCompilerFlag)

TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    add_definitions(-DIS_BIG_ENDIAN)
endif()

if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4390")
else()
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

add_library(cryptopp STATIC
    cryptopp/cryptlib.cpp
    cryptopp/cryptlib.h
    cryptopp/cpu.cpp
    cryptopp/cpu.h
    cryptopp/integer.cpp
    cryptopp/integer.h
    cryptopp/algparam.cpp
    cryptopp/algparam.h
    cryptopp/allocate.cpp
    cryptopp/allocate.h
    cryptopp/asn.cpp
    cryptopp/asn.h
    cryptopp/authenc.cpp
    cryptopp/authenc.h
    cryptopp/base64.cpp
    cryptopp/base64.h
    cryptopp/basecode.cpp
    cryptopp/basecode.h
    cryptopp/ccm.cpp
    cryptopp/ccm.h
    cryptopp/crc_simd.cpp
    cryptopp/des.cpp
    cryptopp/des.h
    cryptopp/dessp.cpp
    cryptopp/dll.cpp
    cryptopp/dll.h
    cryptopp/ec2n.cpp
    cryptopp/ec2n.h
    cryptopp/ecp.cpp
    cryptopp/ecp.h
    cryptopp/filters.cpp
    cryptopp/filters.h
    cryptopp/fips140.cpp
    cryptopp/fips140.h
    cryptopp/gcm_simd.cpp
    cryptopp/gf2n.cpp
    cryptopp/gf2n.h
    cryptopp/gf2n_simd.cpp
    cryptopp/gfpcrypt.cpp
    cryptopp/gfpcrypt.h
    cryptopp/hex.cpp
    cryptopp/hex.h
    cryptopp/hmac.cpp
    cryptopp/hmac.h
    cryptopp/hrtimer.cpp
    cryptopp/iterhash.cpp
    cryptopp/iterhash.h
    cryptopp/md5.cpp
    cryptopp/md5.h
    cryptopp/misc.cpp
    cryptopp/misc.h
    cryptopp/modes.cpp
    cryptopp/modes.h
    cryptopp/mqueue.cpp
    cryptopp/mqueue.h
    cryptopp/nbtheory.cpp
    cryptopp/nbtheory.h
    cryptopp/neon_simd.cpp
    cryptopp/oaep.cpp
    cryptopp/oaep.h
    cryptopp/osrng.cpp
    cryptopp/osrng.h
    cryptopp/pubkey.cpp
    cryptopp/pubkey.h
    cryptopp/queue.cpp
    cryptopp/queue.h
    cryptopp/randpool.cpp
    cryptopp/randpool.h
    cryptopp/rdtables.cpp
    cryptopp/rijndael_simd.cpp
    cryptopp/rijndael.cpp
    cryptopp/rijndael.h
    cryptopp/rng.cpp
    cryptopp/rng.h
    cryptopp/sha_simd.cpp
    cryptopp/sha.cpp
    cryptopp/sha.h
    cryptopp/sse_simd.cpp
)

if(WIN32)
    target_sources(cryptopp PRIVATE cryptopp/x64dll.asm)
    target_sources(cryptopp PRIVATE cryptopp/x64masm.asm)
    set_source_files_properties(cryptopp/x64dll.asm PROPERTIES COMPILE_FLAGS "/D_M_X64")
    set_source_files_properties(cryptopp/x64masm.asm PROPERTIES COMPILE_FLAGS "/D_M_X64")
    enable_language(ASM_MASM)
endif()

if(NOT WIN32)
    check_cxx_compiler_flag(-msse2 CRYPTOPP_HAS_MSSE2)
    check_cxx_compiler_flag(-mssse3 CRYPTOPP_HAS_MSSSE3)
    check_cxx_compiler_flag(-msse4.1 CRYPTOPP_HAS_MSSE41)
    check_cxx_compiler_flag(-msse4.2 CRYPTOPP_HAS_MSSE42)
    check_cxx_compiler_flag(-maes CRYPTOPP_HAS_MAES)
    check_cxx_compiler_flag(-mpclmul CRYPTOPP_HAS_PCLMUL)
    check_cxx_compiler_flag(-msha CRYPTOPP_HAS_MSHA)

    if (CRYPTOPP_HAS_MSSE2)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/sse_simd.cpp PROPERTIES COMPILE_FLAGS "-msse2")
    endif()

    if (CRYPTOPP_HAS_MSSSE3 AND CRYPTOPP_HAS_MAES AND CRYPTOPP_HAS_PCLMUL)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/gcm_simd.cpp
                                    ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/gf2n_simd.cpp
                                    PROPERTIES COMPILE_FLAGS "-mssse3 -maes -mpclmul")
    endif()

    if (CRYPTOPP_HAS_MSSE41 AND CRYPTOPP_HAS_MAES)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/rijndael_simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.1 -maes")
    endif()

    if (CRYPTOPP_HAS_MSSE42)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/crc_simd.cpp
                                    PROPERTIES COMPILE_FLAGS "-msse4.2")
    endif()

    if (CRYPTOPP_HAS_MSSE42 AND CRYPTOPP_HAS_MSHA)
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/cryptopp/sha_simd.cpp PROPERTIES COMPILE_FLAGS "-msse4.2 -msha")
    endif()
endif()

target_include_directories(cryptopp INTERFACE .)

find_package(Threads)
target_link_libraries(cryptopp PRIVATE Threads::Threads)
