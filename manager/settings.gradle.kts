enableFeaturePreview("TYPESAFE_PROJECT_ACCESSORS")

pluginManagement {
    repositories {
        gradlePluginPortal()
        google()
        mavenCentral()
    }
    plugins {
        val agp = "7.3.1"
        val kotlin = "1.7.20"
        id("com.android.application") version agp
        id("com.android.library") version agp
        id("com.google.devtools.ksp") version "$kotlin-1.0.8"
        kotlin("android") version kotlin
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
        maven("https://jitpack.io")
    }
}

rootProject.name = "KernelSU"
include(":app")
