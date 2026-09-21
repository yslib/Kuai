#![cfg(kuai_runtime_cpu)]

mod support;

use std::ffi::c_void;
use std::mem::{MaybeUninit, size_of_val};
use std::ptr;
use std::sync::{Mutex, MutexGuard, mpsc};
use std::time::Duration;

use kuai_sys::*;
use support::{Object, success, view};

// The runtime permits only one active instance per vendor in a process.
static CPU_LOCK: Mutex<()> = Mutex::new(());
const DEVICE: ku_device_capabilities_t = ku_device_capabilities_t {
    device_id: 0,
    streams: ku_device_stream_capabilities_t {
        max_compute_streams: 0,
        max_copy_streams: 0,
    },
    max_memory_bytes: 0,
};

fn init_info(scheduler: ku_scheduler_t) -> ku_instance_init_info_t {
    ku_instance_init_info_t {
        vendor: view(b"cpu"),
        default_device_id: 0,
        capabilities: ku_instance_capabilities_t {
            scheduler,
            max_worker_concurrency: 2,
            max_host_memory_bytes: 0,
            devices: &DEVICE,
            device_count: 1,
        },
    }
}

fn default_scheduler() -> ku_scheduler_t {
    ku_scheduler_t {
        ctx: ptr::null_mut(),
        submit: ptr::null(),
    }
}

struct Cpu {
    instance: ku_instance_t,
    device: ku_device_t,
    _lock: MutexGuard<'static, ()>,
}

impl Cpu {
    fn new(scheduler: ku_scheduler_t) -> Self {
        let lock = CPU_LOCK.lock().unwrap_or_else(|poison| poison.into_inner());
        // SAFETY: descriptor storage is valid through init and copied by the
        // runtime. Tests keep any scheduler context alive until instance teardown.
        unsafe {
            let mut instance = ptr::null_mut();
            success(ku_instance_init(&init_info(scheduler), &mut instance));
            let mut cpu = Self {
                instance,
                device: ptr::null_mut(),
                _lock: lock,
            };
            success(ku_instance_get_default_device(instance, &mut cpu.device));
            cpu
        }
    }
}

impl Drop for Cpu {
    fn drop(&mut self) {
        // SAFETY: tests drop all derived resources before their Cpu guard.
        unsafe { success(ku_instance_destroy(self.instance)) }
    }
}

struct Completion(ku_completion_t);

impl Drop for Completion {
    fn drop(&mut self) {
        // SAFETY: the guard owns one reference. Waiting also keeps borrowed
        // buffers alive if a test assertion unwinds before its explicit wait.
        unsafe {
            success(ku_completion_wait(self.0));
            success(ku_completion_release(self.0));
        }
    }
}

struct FrameContext(ku_frame_ctx_t);

impl Drop for FrameContext {
    fn drop(&mut self) {
        unsafe { ku_frame_ctx_destroy(self.0) }
    }
}

fn tensor_size(tensor: &Object) -> usize {
    // SAFETY: the owned object stays alive while its borrowed shape is read.
    // Successful kind checking establishes that all ndim entries are valid.
    unsafe {
        let mut info = MaybeUninit::uninit();
        success(ku_tensor_get_info(tensor.0, info.as_mut_ptr()));
        let info = info.assume_init();
        if info.ndim == 0 {
            assert!(info.shape.is_null());
            assert!(info.strides.is_null());
            return 1;
        }
        std::slice::from_raw_parts(info.shape, info.ndim as usize)
            .iter()
            .product()
    }
}

#[test]
fn explicit_streams_are_owned_and_distinct_from_the_per_thread_stream() {
    let cpu = Cpu::new(default_scheduler());
    // SAFETY: the vendor table and all streams stay within the Cpu lifetime.
    // Each explicitly created stream is destroyed once after synchronization.
    unsafe {
        let mut table = ptr::null();
        success(ku_device_get_vendor_api(cpu.device, &mut table));
        let api = &*table;
        let mut first = ptr::null_mut();
        let mut second = ptr::null_mut();
        success((api.stream_create)(api.ctx, &mut first));
        success((api.stream_create)(api.ctx, &mut second));
        let mut default_stream = ptr::null_mut();
        success(ku_device_get_default_stream(
            cpu.device,
            &mut default_stream,
        ));
        let per_thread = (api.get_per_thread_stream)(api.ctx);
        let distinct = !first.is_null()
            && !second.is_null()
            && first != second
            && first != per_thread
            && second != per_thread;
        for stream in [first, second] {
            success((api.stream_synchronize)(api.ctx, stream));
            success((api.stream_query)(api.ctx, stream));
            success((api.stream_destroy)(api.ctx, stream));
        }
        assert!(
            distinct,
            "explicit live streams must have independent owned handles"
        );
        assert!(!default_stream.is_null());
        assert_ne!(default_stream, per_thread);
        success(ku_device_synchronize(cpu.device, default_stream));
    }
}

#[test]
fn instance_device_capabilities_and_lifecycle() {
    let cpu = Cpu::new(default_scheduler());
    // SAFETY: handles remain live under the Cpu guard; outputs are writable.
    unsafe {
        let mut duplicate = ptr::null_mut();
        assert_eq!(
            ku_instance_init(&init_info(default_scheduler()), &mut duplicate),
            KU_STATUS_ALREADY_INITIALIZED
        );
        assert!(duplicate.is_null());

        let mut device = ptr::null_mut();
        success(ku_instance_get_device(cpu.instance, 0, &mut device));
        assert_eq!(device, cpu.device);
        assert_eq!(
            ku_instance_get_device(cpu.instance, 999, &mut device),
            KU_STATUS_OUT_OF_RANGE
        );

        let mut info = MaybeUninit::uninit();
        success(ku_device_get_info(cpu.device, info.as_mut_ptr()));
        assert_eq!(
            info.assume_init(),
            ku_device_info_t {
                device_type: KU_DEVICE_CPU,
                device_id: 0
            }
        );
        let mut capabilities = MaybeUninit::uninit();
        success(ku_device_get_capabilities(
            cpu.device,
            capabilities.as_mut_ptr(),
        ));
        assert_eq!(capabilities.assume_init(), DEVICE);
        let mut capabilities = MaybeUninit::uninit();
        success(ku_instance_get_capabilities(
            cpu.instance,
            capabilities.as_mut_ptr(),
        ));
        let capabilities = capabilities.assume_init();
        assert_eq!(capabilities.device_count, 1);
        assert_eq!(*capabilities.devices, DEVICE);
        assert_eq!(capabilities.max_worker_concurrency, 2);
        assert_eq!(capabilities.max_host_memory_bytes, 0);
        assert!(!capabilities.scheduler.submit.is_null());

        let mut ctx = ptr::null_mut();
        success(ku_frame_ctx_create(cpu.device, &mut ctx));
        let ctx = FrameContext(ctx);
        success(ku_frame_ctx_get_device(ctx.0, &mut device));
        assert_eq!(device, cpu.device);
        let mut stream = ptr::null_mut();
        success(ku_device_get_default_stream(cpu.device, &mut stream));
        success(ku_device_synchronize(cpu.device, stream));
        success(ku_device_flush(cpu.device));
        success(ku_instance_flush(cpu.instance));
    }
    drop(cpu);
    // Destruction releases the vendor slot, allowing a fresh instance.
    drop(Cpu::new(default_scheduler()));
}

#[test]
fn failed_instance_initialization_preserves_status_and_allows_retry() {
    let lock = CPU_LOCK.lock().unwrap_or_else(|poison| poison.into_inner());
    let mut info = init_info(default_scheduler());
    let devices = [
        DEVICE,
        ku_device_capabilities_t {
            device_id: 1,
            ..DEVICE
        },
    ];

    // SAFETY: descriptors stay alive through initialization. The held lock
    // serializes failed attempts and is transferred to the successful instance.
    unsafe {
        let mut instance = ptr::null_mut();
        info.default_device_id = 1;
        assert_eq!(
            ku_instance_init(&info, &mut instance),
            KU_STATUS_OUT_OF_RANGE
        );
        assert!(instance.is_null());

        // CPU 0 is initialized before CPU 1 fails backend device validation.
        info.default_device_id = 0;
        info.capabilities.devices = devices.as_ptr();
        info.capabilities.device_count = devices.len();
        assert_eq!(
            ku_instance_init(&info, &mut instance),
            KU_STATUS_OUT_OF_RANGE
        );
        assert!(instance.is_null());

        success(ku_instance_init(
            &init_info(default_scheduler()),
            &mut instance,
        ));
        let mut cpu = Cpu {
            instance,
            device: ptr::null_mut(),
            _lock: lock,
        };
        success(ku_instance_get_default_device(
            cpu.instance,
            &mut cpu.device,
        ));
        success(ku_device_flush(cpu.device));
    }
}

#[test]
fn vendor_function_table_copies_memory() {
    let cpu = Cpu::new(default_scheduler());
    // SAFETY: table borrows cpu; allocations and stream remain live until all
    // copies finish, then are released by their corresponding backend functions.
    unsafe {
        let mut table = ptr::null();
        success(ku_device_get_vendor_api(cpu.device, &mut table));
        let api = &*table;
        let mut count = 0;
        success((api.get_device_count)(api.ctx, &mut count));
        assert!(count > 0);
        success((api.set_device)(api.ctx, 0));
        let mut selected = -1;
        success((api.get_device)(api.ctx, &mut selected));
        assert_eq!(selected, 0);

        let input = [3_i64, -7, 42, i64::MAX];
        let mut output = [0_i64; 4];
        let bytes = size_of_val(&input);
        let mut memory = ptr::null_mut();
        success((api.malloc_device)(api.ctx, &mut memory, bytes));
        let mut host = ptr::null_mut();
        success((api.malloc_host)(api.ctx, &mut host, bytes));
        success((api.memcpy)(
            api.ctx,
            memory,
            input.as_ptr().cast(),
            bytes,
            KU_MEMCPY_HOST_TO_DEVICE,
        ));
        let mut stream = ptr::null_mut();
        success((api.stream_create)(api.ctx, &mut stream));
        success((api.memcpy_async)(
            api.ctx,
            host,
            memory,
            bytes,
            KU_MEMCPY_DEVICE_TO_HOST,
            stream,
        ));
        success((api.stream_synchronize)(api.ctx, stream));
        success((api.stream_query)(api.ctx, stream));
        assert_eq!(
            std::slice::from_raw_parts(host.cast::<i64>(), input.len()),
            input
        );

        let mut pool = ptr::null_mut();
        success((api.memory_pool_create)(api.ctx, 0, &mut pool));
        let mut pooled = ptr::null_mut();
        success((api.malloc_from_pool_async)(
            api.ctx,
            &mut pooled,
            bytes,
            pool,
            stream,
        ));
        success((api.memcpy_async)(
            api.ctx,
            pooled,
            memory,
            bytes,
            KU_MEMCPY_DEVICE_TO_DEVICE,
            stream,
        ));
        success((api.memcpy_async)(
            api.ctx,
            output.as_mut_ptr().cast(),
            pooled,
            bytes,
            KU_MEMCPY_DEVICE_TO_HOST,
            stream,
        ));
        success((api.stream_synchronize)(api.ctx, stream));
        assert_eq!(output, input);
        success((api.free_async)(api.ctx, pooled, stream));
        success((api.stream_synchronize)(api.ctx, stream));
        success((api.memory_pool_destroy)(api.ctx, pool));
        success((api.stream_destroy)(api.ctx, stream));
        success((api.free_host)(api.ctx, host));
        success((api.free_device)(api.ctx, memory));
        let borrowed_stream = (api.get_per_thread_stream)(api.ctx);
        success((api.stream_synchronize)(api.ctx, borrowed_stream));
    }
}

unsafe extern "C" fn send_completion(user_data: *mut c_void, status: ku_status_t) {
    // SAFETY: registration transfers this Box to the one-shot callback.
    let sender = unsafe { Box::from_raw(user_data.cast::<mpsc::Sender<ku_status_t>>()) };
    let _ = sender.send(status);
}

#[test]
fn tensor_device_borrows_instance_and_preserves_creation_identity() {
    let cpu = Cpu::new(default_scheduler());
    let shape = [2_i64, 3];
    let desc = ku_tensor_create_desc_t {
        device: cpu.device,
        primitive_type: KU_PRIMITIVE_I64,
        ndim: 2,
        shape: shape.as_ptr(),
        strides: ptr::null(),
    };
    // SAFETY: each Object guard owns a native reference and is dropped before
    // cpu. The queried device borrows cpu, so it remains live after both drops.
    unsafe {
        let mut raw = ptr::null_mut();
        success(ku_tensor_create(&desc, &mut raw));
        let original = Object(raw);
        let mut device = ptr::null_mut();
        for _ in 0..3 {
            success(ku_tensor_get_device(original.0, &mut device));
            assert_eq!(device, cpu.device);
        }

        success(ku_object_retain(original.0));
        let retained = Object(original.0);
        drop(original);
        success(ku_tensor_get_device(retained.0, &mut device));
        assert_eq!(device, cpu.device);
        drop(retained);

        let mut info = MaybeUninit::uninit();
        success(ku_device_get_info(device, info.as_mut_ptr()));
        assert_eq!(
            info.assume_init(),
            ku_device_info_t {
                device_type: KU_DEVICE_CPU,
                device_id: 0,
            }
        );
    }
}

#[test]
fn tensor_transfer_completion_and_borrowed_info() {
    let cpu = Cpu::new(default_scheduler());
    let input = [1_i64, -2, 3, 4, 5, 6];
    let shape = [2_i64, 3];
    let desc = ku_tensor_create_desc_t {
        device: cpu.device,
        primitive_type: KU_PRIMITIVE_I64,
        ndim: 2,
        shape: shape.as_ptr(),
        strides: ptr::null(),
    };
    // SAFETY: all host storage outlives transfer completion; borrowed CPU data
    // is accessed only after a successful wait. Guards preserve ownership order.
    unsafe {
        let mut tensor = ptr::null_mut();
        let mut completion = ptr::null_mut();
        success(ku_tensor_create_from_host_async(
            &desc,
            input.as_ptr().cast(),
            size_of_val(&input),
            &mut tensor,
            &mut completion,
        ));
        let tensor = Object(tensor);
        let completion = Completion(completion);
        success(ku_completion_retain(completion.0));
        let retained = Completion(completion.0);
        let (sender, receiver) = mpsc::channel::<ku_status_t>();
        let sender = Box::into_raw(Box::new(sender));
        let status = ku_completion_on_completion(completion.0, send_completion, sender.cast());
        if status != KU_STATUS_SUCCESS {
            drop(Box::from_raw(sender));
        }
        success(status);
        success(ku_completion_wait(completion.0));
        assert_eq!(
            receiver.recv_timeout(Duration::from_secs(5)).unwrap(),
            KU_STATUS_SUCCESS
        );
        drop(completion);
        success(ku_completion_wait(retained.0));

        let mut device = ptr::null_mut();
        success(ku_tensor_get_device(tensor.0, &mut device));
        assert_eq!(device, cpu.device);
        assert_eq!(tensor_size(&tensor), 6);
        let mut kind = 0;
        success(ku_object_get_value_kind(tensor.0, &mut kind));
        assert_eq!(kind, KU_VALUE_TENSOR);
        let mut info = MaybeUninit::uninit();
        success(ku_tensor_get_info(tensor.0, info.as_mut_ptr()));
        let info = info.assume_init();
        assert_eq!(info.primitive_type, KU_PRIMITIVE_I64);
        assert_eq!(info.ndim, 2);
        assert_eq!(std::slice::from_raw_parts(info.shape, 2), [2, 3]);
        assert_eq!(std::slice::from_raw_parts(info.strides, 2), [1, 2]);
        let copied_shape = std::slice::from_raw_parts(info.shape, 2).to_vec();
        let copied_strides = std::slice::from_raw_parts(info.strides, 2).to_vec();
        let mut data = ptr::null_mut();
        success(ku_tensor_get_data(tensor.0, &mut data));
        assert!(!data.is_null());
        assert_eq!(
            std::slice::from_raw_parts(data.cast::<i64>(), input.len()),
            input
        );
        success(ku_object_retain(tensor.0));
        let retained_tensor = Object(tensor.0);
        drop(tensor);

        // Any retained reference to the same tensor preserves both borrowed views.
        assert_eq!(std::slice::from_raw_parts(info.shape, 2), [2, 3]);
        assert_eq!(std::slice::from_raw_parts(info.strides, 2), [1, 2]);
        let mut output = [0_i64; 6];
        let mut stream = ptr::null_mut();
        success(ku_device_get_default_stream(cpu.device, &mut stream));
        let mut copied = ptr::null_mut();
        success(ku_device_copy_async(
            cpu.device,
            output.as_mut_ptr().cast(),
            data,
            size_of_val(&output),
            KU_MEMCPY_DEVICE_TO_HOST,
            stream,
            &mut copied,
        ));
        let copied = Completion(copied);
        success(ku_completion_wait(copied.0));
        assert_eq!(output, input);
        drop(copied);
        drop(retained_tensor);
        // Only independent copies can be observed after the final tensor release.
        assert_eq!(copied_shape, [2, 3]);
        assert_eq!(copied_strides, [1, 2]);
    }
}

#[test]
fn tensor_descriptor_validation_and_late_completion_callback() {
    let cpu = Cpu::new(default_scheduler());
    let shape = [2_i64, 3];
    let mut desc = ku_tensor_create_desc_t {
        device: cpu.device,
        primitive_type: KU_PRIMITIVE_F32,
        ndim: 2,
        shape: shape.as_ptr(),
        strides: ptr::null(),
    };
    // SAFETY: invalid descriptors contain live pointers; we never pass invalid
    // direct handles. For the empty tensor, input still has a non-null address.
    unsafe {
        let mut raw = ptr::null_mut();
        let bad_strides = [3_i64, 1];
        desc.strides = bad_strides.as_ptr();
        assert_ne!(ku_tensor_create(&desc, &mut raw), KU_STATUS_SUCCESS);
        assert!(raw.is_null());
        desc.strides = ptr::null();
        let input = [0_f32; 6];
        let mut completion = ptr::null_mut();
        assert_eq!(
            ku_tensor_create_from_host_async(
                &desc,
                input.as_ptr().cast(),
                1,
                &mut raw,
                &mut completion
            ),
            KU_STATUS_INVALID_ARGUMENT
        );
        assert!(raw.is_null());
        assert!(completion.is_null());

        desc.ndim = 0;
        desc.shape = ptr::null();
        success(ku_tensor_create(&desc, &mut raw));
        let scalar_tensor = Object(raw);
        assert_eq!(tensor_size(&scalar_tensor), 1);
        let mut info = MaybeUninit::uninit();
        success(ku_tensor_get_info(scalar_tensor.0, info.as_mut_ptr()));
        let info = info.assume_init();
        assert_eq!(info.primitive_type, KU_PRIMITIVE_F32);
        assert_eq!(info.ndim, 0);
        assert!(info.shape.is_null());
        assert!(info.strides.is_null());
        let mut data = ptr::null_mut();
        success(ku_tensor_get_data(scalar_tensor.0, &mut data));
        assert!(!data.is_null());

        let empty_shape = [0_i64];
        desc.ndim = 1;
        desc.shape = empty_shape.as_ptr();
        success(ku_tensor_create_from_host_async(
            &desc,
            input.as_ptr().cast(),
            0,
            &mut raw,
            &mut completion,
        ));
        let empty = Object(raw);
        let completion = Completion(completion);
        success(ku_completion_wait(completion.0));
        let (sender, receiver) = mpsc::channel::<ku_status_t>();
        let sender = Box::into_raw(Box::new(sender));
        let status = ku_completion_on_completion(completion.0, send_completion, sender.cast());
        if status != KU_STATUS_SUCCESS {
            drop(Box::from_raw(sender));
        }
        success(status);
        // Registration after completion invokes the callback before returning.
        assert_eq!(receiver.try_recv().unwrap(), KU_STATUS_SUCCESS);
        assert_eq!(tensor_size(&empty), 0);
        let mut info = MaybeUninit::uninit();
        success(ku_tensor_get_info(empty.0, info.as_mut_ptr()));
        let info = info.assume_init();
        assert_eq!(info.primitive_type, KU_PRIMITIVE_F32);
        assert_eq!(info.ndim, 1);
        assert_eq!(std::slice::from_raw_parts(info.shape, 1), [0]);
        assert_eq!(std::slice::from_raw_parts(info.strides, 1), [1]);
        success(ku_tensor_get_data(empty.0, &mut data));
        assert!(data.is_null());
    }
}

#[test]
fn tensor_shape_overflow_and_zero_extents_preserve_statuses() {
    let cpu = Cpu::new(default_scheduler());
    let mut desc = ku_tensor_create_desc_t {
        device: cpu.device,
        primitive_type: KU_PRIMITIVE_F32,
        ndim: 0,
        shape: ptr::null(),
        strides: ptr::null(),
    };

    // SAFETY: all descriptor arrays remain live through each call. Successful
    // creations are empty or small and their owned handles use Object guards.
    unsafe {
        let mut raw = ptr::null_mut();
        for shape in [&[i64::MAX, 3][..], &[i64::MAX, 3, 0][..], &[-1, 2][..]] {
            desc.ndim = shape.len() as i32;
            desc.shape = shape.as_ptr();
            assert_eq!(
                ku_tensor_create(&desc, &mut raw),
                KU_STATUS_INVALID_ARGUMENT
            );
            assert!(raw.is_null());
        }

        // A leading zero prevents overflow even with large later dimensions.
        let empty_shape = [0_i64, i64::MAX, 3];
        let empty_strides = [1_i64, 0, 0];
        desc.ndim = empty_shape.len() as i32;
        desc.shape = empty_shape.as_ptr();
        for strides in [ptr::null(), empty_strides.as_ptr()] {
            desc.strides = strides;
            success(ku_tensor_create(&desc, &mut raw));
            let tensor = Object(raw);
            assert_eq!(tensor_size(&tensor), 0);
            let mut info = MaybeUninit::uninit();
            success(ku_tensor_get_info(tensor.0, info.as_mut_ptr()));
            let info = info.assume_init();
            assert_eq!(info.ndim, 3);
            assert_eq!(
                std::slice::from_raw_parts(info.shape, 3),
                [0, i64::MAX as usize, 3]
            );
            assert_eq!(std::slice::from_raw_parts(info.strides, 3), [1, 0, 0]);
            let mut data = ptr::dangling_mut::<c_void>();
            success(ku_tensor_get_data(tensor.0, &mut data));
            assert!(data.is_null());
        }

        let shape = [2_i64, 3];
        let strides = [1_i64, 2];
        desc.ndim = shape.len() as i32;
        desc.shape = shape.as_ptr();
        desc.strides = strides.as_ptr();
        success(ku_tensor_create(&desc, &mut raw));
        let tensor = Object(raw);
        assert_eq!(tensor_size(&tensor), 6);

        let incompatible_strides = [3_i64, 1];
        desc.strides = incompatible_strides.as_ptr();
        assert_eq!(ku_tensor_create(&desc, &mut raw), KU_STATUS_NOT_SUPPORTED);
        assert!(raw.is_null());
    }
}

#[test]
fn builtin_enumeration_lookup_and_invocation() {
    let cpu = Cpu::new(default_scheduler());
    // SAFETY: borrowed names/targets stay within the instance lifetime. The
    // frame's storage is valid through each call; results are owned references.
    unsafe {
        let mut count = 0;
        success(ku_instance_get_builtin_info(
            cpu.instance,
            ptr::null_mut(),
            0,
            &mut count,
        ));
        assert!(count > 1);
        let sentinel = ku_builtin_info_t {
            name: view(b"unchanged"),
        };
        let mut small = [sentinel];
        let mut required = 0;
        assert_eq!(
            ku_instance_get_builtin_info(cpu.instance, small.as_mut_ptr(), 1, &mut required),
            KU_STATUS_BUFFER_TOO_SMALL
        );
        assert_eq!(required, count);
        assert_eq!(small[0].name.data, sentinel.name.data);
        assert_eq!(small[0].name.size, sentinel.name.size);
        let mut names = vec![sentinel; count];
        success(ku_instance_get_builtin_info(
            cpu.instance,
            names.as_mut_ptr(),
            names.len(),
            &mut count,
        ));
        assert!(names.iter().any(|info| std::slice::from_raw_parts(
            info.name.data.cast::<u8>(),
            info.name.size
        ) == b"add"));
        let mut target = MaybeUninit::uninit();
        assert_eq!(
            ku_instance_get_proc_address(
                cpu.instance,
                view(b"does_not_exist"),
                target.as_mut_ptr()
            ),
            KU_STATUS_NOT_FOUND
        );
        success(ku_instance_get_proc_address(
            cpu.instance,
            view(b"add"),
            target.as_mut_ptr(),
        ));
        let target = target.assume_init();
        let mut ctx = ptr::null_mut();
        success(ku_frame_ctx_create(cpu.device, &mut ctx));
        let ctx = FrameContext(ctx);
        let mut args = Vec::new();
        for value in [20_i64, 22] {
            let scalar = ku_union_t {
                value: ku_union_value_t { i64: value },
                tag: KU_PRIMITIVE_I64,
            };
            let mut raw = ptr::null_mut();
            success(ku_scalar_create(&scalar, &mut raw));
            args.push(Object(raw));
        }
        let argv = [args[0].0, args[1].0];
        let mut results = [ptr::null_mut()];
        let mut frame = ku_frame_t {
            ctx: ctx.0,
            argv: argv.as_ptr(),
            pargn: argv.len(),
            knames: ptr::null(),
            kargn: 0,
            results: results.as_mut_ptr(),
            result_capacity: results.len(),
            result_count: 0,
        };
        let invoke = |frame: &mut ku_frame_t| match target.kind {
            KU_CALL_TARGET_FFI => (target.value.ffi)(frame),
            KU_CALL_TARGET_CALLABLE => {
                let closure = target.value.closure;
                (closure.ffi)(closure.capture, frame)
            }
            kind => panic!("unknown call target kind: {kind}"),
        };
        success(invoke(&mut frame));
        assert_eq!(frame.result_count, 1);
        let result = Object(results[0]);
        let mut value = MaybeUninit::uninit();
        success(ku_scalar_get_value(result.0, value.as_mut_ptr()));
        let value = value.assume_init();
        assert_eq!(value.tag, KU_PRIMITIVE_I64);
        assert_eq!(value.value.i64, 42);
        frame.pargn = 1;
        assert_eq!(invoke(&mut frame), KU_STATUS_INVALID_ARGUMENT);
        assert_eq!(frame.result_count, 0);
    }
}

unsafe extern "C" fn submit_inline(ctx: *mut c_void, task: ku_task_t) -> ku_status_t {
    // SAFETY: this test supplies a live AtomicUsize through instance teardown;
    // runtime tasks supply a non-null callable and their own live context.
    unsafe {
        let counter = &*ctx.cast::<std::sync::atomic::AtomicUsize>();
        counter.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        (task.run)(task.ctx);
    }
    KU_STATUS_SUCCESS
}

#[test]
fn custom_scheduler_executes_async_device_copy() {
    let counter = std::sync::atomic::AtomicUsize::new(0);
    let cpu = Cpu::new(ku_scheduler_t {
        ctx: ptr::from_ref(&counter).cast_mut().cast(),
        submit: (submit_inline as ku_scheduler_submit_t) as *const c_void,
    });
    let input = [7_u8, 11, 23, 42];
    let mut output = [0_u8; 4];
    // SAFETY: CPU device copies use accessible host storage; both buffers and
    // scheduler state stay live until the operation and instance are finished.
    unsafe {
        let mut stream = ptr::null_mut();
        success(ku_device_get_default_stream(cpu.device, &mut stream));
        let mut completion = ptr::null_mut();
        success(ku_device_copy_async(
            cpu.device,
            output.as_mut_ptr().cast(),
            input.as_ptr().cast(),
            input.len(),
            KU_MEMCPY_HOST_TO_DEVICE,
            stream,
            &mut completion,
        ));
        let completion = Completion(completion);
        success(ku_completion_wait(completion.0));
        assert_eq!(output, input);
        assert!(counter.load(std::sync::atomic::Ordering::Relaxed) > 0);
    }
}
