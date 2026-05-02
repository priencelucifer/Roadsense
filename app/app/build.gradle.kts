plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.ricky.roadsense"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.ricky.roadsense"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        vectorDrawables {
            useSupportLibrary = true
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    kotlinOptions {
        jvmTarget = "1.8"
    }
    buildFeatures {
        viewBinding = true
    }
}

tasks.register("fixResourceName") {
    doLast {
        val resDir = file("src/main/res/drawable")
        val badFile = File(resDir, "hazard_marker.png.png")
        val goodFile = File(resDir, "hazard_marker.png")
        if (badFile.exists()) {
            if (badFile.renameTo(goodFile)) {
                println("Successfully renamed $badFile to $goodFile")
            } else {
                throw GradleException("Failed to rename $badFile")
            }
        } else {
            println("File $badFile does not exist, skipping.")
        }
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.11.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("com.google.android.gms:play-services-location:21.2.0")

    // Mapbox SDK v11
    implementation("com.mapbox.maps:android:11.2.0")

    // OkHttp for API calls
    implementation("com.squareup.okhttp3:okhttp:4.12.0")

    // Coroutines
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3")

    // Lifecycle
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.7.0")

    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
}
