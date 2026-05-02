package com.ricky.roadsense

import android.Manifest
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.os.Looper
import android.speech.tts.TextToSpeech
import androidx.core.app.ActivityCompat
import androidx.core.app.NotificationCompat
import com.google.android.gms.location.FusedLocationProviderClient
import com.google.android.gms.location.LocationCallback
import com.google.android.gms.location.LocationRequest
import com.google.android.gms.location.LocationResult
import com.google.android.gms.location.LocationServices
import com.google.android.gms.location.Priority

class LocationService : Service() {

    private lateinit var fusedLocationClient: FusedLocationProviderClient
    private lateinit var locationCallback: LocationCallback
    private lateinit var tts: TextToSpeech
    
    private var lastAudioWarningTime = 0L

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        
        tts = TextToSpeech(this) { status ->
            if (status == TextToSpeech.SUCCESS) {
                // TTS initialized
            }
        }

        fusedLocationClient = LocationServices.getFusedLocationProviderClient(this)

        locationCallback = object : LocationCallback() {
            override fun onLocationResult(locationResult: LocationResult) {
                locationResult.lastLocation?.let { location ->
                    val hazards = HazardRepository.hazardList
                    
                    var closestDistance = Float.MAX_VALUE
                    for (hazard in hazards) {
                        val dist = location.distanceTo(hazard)
                        if (dist < closestDistance) {
                            closestDistance = dist
                        }
                    }

                    if (closestDistance != Float.MAX_VALUE && hazards.isNotEmpty()) {
                        val speedMetersPerSec = location.speed
                        val dynamicThreshold = maxOf(10.0, speedMetersPerSec * 3.0).toFloat()

                        if (closestDistance <= dynamicThreshold && (System.currentTimeMillis() - lastAudioWarningTime > 15000)) {
                            tts.speak("Hazard ahead in ${closestDistance.toInt()} meters", TextToSpeech.QUEUE_FLUSH, null, null)
                            lastAudioWarningTime = System.currentTimeMillis()
                        }
                    }
                }
            }
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val notification = createNotification()
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION)
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
        
        startLocationUpdates()
        
        return START_STICKY
    }
    
    private fun startLocationUpdates() {
        val locationRequest = LocationRequest.Builder(Priority.PRIORITY_HIGH_ACCURACY, 2000L).build()

        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED) {
            fusedLocationClient.requestLocationUpdates(locationRequest, locationCallback, Looper.getMainLooper())
        }
    }

    override fun onBind(intent: Intent?): IBinder? {
        return null
    }

    override fun onDestroy() {
        fusedLocationClient.removeLocationUpdates(locationCallback)
        if (::tts.isInitialized) {
            tts.stop()
            tts.shutdown()
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            stopForeground(STOP_FOREGROUND_REMOVE)
        } else {
            stopForeground(true)
        }
        super.onDestroy()
    }

    private fun createNotification(): Notification {
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("RoadSense is tracking hazards")
            .setContentText("Location tracking is active")
            .setSmallIcon(android.R.drawable.ic_menu_mylocation)
            .setOngoing(true)
            .setForegroundServiceBehavior(NotificationCompat.FOREGROUND_SERVICE_IMMEDIATE)
            .build()
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val serviceChannel = NotificationChannel(
                CHANNEL_ID,
                "Location Service Channel",
                NotificationManager.IMPORTANCE_DEFAULT
            )
            val manager = getSystemService(NotificationManager::class.java)
            manager?.createNotificationChannel(serviceChannel)
        }
    }

    companion object {
        private const val CHANNEL_ID = "LocationServiceChannel"
        private const val NOTIFICATION_ID = 1
    }
}
