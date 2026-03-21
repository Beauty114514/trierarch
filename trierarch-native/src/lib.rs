// JNI library for Compose UI. Exposes proot, rootfs, PTY I/O via NativeBridge.

#[cfg(target_os = "android")]
mod android;

#[cfg(target_os = "android")]
mod jni_bridge;

#[cfg(target_os = "android")]
mod jni_context;
