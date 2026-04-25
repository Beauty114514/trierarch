//! JNI entrypoints for `NativeBridge` (init, PTY, rootfs, renderer mode).

use jni::objects::{JByteArray, JObject, JString, JValue};
use jni::sys::{jboolean, jint};
use jni::JNIEnv;
use std::path::PathBuf;
use std::thread;

use crate::android::pulse_host;
use crate::android::rootfs_fetch;
use crate::android::virgl_host;
use crate::jni_context;

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
            .with_max_level(log::LevelFilter::Warn)
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

    pulse_host::spawn_host_pulseaudio_if_present();

    1
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_stopVirglHost(_env: JNIEnv, _: JObject) {
    virgl_host::stop_if_running();
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_startVirglHostIfPossible(
    _env: JNIEnv,
    _: JObject,
) {
    virgl_host::start_if_possible();
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_hasArchRootfs(
    _env: JNIEnv,
    _: JObject,
) -> jboolean {
    jni_context::has_arch_rootfs() as jboolean
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_hasDebianRootfs(
    _env: JNIEnv,
    _: JObject,
) -> jboolean {
    match crate::android::application_context::debian_rootfs_dir() {
        Ok(root) => crate::android::application_context::has_rootfs(&root) as jboolean,
        Err(_) => 0,
    }
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_hasWineRootfs(
    _env: JNIEnv,
    _: JObject,
) -> jboolean {
    match crate::android::application_context::wine_rootfs_dir() {
        Ok(root) => crate::android::application_context::has_rootfs(&root) as jboolean,
        Err(_) => 0,
    }
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_downloadArchRootfs(
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

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_downloadDebianRootfs(
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
        rootfs_fetch::ensure_debian_rootfs_with_progress(Some(progress_fn))
    })
    .join();

    match result {
        Ok(Ok(())) => 1,
        Ok(Err(e)) => {
            log::error!("downloadDebianRootfs failed: {:?}", e);
            0
        }
        Err(e) => {
            log::error!("downloadDebianRootfs thread panic: {:?}", e);
            0
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_downloadWineRootfs(
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
        rootfs_fetch::ensure_wine_rootfs_with_progress(Some(progress_fn))
    })
    .join();

    match result {
        Ok(Ok(())) => 1,
        Ok(Err(e)) => {
            log::error!("downloadWineRootfs failed: {:?}", e);
            0
        }
        Err(e) => {
            log::error!("downloadWineRootfs thread panic: {:?}", e);
            0
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_spawnSession(
    _env: JNIEnv,
    _: JObject,
    session_id: jint,
    rows: jint,
    cols: jint,
) -> jboolean {
    let r = rows.max(1).min(i32::from(u16::MAX)) as u16;
    let c = cols.max(1).min(i32::from(u16::MAX)) as u16;
    match jni_context::spawn_session(session_id, r, c) {
        Ok(()) => 1,
        Err(e) => {
            log::error!("spawnSession failed: {:?}", e);
            0
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_spawnSessionInRootfs(
    _env: JNIEnv,
    _: JObject,
    session_id: jint,
    rows: jint,
    cols: jint,
    rootfs_kind: jint,
) -> jboolean {
    let r = rows.max(1).min(i32::from(u16::MAX)) as u16;
    let c = cols.max(1).min(i32::from(u16::MAX)) as u16;
    match jni_context::spawn_session_in_rootfs(session_id, r, c, rootfs_kind) {
        Ok(()) => 1,
        Err(e) => {
            log::error!("spawnSessionInRootfs failed: {:?}", e);
            0
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_closeSession(
    _env: JNIEnv,
    _: JObject,
    session_id: jint,
) {
    if let Err(e) = jni_context::close_session(session_id) {
        log::warn!("closeSession: {:?}", e);
    }
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_isSessionAlive(
    _env: JNIEnv,
    _: JObject,
    session_id: jint,
) -> jboolean {
    jni_context::is_session_alive(session_id) as jboolean
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_setPtyWindowSize(
    _env: JNIEnv,
    _: JObject,
    session_id: jint,
    rows: jint,
    cols: jint,
) {
    let r = rows.max(1).min(i32::from(u16::MAX)) as u16;
    let c = cols.max(1).min(i32::from(u16::MAX)) as u16;
    if let Err(e) = jni_context::set_pty_window_size(session_id, r, c) {
        log::warn!("setPtyWindowSize: {:?}", e);
    }
}

#[no_mangle]
pub extern "system" fn Java_app_trierarch_NativeBridge_writeInput(
    env: JNIEnv,
    _: JObject,
    session_id: jint,
    bytes: JByteArray,
) {
    let arr = match env.convert_byte_array(&bytes) {
        Ok(a) => a,
        Err(_) => return,
    };
    if let Err(e) = jni_context::write_input(session_id, &arr) {
        log::warn!("writeInput: {:?}", e);
    }
}
