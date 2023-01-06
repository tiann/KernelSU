import com.android.build.api.dsl.ApplicationExtension
import com.android.build.gradle.BaseExtension
import org.jetbrains.kotlin.gradle.tasks.KotlinCompile
import org.jetbrains.kotlin.konan.properties.Properties
import java.io.ByteArrayOutputStream

plugins {
    id("com.android.application") apply false
    id("com.android.library") apply false
    kotlin("android") apply false
}

buildscript {
    repositories {
        google()
        mavenCentral()
    }
    dependencies {
        classpath(kotlin("gradle-plugin", version = "1.7.20"))
    }
}

val androidMinSdk = 26
val androidTargetSdk = 33
val androidCompileSdk = 33
val androidBuildToolsVersion = "33.0.1"
val androidSourceCompatibility = JavaVersion.VERSION_11
val androidTargetCompatibility = JavaVersion.VERSION_11
val managerVersionCode = getVersionCode()
val managerVersionName = getVersionName()

tasks.register<Delete>("clean") {
    delete(rootProject.buildDir)
}

fun getGitCommitCount(): Int {
    val out = ByteArrayOutputStream()
    exec {
        commandLine("git", "rev-list", "--count", "HEAD")
        standardOutput = out
    }
    return out.toString().trim().toInt()
}

fun getGitDescribe(): String {
    val out = ByteArrayOutputStream()
    exec {
        commandLine("git", "describe", "--tags", "--always")
        standardOutput = out
    }
    return out.toString().trim()
}

fun getVersionCode(): Int {
    val commitCount = getGitCommitCount()
    val major = 1
    return major * 10000 + commitCount + 200
}

fun getVersionName(): String {
    return getGitDescribe()
}

fun Project.configureBaseExtension() {
    extensions.findByType<BaseExtension>()?.run {
        compileSdkVersion(androidCompileSdk)
        buildToolsVersion = androidBuildToolsVersion

        defaultConfig {
            minSdk = androidMinSdk
            targetSdk = androidTargetSdk
            versionCode = managerVersionCode
            versionName = managerVersionName

            consumerProguardFiles("proguard-rules.pro")
        }

        val signFile = rootProject.file("sign.properties")
        val config = if (signFile.canRead()) {
            val prop = Properties()
            prop.load(signFile.inputStream())
            signingConfigs.create("config") {
                storeFile = file(prop.getProperty("KEYSTORE_FILE"))
                storePassword = prop.getProperty("KEYSTORE_PASSWORD")
                keyAlias = prop.getProperty("KEY_ALIAS")
                keyPassword = prop.getProperty("KEY_PASSWORD")
            }
        } else {
            signingConfigs["debug"]
        }

        buildTypes {
            all {
                signingConfig = config
            }

            named("release") {
                isMinifyEnabled = true
                proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            }
        }

        compileOptions {
            sourceCompatibility = androidSourceCompatibility
            targetCompatibility = androidTargetCompatibility
        }

        extensions.findByType<ApplicationExtension>()?.run {
            buildTypes {
                named("release") {
                    isShrinkResources = true
                }
            }
        }

        extensions.findByType<KotlinCompile>()?.run {
            kotlinOptions {
                jvmTarget = "11"
            }
        }
    }
}

subprojects {
    plugins.withId("com.android.application") {
        configureBaseExtension()
    }
    plugins.withId("com.android.library") {
        configureBaseExtension()
    }
}
