//! JNI bridge used by the Android/Compose UI.
//!
//! This module is the ABI boundary between Kotlin and the native runtime:
//! - proot process lifecycle (spawn)
//! - rootfs acquisition (download/extract)
//! - PTY stdin (write) and raw output relay to Java (Termux TerminalEmulator)
//!
//! Guideline: keep these functions thin and deterministic. Complex policy should live in
//! the Rust modules and be tested there.

use jni::objects::{JByteArray, JObject, JString, JValue};
use jni::sys::{jboolean, jint};
use jni::JNIEnv;
use std::io::Write;
use std::path::PathBuf;
use std::thread;

use crate::android::rootfs_fetch;
use crate::jni_context;

/// Initialize native layer with app paths.
///
/// Contract:
/// - Must be called before any other `NativeBridge.*` JNI method.
/// - Safe to call again on activity recreation; it updates paths but preserves PTY/stdin handles.
///
/// `external_storage_dir`: optional path (e.g. `/storage/emulated/0`). When set, the proot
/// environment will bind it into the guest as `/android` and `/root/Android`.
#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_init(
    mut env: JNIEnv,
    _: JObject,
    data_dir: JString,
    cache_dir: JString,
    native_library_dir: JString,
    external_storage_dir: jni::sys::jstring,
) -> jboolean {
    android_logger::init_once(
        android_logger::Config::default()
            .with_max_level(log::LevelFilter::Debug)
            .with_tag("trierarch"),
    );
    std::panic::set_hook(Box::new(|info| {
        log::error!("trierarch: panic: {:?}", info);
    }));

    let data_dir: String = env
        .get_string(&data_dir)
        .map(|s| s.into())
        .unwrap_or_default();
    let cache_dir: String = env
        .get_string(&cache_dir)
        .map(|s| s.into())
        .unwrap_or_default();
    let native_library_dir: String = env
        .get_string(&native_library_dir)
        .map(|s| s.into())
        .unwrap_or_default();

    let external_storage_path: Option<PathBuf> = if external_storage_dir.is_null() {
        None
    } else {
        let jstr = unsafe { JString::from_raw(external_storage_dir) };
        env.get_string(&jstr)
            .ok()
            .map(|s| PathBuf::from(String::from(s)))
    };

    match jni_context::init(
        PathBuf::from(data_dir),
        PathBuf::from(cache_dir),
        PathBuf::from(native_library_dir),
        external_storage_path,
    ) {
        Ok(()) => {}
        Err(e) => {
            log::error!("NativeBridge.init failed: {:?}", e);
            return 0;
        }
    }

    if let Err(e) = jni_context::init_pty_output_jni(&mut env) {
        log::error!("init_pty_output_jni: {:?}", e);
        return 0;
    }

    1
}

/// Returns 1 if Arch rootfs exists, 0 otherwise.
#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_hasRootfs(_env: JNIEnv, _: JObject) -> jboolean {
    jni_context::has_rootfs() as jboolean
}

/// Download and install the Arch rootfs.
///
/// Threading:
/// - Runs on a Rust background thread (caller blocks waiting for completion).
/// - Calls `callback.onProgress(pct, msg)` from that background thread via JNI attach.
#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_downloadRootfs(
    env: JNIEnv,
    _: JObject,
    callback: JObject,
) -> jboolean {
    let vm = env.get_java_vm().expect("get JavaVM");
    let callback_global = env
        .new_global_ref(callback)
        .expect("global ref for callback");

    let result = thread::spawn(move || {
        let progress_fn = Box::new(move |pct: u32, msg: &str| {
            let mut env = vm.attach_current_thread().expect("attach thread");
            let callback = callback_global.as_obj();
            let msg_j = env.new_string(msg).expect("new string");
            let msg_obj: JObject = msg_j.into();
            let args = [JValue::Int(pct as jint), JValue::Object(&msg_obj)];
            let _ = env.call_method(callback, "onProgress", "(ILjava/lang/String;)V", &args);
        });
        rootfs_fetch::ensure_arch_rootfs_with_progress(Some(progress_fn))
    })
    .join();

    match result {
        Ok(Ok(())) => 1,
        Ok(Err(e)) => {
            log::error!("downloadRootfs failed: {:?}", e);
            0
        }
        Err(e) => {
            log::error!("downloadRootfs thread panic: {:?}", e);
            0
        }
    }
}

/// Spawn the interactive proot shell under a PTY (idempotent). Pass the same rows/columns as the Termux terminal grid.
#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_spawnProot(
    _env: JNIEnv,
    _: JObject,
    rows: jint,
    cols: jint,
) -> jboolean {
    let r = rows.max(1).min(i32::from(u16::MAX)) as u16;
    let c = cols.max(1).min(i32::from(u16::MAX)) as u16;
    match jni_context::spawn_proot(r, c) {
        Ok(()) => 1,
        Err(e) => {
            log::error!("spawnProot failed: {:?}", e);
            0
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_isProotSpawned(_env: JNIEnv, _: JObject) -> jboolean {
    jni_context::is_proot_spawned() as jboolean
}

/// Update kernel PTY window size seen by the proot shell (TIOCSWINSIZE).
#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_setPtyWindowSize(
    _env: JNIEnv,
    _: JObject,
    rows: jint,
    cols: jint,
) {
    let r = rows.max(1).min(i32::from(u16::MAX)) as u16;
    let c = cols.max(1).min(i32::from(u16::MAX)) as u16;
    if let Err(e) = jni_context::set_pty_window_size(r, c) {
        log::warn!("setPtyWindowSize: {:?}", e);
    }
}

/// Write raw bytes to the PTY stdin of the proot shell.
#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_writeInput(env: JNIEnv, _: JObject, bytes: JByteArray) {
    if let Ok(mut stdin_guard) = jni_context::stdin().lock() {
        if let Some(ref mut stdin) = *stdin_guard {
            if let Ok(arr) = env.convert_byte_array(&bytes) {
                let _ = stdin.write_all(&arr);
                let _ = stdin.flush();
            }
        }
    }
}
