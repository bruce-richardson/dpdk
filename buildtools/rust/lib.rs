/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_helloworld() {
	let appname = std::ffi::CString::new("test-rs").unwrap();
	let mut argv = [appname.into_raw()];
	let ret = unsafe {
		rte_eal_init(argv.len().try_into().unwrap(), argv.as_mut_ptr())
	};
	assert!(ret >= 0, "rte_eal_init failed");
	assert!(unsafe { dpdk_lcore_id() } == 0, "Failed to get expected lcore id");
        unsafe { rte_eal_cleanup() };
    }
}
