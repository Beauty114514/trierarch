//! JNI bridge for Compose UI. Exposes proot, rootfs, and PTY I/O to Kotlin.

use jni::objects::{JByteArray, JObject, JString, JValue};
use jni::sys::{jboolean, jint, jobject, jstring};
use jni::JNIEnv;
use std::io::Write;
use std::path::PathBuf;
use std::thread;

use crate::android::rootfs_fetch;
use crate::jni_context;

/// Initialize native layer with app paths. Call from Kotlin before any other native method.
/// external_storage_dir: optional path (e.g. from Environment.getExternalStorageDirectory()); when set, proot binds it as /android and /root/Android like LocalDesktop.
#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_init(
    mut env: JNIEnv,
    _: JObject,
    data_dir: JString,
    cache_dir: JString,
    native_library_dir: JString,
    external_storage_dir: jstring,
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
        Ok(()) => 1,
        Err(e) => {
            log::error!("NativeBridge.init failed: {:?}", e);
            0
        }
    }
}

/// Returns 1 if Arch rootfs exists, 0 otherwise.
#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_hasRootfs(
    _env: JNIEnv,
    _: JObject,
) -> jboolean {
    jni_context::has_rootfs() as jboolean
}

/// Download Arch rootfs. Calls callback.onProgress(pct, msg) on a background thread.
/// Callback must implement interface with method: void onProgress(int pct, String msg)
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

/// Spawn proot shell. Returns 1 on success, 0 on failure.
#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_spawnProot(
    _env: JNIEnv,
    _: JObject,
) -> jboolean {
    match jni_context::spawn_proot() {
        Ok(()) => 1,
        Err(e) => {
            log::error!("spawnProot failed: {:?}", e);
            if let Ok(mut lines) = jni_context::lines().lock() {
                lines.clear();
                lines.push_back(format!("Error: {}", e));
            }
            0
        }
    }
}

/// Get terminal output lines. Returns Java ArrayList<String>.
#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_getLines(
    mut env: JNIEnv,
    _: JObject,
) -> jobject {
    let lines = jni_context::lines()
        .lock()
        .map(|l| l.clone())
        .unwrap_or_default();
    let list = env
        .new_object(
            "java/util/ArrayList",
            "(I)V",
            &[JValue::Int(lines.len() as jint)],
        )
        .unwrap_or_else(|_| {
            env.new_object("java/util/ArrayList", "()V", &[])
                .expect("ArrayList")
        });
    for line in lines {
        let jstr = env.new_string(&line).expect("new string");
        let jstr_obj: JObject = jstr.into();
        let _ = env.call_method(
            &list,
            "add",
            "(Ljava/lang/Object;)Z",
            &[JValue::Object(&jstr_obj)],
        );
    }
    list.into_raw()
}

/// Current line buffer (prompt + echoed input) without trailing newline.
#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_getPartialLine(
    env: JNIEnv,
    _: JObject,
) -> jstring {
    let partial = jni_context::partial_line()
        .lock()
        .map(|p| p.clone())
        .unwrap_or_default();
    env.new_string(&partial)
        .map(|s| s.into_raw())
        .unwrap_or(std::ptr::null_mut())
}

/// Write bytes to PTY stdin.
#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_writeInput(
    env: JNIEnv,
    _: JObject,
    bytes: JByteArray,
) {
    if let Ok(mut stdin_guard) = jni_context::stdin().lock() {
        if let Some(ref mut stdin) = *stdin_guard {
            if let Ok(arr) = env.convert_byte_array(&bytes) {
                let _ = stdin.write_all(&arr);
                let _ = stdin.flush();
            }
        }
    }
}
