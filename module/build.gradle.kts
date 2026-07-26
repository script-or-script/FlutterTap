// FlutterTap native Zygisk module. by Eduardo Lopes
plugins {
    id("com.android.library")
}

android {
    namespace = "com.eduardolopes.fluttertap.module"
    compileSdk = 35
    ndkVersion = "27.2.12479018"

    defaultConfig {
        // Android 10 (API 29) and up. Real signature-resolution logic only
        // covers arm64/x86_64 (see docs/ARCHITECTURE.md), matching the scope
        // of the original script it was ported from.
        minSdk = 29

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20", "-fno-rtti")
                arguments += listOf("-DANDROID_STL=c++_static")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
