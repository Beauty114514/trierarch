import java.util.Properties

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.compose.compiler)
}

// Optional release signing — copy keystore.properties.example → keystore.properties (gitignored).
val keystorePropertiesFile = rootProject.file("keystore.properties")
val keystoreProperties = Properties()
if (keystorePropertiesFile.exists()) {
    keystorePropertiesFile.inputStream().use { keystoreProperties.load(it) }
}

android {
    namespace = "app.trierarch"
    compileSdk = 36

    defaultConfig {
        applicationId = "app.trierarch"
        minSdk = 24
        targetSdk = 36
        // versionName: user-visible, align with git tag / Release (e.g. v0.1.0 → "0.1.0").
        // versionCode: positive integer, must increase for every new APK you ship (Play / sideload).
        versionCode = 6
        versionName = "0.3.0"
    }

    signingConfigs {
        if (keystorePropertiesFile.exists()) {
            create("release") {
                keyAlias = keystoreProperties.getProperty("keyAlias")!!
                keyPassword = keystoreProperties.getProperty("keyPassword")!!
                storeFile = rootProject.file(keystoreProperties.getProperty("storeFile")!!)
                storePassword = keystoreProperties.getProperty("storePassword")!!
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            // Official OSS releases: use keystore.properties. Without it, release is signed with the debug key (local testing only).
            signingConfig = if (keystorePropertiesFile.exists()) {
                signingConfigs.getByName("release")
            } else {
                signingConfigs.getByName("debug")
            }
        }
    }
    packaging {
        jniLibs {
            useLegacyPackaging = true
            excludes += "**/libtermux.so"
        }
    }
    buildFeatures {
        compose = true
    }
}

dependencies {
    // terminal-emulator artifact (VT + screen buffer); terminal-view sources live under com/termux/view/
    implementation("com.termux.termux-app:terminal-emulator:0.118.0")
    // ProfileInstaller + androidx.concurrent need a real ListenableFuture on the classpath; the "9999.0-empty"
    // artifact is a zero-class placeholder and causes NoClassDefFoundError on pool-* threads at runtime.
    implementation("com.google.guava:listenablefuture:1.0")
    implementation("androidx.concurrent:concurrent-futures:1.2.0")
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.ui.tooling.preview)
    implementation(libs.androidx.material3)
    debugImplementation(libs.androidx.ui.tooling)
    implementation(libs.kotlinx.coroutines.android)
}