qt_internal_add_3rdparty_library(BundledZSTD
    STATIC
    SKIP_AUTOMOC
    SOURCES
        src/zdict.h
        src/zstd.h
        src/zstd_errors.h
        src/common/allocations.h
        src/common/bits.h
        src/common/bitstream.h
        src/common/compiler.h
        src/common/cpu.h
        src/common/debug.c
        src/common/debug.h
        src/common/entropy_common.c
        src/common/error_private.c
        src/common/error_private.h
        src/common/fse.h
        src/common/fse_decompress.c
        src/common/huf.h
        src/common/mem.h
        src/common/pool.c
        src/common/pool.h
        src/common/portability_macros.h
        src/common/threading.c
        src/common/threading.h
        src/common/xxhash.c
        src/common/xxhash.h
        src/common/zstd_common.c
        src/common/zstd_deps.h
        src/common/zstd_internal.h
        src/common/zstd_trace.h
        src/compress/clevels.h
        src/compress/fse_compress.c
        src/compress/hist.c
        src/compress/hist.h
        src/compress/huf_compress.c
        src/compress/zstdmt_compress.c
        src/compress/zstdmt_compress.h
        src/compress/zstd_compress.c
        src/compress/zstd_compress_internal.h
        src/compress/zstd_compress_literals.c
        src/compress/zstd_compress_literals.h
        src/compress/zstd_compress_sequences.c
        src/compress/zstd_compress_sequences.h
        src/compress/zstd_compress_superblock.c
        src/compress/zstd_compress_superblock.h
        src/compress/zstd_cwksp.h
        src/compress/zstd_double_fast.c
        src/compress/zstd_double_fast.h
        src/compress/zstd_fast.c
        src/compress/zstd_fast.h
        src/compress/zstd_lazy.c
        src/compress/zstd_lazy.h
        src/compress/zstd_ldm.c
        src/compress/zstd_ldm.h
        src/compress/zstd_ldm_geartab.h
        src/compress/zstd_opt.c
        src/compress/zstd_opt.h
        src/decompress/huf_decompress.c
        src/decompress/zstd_ddict.c
        src/decompress/zstd_ddict.h
        src/decompress/zstd_decompress.c
        src/decompress/zstd_decompress_block.c
        src/decompress/zstd_decompress_block.h
        src/decompress/zstd_decompress_internal.h
        src/dictBuilder/cover.c
        src/dictBuilder/cover.h
        src/dictBuilder/divsufsort.c
        src/dictBuilder/divsufsort.h
        src/dictBuilder/fastcover.c
        src/dictBuilder/zdict.c
    DEFINES
        XXH_NAMESPACE=ZSTD_ # Always hide XXHash symbols
        ZSTD_LEGACY_SUPPORT=0
        ZSTD_MULTITHREAD
        QT_BUILD_CORE_LIB
    INCLUDE_DIRECTORIES
        $<TARGET_PROPERTY:Core,INCLUDE_DIRECTORIES>
)

qt_internal_extend_target(BundledZSTD CONDITION MSVC
    DEFINES
        ZSTD_DISABLE_ASM
        ZSTD_HEAPMODE=0
        _CRT_SECURE_NO_WARNINGS
)

qt_internal_extend_target(BundledZSTD CONDITION NOT MSVC
    SOURCES
        src/decompress/huf_decompress_amd64.S
)

qt_internal_add_sync_header_dependencies(BundledZSTD Core)

qt_disable_warnings(BundledZSTD)

qt_set_symbol_visibility_hidden(BundledZSTD)

qt_internal_add_3rdparty_header_module(ZstdPrivate
    EXTERNAL_HEADERS
        src/zdict.h
        src/zstd.h
        src/zstd_errors.h
)
