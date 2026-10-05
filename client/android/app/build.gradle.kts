plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "com.deskhub.app"
    compileSdk = 37
    ndkVersion = (project.findProperty("androidNdkVersion") as String?)
        ?: System.getenv("ANDROID_NDK_VERSION")
        ?: "26.1.10909125"

    defaultConfig {
        applicationId = (project.findProperty("applicationId") as String?) ?: "com.manhpham.deskhub"
        minSdk = 26
        targetSdk = 36
        versionCode = (project.findProperty("versionCode") as String?)?.toInt() ?: 1
        versionName = (project.findProperty("versionName") as String?) ?: "0.1-dev"

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
            debugSymbolLevel = "FULL"
        }

        externalNativeBuild {
            cmake {
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

    if (System.getenv("KEYSTORE_FILE") != null) {
        signingConfigs {
            create("release") {
                storeFile = file(System.getenv("KEYSTORE_FILE"))
                storePassword = System.getenv("KEYSTORE_PASSWORD")
                keyAlias = System.getenv("KEY_ALIAS")
                keyPassword = System.getenv("KEY_PASSWORD")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"))
            signingConfig = signingConfigs.findByName("release")
        }
        debug {
            isJniDebuggable = true
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
    }
}

abstract class BundleThirdPartyNotices : DefaultTask() {
    @get:InputFiles
    @get:PathSensitive(PathSensitivity.NAME_ONLY)
    abstract val notices: ConfigurableFileCollection

    @get:InputFiles
    @get:PathSensitive(PathSensitivity.NAME_ONLY)
    abstract val licenses: ConfigurableFileCollection

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun bundle() {
        val out = outputDir.get().asFile
        out.deleteRecursively()
        val licenseDir = File(out, "licenses").apply { mkdirs() }
        notices.forEach { it.copyTo(File(out, it.name), overwrite = true) }
        licenses.forEach { it.copyTo(File(licenseDir, it.name), overwrite = true) }
    }
}

val deskhubRoot = rootProject.layout.projectDirectory.dir("../..")
val licensesShippedOnAndroid =
    listOf(
        "BSD-2-Clause-quiche",
        "BoringSSL",
        "Apache-2.0",
        "rust-crates",
        "BSD-3-Clause-opus",
        "android-libraries",
    )

val bundleThirdPartyNotices = tasks.register<BundleThirdPartyNotices>("bundleThirdPartyNotices") {
    notices.from(deskhubRoot.file("THIRD_PARTY_NOTICES.md"))
    licenses.from(licensesShippedOnAndroid.map { deskhubRoot.file("licenses/$it.txt") })
}

androidComponents {
    onVariants { variant ->
        variant.sources.assets?.addGeneratedSourceDirectory(bundleThirdPartyNotices, BundleThirdPartyNotices::outputDir)
    }
}
dependencies {
    implementation("androidx.core:core-ktx:1.19.1")

    implementation(platform("androidx.compose:compose-bom:2026.09.00"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-graphics")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.activity:activity-compose:1.13.0")

    implementation("androidx.camera:camera-camera2:1.4.2")
    implementation("androidx.camera:camera-lifecycle:1.4.2")
    implementation("androidx.camera:camera-view:1.4.2")
    implementation("com.google.zxing:core:3.5.4")
}
