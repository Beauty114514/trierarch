//! `libtrierarch`: JNI (`NativeBridge`), proot PTY, rootfs, host services.

#[cfg(target_os = "android")]
mod android;

#[cfg(target_os = "android")]
mod jni_bridge;

#[cfg(target_os = "android")]
mod jni_context;
