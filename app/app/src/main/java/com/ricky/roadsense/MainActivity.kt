package com.ricky.roadsense

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.ricky.roadsense.databinding.ActivityMainBinding
import com.mapbox.maps.Style
import com.mapbox.maps.plugin.locationcomponent.location
import com.mapbox.maps.plugin.viewport.viewport
import com.mapbox.maps.extension.style.expressions.dsl.generated.*
import android.content.Context
import com.mapbox.maps.extension.style.layers.generated.heatmapLayer
import com.mapbox.maps.extension.style.sources.generated.geoJsonSource
import com.mapbox.maps.extension.style.style
import com.mapbox.maps.extension.style.sources.getSourceAs
import com.mapbox.maps.extension.style.sources.generated.GeoJsonSource
import com.mapbox.geojson.Feature
import com.mapbox.geojson.FeatureCollection
import com.mapbox.geojson.Point
import org.json.JSONArray
import org.json.JSONException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val client = OkHttpClient()
    private var isDriveModeActive = false

    private val locationPermissionRequest = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val fineLocationGranted = permissions[Manifest.permission.ACCESS_FINE_LOCATION] ?: false
        val coarseLocationGranted = permissions[Manifest.permission.ACCESS_COARSE_LOCATION] ?: false
        if (fineLocationGranted || coarseLocationGranted) {
            enableLocationTracking()
        } else {
            Toast.makeText(this, "Location permission denied", Toast.LENGTH_SHORT).show()
        }
    }

    private fun checkLocationPermissionAndEnable() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
            ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED) {
            enableLocationTracking()
        } else {
            locationPermissionRequest.launch(arrayOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            ))
        }
    }

    private fun enableLocationTracking() {
        binding.mapView.location.updateSettings {
            enabled = true
        }
        binding.mapView.viewport.transitionTo(
            binding.mapView.viewport.makeFollowPuckViewportState()
        )

        val locationManager = getSystemService(Context.LOCATION_SERVICE) as android.location.LocationManager
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED) {
            locationManager.requestLocationUpdates(android.location.LocationManager.GPS_PROVIDER, 1000L, 1f, object : android.location.LocationListener {
                override fun onLocationChanged(location: android.location.Location) {
                    // Log the raw data so we can see if the GPS is actually sending speed
                    android.util.Log.d("SPEED_TEST", "Raw GPS Speed (m/s): ${location.speed}, Has Speed: ${location.hasSpeed()}")

                    // Convert to KM/H (Ensure it doesn't drop below 0)
                    val speedKmh = if (location.hasSpeed()) (location.speed * 3.6).toInt() else 0

                    // Force the UI update onto the Main Thread
                    runOnUiThread {
                        binding.tvCurrentSpeed.text = "$speedKmh"
                    }

                    var closestDistance = Float.MAX_VALUE
                    for (hazard in HazardRepository.hazardList) {
                        val dist = location.distanceTo(hazard)
                        if (dist < closestDistance) {
                            closestDistance = dist
                        }
                    }

                    if (closestDistance != Float.MAX_VALUE && HazardRepository.hazardList.isNotEmpty()) {
                        runOnUiThread {
                            binding.tvNextHazard.text = "NEXT HAZARD: ${closestDistance.toInt()}M"
                        }
                    } else {
                        runOnUiThread {
                            binding.tvNextHazard.text = "NEXT HAZARD: --M"
                        }
                    }
                }
            })
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.mapView.mapboxMap.loadStyle(
            style(Style.DARK) {
                +geoJsonSource("hazard-source") {
                    data("https://roadsense-app-v2.pages.dev/api/hazards")
                }
                +heatmapLayer("heatmap-layer", "hazard-source") {
                    heatmapIntensity(
                        interpolate {
                            linear()
                            zoom()
                            stop(0.0, 1.0)
                            stop(15.0, 3.0)
                        }
                    )
                    heatmapColor(
                        interpolate {
                            linear()
                            heatmapDensity()
                            stop(0.0) { rgba(0.0, 0.0, 0.0, 0.0) }
                            stop(0.5) { rgb(255.0, 215.0, 64.0) } // Yellow
                            stop(1.0) { rgb(255.0, 115.0, 81.0) } // Red
                        }
                    )
                    heatmapRadius(
                        interpolate {
                            linear()
                            zoom()
                            stop(0.0, 2.0)
                            stop(15.0, 20.0)
                        }
                    )
                    heatmapOpacity(0.8)
                }
            }
        )

        setupUI()
        fetchHazards()
        checkLocationPermissionAndEnable()
    }

    private fun setupUI() {
        binding.myLocationFab.setOnClickListener {
            checkLocationPermissionAndEnable()
        }

        binding.driveModeButton.setOnClickListener {
            isDriveModeActive = !isDriveModeActive
            if (isDriveModeActive) {
                binding.driveModeButton.setColorFilter(resources.getColor(android.R.color.holo_orange_dark, theme))
                binding.driveModeText.text = "DRIVE MODE: ON"
                val intent = Intent(this, LocationService::class.java)
                ContextCompat.startForegroundService(this, intent)
                Toast.makeText(this, "Drive Mode: ON", Toast.LENGTH_SHORT).show()
            } else {
                binding.driveModeButton.clearColorFilter()
                binding.driveModeText.text = "DRIVE MODE: OFF"
                val intent = Intent(this, LocationService::class.java)
                stopService(intent)
                Toast.makeText(this, "Drive Mode: OFF", Toast.LENGTH_SHORT).show()
            }
        }

    }

    private fun fetchHazards() {
        val prefs = getSharedPreferences("RoadSensePrefs", MODE_PRIVATE)

        // Instant load from cache
        val cachedJson = prefs.getString("cached_hazards", null)
        if (!cachedJson.isNullOrEmpty()) {
            android.util.Log.d("HAZARD_CACHE", "Loading cached hazard data")
            parseAndLoadHazards(cachedJson)
        }

        // Background sync from network
        lifecycleScope.launch {
            val freshJson = withContext(Dispatchers.IO) {
                try {
                    val request = Request.Builder()
                        .url("https://roadsense-app-v2.pages.dev/api/hazards")
                        .build()
                    client.newCall(request).execute().use { response ->
                        if (response.isSuccessful) {
                            val responseData = response.body?.string() ?: ""
                            android.util.Log.d("HAZARD_FETCH", "HTTP ${response.code} - Data: $responseData")
                            responseData
                        } else null
                    }
                } catch (e: Exception) {
                    android.util.Log.w("HAZARD_FETCH", "Offline mode: Using cached hazard data.", e)
                    null
                }
            }

            if (!freshJson.isNullOrEmpty()) {
                // Save to cache
                prefs.edit().putString("cached_hazards", freshJson).apply()
                // Parse and update map + repository
                parseAndLoadHazards(freshJson)
            }
        }
    }

    private fun parseAndLoadHazards(jsonString: String) {
        lifecycleScope.launch {
            withContext(Dispatchers.Default) {
                try {
                    val jsonArray = JSONArray(jsonString)
                    val features = mutableListOf<Feature>()
                    val tempLocations = mutableListOf<android.location.Location>()

                    for (i in 0 until jsonArray.length()) {
                        val obj = jsonArray.getJSONObject(i)
                        val lat = if (obj.has("lat")) obj.getDouble("lat") else obj.optDouble("latitude")
                        val lng = if (obj.has("lng")) obj.getDouble("lng") else obj.optDouble("longitude")
                        val point = Point.fromLngLat(lng, lat)
                        features.add(Feature.fromGeometry(point))

                        val loc = android.location.Location("hazard")
                        loc.latitude = lat
                        loc.longitude = lng
                        tempLocations.add(loc)
                    }

                    HazardRepository.hazardList = tempLocations
                    val featureCollection = FeatureCollection.fromFeatures(features)

                    withContext(Dispatchers.Main) {
                        binding.mapView.mapboxMap.style?.let { style ->
                            val source = style.getSourceAs<GeoJsonSource>("hazard-source")
                            source?.data(featureCollection.toJson())
                        }
                    }
                } catch (e: JSONException) {
                    android.util.Log.e("HAZARD_FETCH", "JSON Parsing Error", e)
                } catch (e: Exception) {
                    android.util.Log.e("HAZARD_FETCH", "Data Parsing Error", e)
                }
            }
        }
    }

    override fun onStart() {
        super.onStart()
        // binding.mapView.onStart() // Optional in v11
    }

    override fun onStop() {
        super.onStop()
        // binding.mapView.onStop() // Optional in v11
    }

    override fun onDestroy() {
        super.onDestroy()
        // binding.mapView.onDestroy() // Optional in v11
    }
}
