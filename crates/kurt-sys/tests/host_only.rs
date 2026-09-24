mod support;

use std::mem::MaybeUninit;
use std::ptr;

use kurt_sys::*;
use support::{Object, success, view};

#[test]
#[ignore = "requires a host-only installation without the CPU plugin"]
fn installed_host_works_without_cpu_vendor() {
    let input = ku_union_t {
        value: ku_union_value_t { i64: 42 },
        tag: KU_PRIMITIVE_I64,
    };
    // SAFETY: the scalar descriptors and output slots are live stack storage.
    unsafe {
        let mut raw = ptr::null_mut();
        success(ku_scalar_create(&input, &mut raw));
        let scalar = Object(raw);
        let mut output = MaybeUninit::uninit();
        success(ku_scalar_get_value(scalar.0, output.as_mut_ptr()));
        let output = output.assume_init();
        assert_eq!(output.tag, KU_PRIMITIVE_I64);
        assert_eq!(output.value.i64, 42);
    }

    let device = ku_device_capabilities_t {
        device_id: 0,
        streams: ku_device_stream_capabilities_t {
            max_compute_streams: 0,
            max_copy_streams: 0,
        },
        max_memory_bytes: 0,
    };
    let info = ku_instance_init_info_t {
        vendor: view(b"cpu"),
        default_device_id: 0,
        capabilities: ku_instance_capabilities_t {
            scheduler: ku_scheduler_t {
                ctx: ptr::null_mut(),
                submit: ptr::null(),
            },
            max_worker_concurrency: 2,
            max_host_memory_bytes: 0,
            devices: &device,
            device_count: 1,
        },
    };
    // SAFETY: the descriptor and its device storage remain live through init.
    unsafe {
        let mut instance = ptr::null_mut();
        let status = ku_instance_init(&info, &mut instance);
        if status == KU_STATUS_SUCCESS {
            success(ku_instance_destroy(instance));
        }
        assert_eq!(status, KU_STATUS_BACKEND_UNAVAILABLE);
        assert!(instance.is_null());
    }
}
