// FlutterTap manager app. by Eduardo Lopes
plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "com.eduardolopes.fluttertap"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.eduardolopes.fluttertap"
        // Android 10 through the current Android 17 (API 37) are supported.
        minSdk = 29
        targetSdk = 35
        versionCode = 1
        versionName = "1.0.0"
    }

    // Release signing credentials come from gradle.properties in the user's
    // GRADLE_USER_HOME (~/.gradle/gradle.properties), never from this repository:
    // the keystore and its password must not be committed. A clone without them
    // still builds -- `release` is simply left unsigned, which is the correct
    // behaviour for anyone who is not the release signer. See docs/BUILD.md.
    val storeFilePath = providers.gradleProperty("FLUTTERTAP_STORE_FILE").orNull
    val hasReleaseKeystore = storeFilePath != null && file(storeFilePath).exists()

    signingConfigs {
        if (hasReleaseKeystore) {
            create("release") {
                storeFile = file(storeFilePath!!)
                storePassword = providers.gradleProperty("FLUTTERTAP_STORE_PASSWORD").get()
                keyAlias = providers.gradleProperty("FLUTTERTAP_KEY_ALIAS").get()
                keyPassword = providers.gradleProperty("FLUTTERTAP_KEY_PASSWORD").get()
                // Only v3 actually ends up in the APK, and that is correct: v1
                // (JAR signing) matters below API 24 and v2 below API 28, so AGP
                // skips both for minSdk 29. Verified with
                // `apksigner verify --print-certs`: v3 true, v1/v2 false.
                enableV3Signing = true
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = if (hasReleaseKeystore) signingConfigs.getByName("release") else null
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        compose = true
    }
}

dependencies {
    implementation(platform("androidx.compose:compose-bom:2024.12.01"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-graphics")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.activity:activity-compose:1.9.3")
    implementation("androidx.core:core-ktx:1.15.0")
    implementation("androidx.appcompat:appcompat:1.7.0") // per-app language (AppCompatDelegate.setApplicationLocales)
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")

    // Root shell access to read/write /data/adb/fluttertap/config.json.
    implementation("com.github.topjohnwu.libsu:core:6.0.0")

    debugImplementation("androidx.compose.ui:ui-tooling")
}
