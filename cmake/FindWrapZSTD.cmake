# Copyright (C) 2023 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

include(QtFindWrapHelper NO_POLICY_SCOPE)

qt_find_package_system_or_bundled(wrap_zstd
    FRIENDLY_PACKAGE_NAME "ZSTD"
    WRAP_PACKAGE_TARGET "WrapZSTD::WrapZSTD"
    WRAP_PACKAGE_FOUND_VAR_NAME "WrapZSTD_FOUND"
    BUNDLED_PACKAGE_NAME "BundledZSTD"
    BUNDLED_PACKAGE_TARGET "BundledZSTD"
    SYSTEM_PACKAGE_NAME "WrapSystemZSTD"
    SYSTEM_PACKAGE_TARGET "WrapSystemZSTD::WrapSystemZSTD"
)
