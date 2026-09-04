plugins {
    alias(libs.plugins.android.application)
}

val nativeProjectDirectory = rootProject.projectDir.parentFile.resolve("trierarch-packages/native")
val nativeJniLibsDirectory = layout.buildDirectory.dir("generated/jniLibs")
val rootfsCommandJniLibsDirectory = layout.buildDirectory.dir("generated/rootfsCommandJniLibs")
// Runtime comparison: consume the independently maintained PRoot package.
// Its source revision and Android build recipe are recorded with the package.
val prootRuntimeDirectory = rootProject.projectDir.parentFile.resolve(
    "trierarch-packages/proot/dist/android/arm64-v8a",
)
val prootJniLibsDirectory = layout.buildDirectory.dir("generated/prootJniLibs")
// Lorie is built and versioned by the independent X11-host package.
val x11RuntimeDirectory = rootProject.projectDir.parentFile.resolve(
    "trierarch-packages/x11-host/dist/android/arm64-v8a",
)
val x11JniLibsDirectory = layout.buildDirectory.dir("generated/x11JniLibs")
val x11AssetsDirectory = layout.buildDirectory.dir("generated/x11Assets")
val waylandRuntimeDirectory = rootProject.projectDir.parentFile.resolve(
    "trierarch-packages/wayland-host/dist/android/arm64-v8a",
)
val waylandJniLibsDirectory = layout.buildDirectory.dir("generated/waylandJniLibs")
// VirGL is an executable host plus private shared libraries, so it is packaged
// as APK assets and extracted to the app's private files directory at runtime.
val virglRuntimeDirectory = rootProject.projectDir.parentFile.resolve(
    "trierarch-packages/virgl-host/dist/android/arm64-v8a",
)
val virglAssetsDirectory = layout.buildDirectory.dir("generated/virglAssets")
val guestCompatibilityProjectDirectory = rootProject.projectDir.parentFile.resolve("trierarch-packages/compat")
val guestCompatibilityAssetsDirectory = layout.buildDirectory.dir("generated/guestCompatibilityAssets")

val cleanNativeJniLibs by tasks.registering(Delete::class) {
    delete(nativeJniLibsDirectory)
}

val buildNativeArm64 by tasks.registering(Exec::class) {
    group = "build"
    description = "Builds Trierarch's Rust PTY host for Android arm64."
    workingDir(nativeProjectDirectory)
    commandLine(
        "cargo", "ndk", "-t", "arm64-v8a", "-P", "24",
        "-o", nativeJniLibsDirectory.get().asFile.absolutePath,
        "build", "--release", "--lib", "--bin", "trierarch-rootfs",
    )
    inputs.dir(nativeProjectDirectory.resolve("src"))
    inputs.file(nativeProjectDirectory.resolve("Cargo.toml"))
    inputs.file(nativeProjectDirectory.resolve("Cargo.lock"))
    outputs.dir(nativeJniLibsDirectory)
    dependsOn(cleanNativeJniLibs)
}

val packageRootfsCommandArm64 by tasks.registering(Copy::class) {
    group = "build"
    description = "Packages Trierarch's rootfs import command for Android arm64."
    dependsOn(buildNativeArm64)
    from(nativeProjectDirectory.resolve("target/aarch64-linux-android/release/trierarch-rootfs"))
    into(rootfsCommandJniLibsDirectory.map { it.dir("arm64-v8a") })
    rename { "libtrierarch-rootfs.so" }
}

val packageProotArm64 by tasks.registering(Sync::class) {
    group = "build"
    description = "Packages the Trierarch PRoot arm64 runtime."
    val prootOutputDirectory = prootRuntimeDirectory
    from(prootOutputDirectory.resolve("proot")) {
        into("arm64-v8a")
        rename { "libproot.so" }
    }
    from(prootOutputDirectory.resolve("loader")) {
        into("arm64-v8a")
        rename { "libproot_loader.so" }
    }
    into(prootJniLibsDirectory)
    inputs.files(
        prootOutputDirectory.resolve("proot"),
        prootOutputDirectory.resolve("loader"),
    )
    doFirst {
        check(prootOutputDirectory.resolve("proot").isFile) {
            "Missing PRoot executable. Build trierarch-packages/proot first."
        }
        check(prootOutputDirectory.resolve("loader").isFile) {
            "Missing PRoot loader. Build trierarch-packages/proot first."
        }
    }
}

val packageX11LibraryArm64 by tasks.registering(Sync::class) {
    group = "build"
    description = "Packages the Trierarch Lorie X11 arm64 library."
    val library = x11RuntimeDirectory.resolve("libXlorie.so")
    from(library) { into("arm64-v8a") }
    into(x11JniLibsDirectory)
    doFirst {
        check(library.isFile) {
            "Missing Lorie library. Build trierarch-packages/x11-host first."
        }
    }
}

val packageX11Assets by tasks.registering(Sync::class) {
    group = "build"
    description = "Packages the Trierarch Lorie XKB runtime asset."
    val xkb = x11RuntimeDirectory.resolve("assets/lorie_xkb_bundled.zip")
    from(xkb)
    into(x11AssetsDirectory)
    doFirst {
        check(xkb.isFile) {
            "Missing Lorie XKB runtime asset. Build trierarch-packages/x11-host first."
        }
    }
}

val packageWaylandArm64 by tasks.registering(Sync::class) {
    group = "build"
    description = "Packages the Trierarch Wayland host arm64 libraries."
    notCompatibleWithConfigurationCache("Wayland artifacts are copied from an external package workspace")
    from(waylandRuntimeDirectory) { into("arm64-v8a") }
    into(waylandJniLibsDirectory)
    doFirst {
        check(waylandRuntimeDirectory.resolve("libtrierarch-wayland-host.so").isFile) {
            "Missing Wayland host. Build trierarch-packages/wayland-host first."
        }
        check(waylandRuntimeDirectory.resolve("libwayland-server.so").isFile) {
            "Missing Wayland server library. Build trierarch-packages/wayland-host first."
        }
        check(waylandRuntimeDirectory.resolve("libffi.so").isFile) {
            "Missing libffi. Build trierarch-packages/wayland-host first."
        }
    }
}

val packageVirglAssets by tasks.registering(Sync::class) {
    group = "build"
    description = "Packages the Trierarch VirGL host arm64 runtime assets."
    notCompatibleWithConfigurationCache("VirGL artifacts are copied from an external package workspace")
    from(virglRuntimeDirectory) {
        include("virgl_test_server_android", "virgl_render_server", "libvirglrenderer.so", "libepoxy.so")
        into("virgl/arm64-v8a")
    }
    from(virglRuntimeDirectory.resolve("angle/vulkan")) {
        include("*.so")
        into("virgl/arm64-v8a/angle/vulkan")
    }
    into(virglAssetsDirectory)
    doFirst {
        listOf("virgl_test_server_android", "libvirglrenderer.so", "libepoxy.so").forEach { name ->
            check(virglRuntimeDirectory.resolve(name).isFile) {
                "Missing VirGL runtime file '$name'. Build trierarch-packages/virgl-host first."
            }
        }
        check(virglRuntimeDirectory.resolve("angle/vulkan/libEGL_angle.so").isFile) {
            "Missing VirGL ANGLE Vulkan runtime. Build trierarch-packages/virgl-host first."
        }
    }
}

val buildGuestCompatibilityArm64 by tasks.registering(Exec::class) {
    group = "build"
    description = "Builds Trierarch's Linux guest compatibility library for arm64."
    workingDir(guestCompatibilityProjectDirectory)
    environment("CC", "aarch64-linux-gnu-gcc")
    commandLine("bash", "scripts/build-linux.sh")
    inputs.dir(guestCompatibilityProjectDirectory.resolve("src"))
    inputs.file(guestCompatibilityProjectDirectory.resolve("scripts/build-linux.sh"))
    outputs.file(guestCompatibilityProjectDirectory.resolve("dist/linux/aarch64/libtrierarch-udev-compat.so"))
}

val packageGuestCompatibilityAssets by tasks.registering(Sync::class) {
    group = "build"
    description = "Packages the Linux guest compatibility library as an APK asset."
    dependsOn(buildGuestCompatibilityArm64)
    val library = guestCompatibilityProjectDirectory.resolve("dist/linux/aarch64/libtrierarch-udev-compat.so")
    from(library) { into("compat/arm64-v8a") }
    into(guestCompatibilityAssetsDirectory)
    doFirst {
        check(library.isFile) { "Missing guest compatibility library." }
    }
}

android {
    namespace = "app.trierarch"
    compileSdk {
        version = release(36) {
            minorApiLevel = 1
        }
    }

    defaultConfig {
        applicationId = "app.trierarch"
        minSdk = 24
        targetSdk = 28
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    buildTypes {
        release {
            optimization {
                enable = false
            }
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    buildFeatures {
        aidl = true
    }

    sourceSets {
        getByName("main").jniLibs.srcDirs(
            nativeJniLibsDirectory.get().asFile,
            rootfsCommandJniLibsDirectory.get().asFile,
            prootJniLibsDirectory.get().asFile,
            x11JniLibsDirectory.get().asFile,
            waylandJniLibsDirectory.get().asFile,
        )
        // SourceSet accepts a concrete directory, not Gradle's Provider wrapper.
        // `preBuild` explicitly depends on packageX11Assets below.
        getByName("main").assets.srcDirs(
            x11AssetsDirectory.get().asFile,
            virglAssetsDirectory.get().asFile,
            guestCompatibilityAssetsDirectory.get().asFile,
        )
    }
    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
}

tasks.named("preBuild").configure {
    dependsOn(
        buildNativeArm64,
        packageRootfsCommandArm64,
        packageProotArm64,
        packageX11LibraryArm64,
        packageX11Assets,
        packageWaylandArm64,
        packageVirglAssets,
        packageGuestCompatibilityAssets,
    )
}

dependencies {
    implementation(libs.androidx.activity.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.core.ktx)
    implementation(libs.material)
    implementation("androidx.annotation:annotation:1.9.0")
    implementation(libs.tomlj)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.junit)
}
