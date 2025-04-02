/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

extern crate meson_next as meson;
use std::collections::HashMap;
use std::env;
use std::path::PathBuf;

fn main() {
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    let build_dir = out_path.join("build");

    let meson_cfg = meson::Config::new().options(HashMap::from([
        ("enable_libs", "eal"),
        ("enable_drivers", "net/*,net/intel/*"),
        ("enable_apps", "test")
    ]));
    meson::build(".", build_dir.to_str().unwrap(), meson_cfg);

    /* open and print file 'cargo_rules.txt' from build_dir */
    let cargo_ldflags_file = build_dir.join("cargo_ldflags.txt");
    println!("cargo:rerun-if-changed={}", cargo_ldflags_file.display());
    print!("{}", std::fs::read_to_string(cargo_ldflags_file).unwrap());

    let bindgen_include_file = build_dir.join("bindgen_cflags.txt");
    let mut bindings = bindgen::Builder::default();
    for line in std::fs::read_to_string(bindgen_include_file).unwrap().lines() {
            bindings = bindings.clang_arg(line);
    }

    let bindings = bindings.header("buildtools/rust/wrapper.h")
        .derive_default(true)
        .allowlist_function("rte_eal_init")
        .allowlist_function("rte_eal_cleanup")
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings");

    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
