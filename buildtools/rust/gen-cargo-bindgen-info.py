# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Intel Corporation

# Script to generate cargo rules for static linking in rust builds
# outputs one line per library, for drivers and libraries

import os
import os.path
import sys
import subprocess

if 'MESON_BUILD_ROOT' not in os.environ:
    print('This script must be called from a meson build environment')
    sys.exit(1)

pkgconf = sys.argv[1]
os.environ['PKG_CONFIG_PATH'] = os.path.join(os.environ['MESON_BUILD_ROOT'], 'meson-uninstalled')
linker_flags = subprocess.check_output([pkgconf, '--libs', '--static', 'libdpdk']).decode('utf-8')
cflags = subprocess.check_output([pkgconf, '--cflags', 'libdpdk']).decode('utf-8')

whole_archive = False
with open(os.path.join(os.environ['MESON_BUILD_ROOT'], 'cargo_ldflags.txt'), 'w') as dst:
    for flag in linker_flags.split():
        if flag == '-pthread':
            continue
        elif flag == '-Wl,--whole-archive':
            whole_archive = True
        elif flag == '-Wl,--no-whole-archive':
            whole_archive = False
        elif flag.startswith('-L'):
            dst.write(f'cargo:rustc-link-search=native={flag[2:]}\n')
        elif flag.startswith('-l:'):
            libname = flag[3:]
            if libname.startswith('lib'):
                libname = libname[3:]
            if libname.endswith('.a'):
                libname = libname[:-2]
            if whole_archive:
                dst.write(f'cargo:rustc-link-lib=static:+whole-archive={libname}\n')
            else:
                dst.write(f'cargo:rustc-link-lib=static={libname}\n')
        elif flag.startswith('-lrte_'):
            # skip any other DPDK lib flags, we already have them above
            continue
        elif flag.startswith('-l'):
            dst.write(f'cargo:rustc-link-lib={flag[2:]}\n')
        else:
            print(f'Warning: Unknown flag: {flag}', file=sys.stderr)

with open(os.path.join(os.environ['MESON_BUILD_ROOT'], 'bindgen_cflags.txt'), 'w') as dst:
    for flag in cflags.split():
        dst.write(f'{flag}\n')
