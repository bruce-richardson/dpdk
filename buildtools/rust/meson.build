# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Intel Corporation

if not fs.exists(meson.project_build_root() / 'meson-uninstalled' / 'libdpdk-uninstalled.pc')
    message('Cannot find pkg-config file to generate cargo bindgen parameters, skipping.')
    subdir_done()
endif

# output a list of cargo parameters if DPDK is being built for rust use
message('Generating compiler and linker flags for Rust bindings using bindgen')
run_command([gen_cargo_bindgen_info, pkgconf], check: true)
