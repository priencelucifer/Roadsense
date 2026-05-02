import java.util.Properties

pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

val localProperties = Properties().apply {
    val localPropertiesFile = File(settingsDir, "local.properties")
    if (localPropertiesFile.exists()) {
        localPropertiesFile.inputStream().use { load(it) }
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
        maven {
            url = uri("https://api.mapbox.com/downloads/v2/releases/maven")
            authentication {
                create<BasicAuthentication>("basic")
            }
            credentials {
                username = "mapbox"
                password = localProperties.getProperty("MAPBOX_DOWNLOADS_TOKEN") ?: ""
            }
        }
    }
}

rootProject.name = "RoadSense"
include(":app")
