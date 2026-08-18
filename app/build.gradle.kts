plugins {
    alias(libs.plugins.android.application)
}

val nativeProjectDirectory = rootProject.projectDir.parentFile.resolve("trierarch-packages/native")
val nativeJniLibsDirectory = layout.buildDirectory.dir("generated/jniLibs")
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

val buildNativeArm64 by tasks.registering(Exec::class) {
    group = "build"
    description = "Builds Trierarch's Rust PTY host for Android arm64."
    workingDir(nativeProjectDirectory)
    commandLine(
        "cargo", "ndk", "-t", "arm64-v8a", "-P", "24",
        "-o", nativeJniLibsDirectory.get().asFile.absolutePath,
        "build", "--release",
    )
    inputs.dir(nativeProjectDirectory.resolve("src"))
    inputs.file(nativeProjectDirectory.resolve("Cargo.toml"))
    inputs.file(nativeProjectDirectory.resolve("Cargo.lock"))
    outputs.dir(nativeJniLibsDirectory)
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
            prootJniLibsDirectory.get().asFile,
            x11JniLibsDirectory.get().asFile,
        )
        // SourceSet accepts a concrete directory, not Gradle's Provider wrapper.
        // `preBuild` explicitly depends on packageX11Assets below.
        getByName("main").assets.srcDir(x11AssetsDirectory.get().asFile)
    }
    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
}

tasks.named("preBuild").configure {
    dependsOn(buildNativeArm64, packageProotArm64, packageX11LibraryArm64, packageX11Assets)
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
