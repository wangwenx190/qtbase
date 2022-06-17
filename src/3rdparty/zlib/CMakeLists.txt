# Copyright (C) 2024 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause
qt_internal_add_3rdparty_library(BundledZLIB
    STATIC
    SKIP_AUTOMOC
    SOURCES
        # zlib
        src/adler32.c
        src/compress.c
        src/crc32.c
        src/crc32.h
        src/deflate.c
        src/deflate.h
        src/gzclose.c
        src/gzguts.h
        src/gzlib.c
        src/gzread.c
        src/gzwrite.c
        src/infback.c
        src/inffast.c
        src/inffast.h
        src/inffixed.h
        src/inflate.c
        src/inflate.h
        src/inftrees.c
        src/inftrees.h
        src/trees.c
        src/trees.h
        src/uncompr.c
        src/zconf.h
        src/zlib.h
        src/zutil.c
        src/zutil.h
        # zstd
        zstd/zdict.h
        zstd/zstd.h
        zstd/zstd_errors.h
        zstd/common/allocations.h
        zstd/common/bits.h
        zstd/common/bitstream.h
        zstd/common/compiler.h
        zstd/common/cpu.h
        zstd/common/debug.c
        zstd/common/debug.h
        zstd/common/entropy_common.c
        zstd/common/error_private.c
        zstd/common/error_private.h
        zstd/common/fse.h
        zstd/common/fse_decompress.c
        zstd/common/huf.h
        zstd/common/mem.h
        zstd/common/pool.c
        zstd/common/pool.h
        zstd/common/portability_macros.h
        zstd/common/threading.c
        zstd/common/threading.h
        zstd/common/xxhash.c
        zstd/common/xxhash.h
        zstd/common/zstd_common.c
        zstd/common/zstd_deps.h
        zstd/common/zstd_internal.h
        zstd/common/zstd_trace.h
        zstd/compress/clevels.h
        zstd/compress/fse_compress.c
        zstd/compress/hist.c
        zstd/compress/hist.h
        zstd/compress/huf_compress.c
        zstd/compress/zstdmt_compress.c
        zstd/compress/zstdmt_compress.h
        zstd/compress/zstd_compress.c
        zstd/compress/zstd_compress_internal.h
        zstd/compress/zstd_compress_literals.c
        zstd/compress/zstd_compress_literals.h
        zstd/compress/zstd_compress_sequences.c
        zstd/compress/zstd_compress_sequences.h
        zstd/compress/zstd_compress_superblock.c
        zstd/compress/zstd_compress_superblock.h
        zstd/compress/zstd_cwksp.h
        zstd/compress/zstd_double_fast.c
        zstd/compress/zstd_double_fast.h
        zstd/compress/zstd_fast.c
        zstd/compress/zstd_fast.h
        zstd/compress/zstd_lazy.c
        zstd/compress/zstd_lazy.h
        zstd/compress/zstd_ldm.c
        zstd/compress/zstd_ldm.h
        zstd/compress/zstd_ldm_geartab.h
        zstd/compress/zstd_opt.c
        zstd/compress/zstd_opt.h
        zstd/compress/zstd_preSplit.c
        zstd/compress/zstd_preSplit.h
        zstd/decompress/huf_decompress.c
        zstd/decompress/zstd_ddict.c
        zstd/decompress/zstd_ddict.h
        zstd/decompress/zstd_decompress.c
        zstd/decompress/zstd_decompress_block.c
        zstd/decompress/zstd_decompress_block.h
        zstd/decompress/zstd_decompress_internal.h
        zstd/dictBuilder/cover.c
        zstd/dictBuilder/cover.h
        zstd/dictBuilder/divsufsort.c
        zstd/dictBuilder/divsufsort.h
        zstd/dictBuilder/fastcover.c
        zstd/dictBuilder/zdict.c
        # lzma
        lzma/7zTypes.h
        lzma/7zWindows.h
        lzma/Alloc.c
        lzma/Alloc.h
        lzma/Compiler.h
        lzma/CpuArch.c
        lzma/CpuArch.h
        lzma/LzFind.c
        lzma/LzFind.h
        lzma/LzFindMt.c
        lzma/LzFindMt.h
        lzma/LzFindOpt.c
        lzma/LzHash.h
        lzma/LzmaDec.c
        lzma/LzmaDec.h
        lzma/LzmaEnc.c
        lzma/LzmaEnc.h
        lzma/LzmaLib.c
        lzma/LzmaLib.h
        lzma/Precomp.h
        lzma/Threads.c
        lzma/Threads.h
    DEFINES
        QT_BUILD_CORE_LIB
        # zlib
        # zstd
        XXH_NAMESPACE=ZSTD_ # Always hide XXHash symbols
        ZSTD_LEGACY_SUPPORT=0
        ZSTD_MULTITHREAD
    INCLUDE_DIRECTORIES
        $<TARGET_PROPERTY:Core,INCLUDE_DIRECTORIES>
)
qt_internal_add_sync_header_dependencies(BundledZLIB Core)
qt_internal_extend_target(BundledZLIB CONDITION MSVC
    DEFINES
        ZSTD_DISABLE_ASM
        ZSTD_HEAPMODE=0
        _CRT_SECURE_NO_WARNINGS
)
qt_internal_extend_target(BundledZLIB CONDITION NOT MSVC
    SOURCES
        zstd/decompress/huf_decompress_amd64.S
)
qt_disable_warnings(BundledZLIB)
qt_set_symbol_visibility_hidden(BundledZLIB)
qt_internal_add_3rdparty_header_module(ZlibPrivate
    EXTERNAL_HEADERS
        # zlib
        src/zlib.h
        src/zconf.h
        # zstd
        zstd/zdict.h
        zstd/zstd.h
        zstd/zstd_errors.h
        # lzma
        lzma/LzmaLib.h
)
if(WIN32 AND MSVC AND TEST_architecture_arch STREQUAL "x86_64")
    enable_language(ASM_MASM)
    set(lzma_asm_sources lzma/LzFindOpt.asm lzma/LzmaDecOpt.asm)
    set_source_files_properties(${lzma_asm_sources} PROPERTIES
        LANGUAGE ASM_MASM
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/lzma/7zAsm.asm"
    )
    add_library(BundledZLIB_lzma_asm OBJECT ${lzma_asm_sources})
    target_include_directories(BundledZLIB_lzma_asm PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/lzma"
    )
    qt_internal_extend_target(BundledZLIB
        SOURCES $<TARGET_OBJECTS:BundledZLIB_lzma_asm>
    )
    qt_internal_extend_target(BundledZLIB
        DEFINES
            Z7_LZ_FIND_OPT
            Z7_LZMA_DEC_OPT
    )
endif()
