package com.ricky.roadsense

import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.ricky.roadsense.databinding.ActivityLoginBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject

class LoginActivity : AppCompatActivity() {

    private lateinit var binding: ActivityLoginBinding
    private val client = OkHttpClient()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Auto-login: skip login screen if already authenticated
        val prefs = getSharedPreferences("RoadSensePrefs", MODE_PRIVATE)
        if (prefs.getBoolean("isLoggedIn", false)) {
            startActivity(Intent(this, MainActivity::class.java))
            finish()
            return
        }

        binding = ActivityLoginBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.loginButton.setOnClickListener {
            val email = binding.emailEdit.text.toString()
            val password = binding.passwordEdit.text.toString()

            if (email.isNotEmpty() && password.isNotEmpty()) {
                login(email, password)
            } else {
                Toast.makeText(this, "Please fill all fields", Toast.LENGTH_SHORT).show()
            }
        }

        binding.signupLink.setOnClickListener {
            startActivity(Intent(this, SignupActivity::class.java))
        }
    }

    private fun login(email: String, password: String) {
        lifecycleScope.launch {
            val success = withContext(Dispatchers.IO) {
                try {
                    val json = JSONObject().apply {
                        put("email", email)
                        put("password", password)
                    }
                    val body = json.toString().toRequestBody("application/json".toMediaType())
                    val request = Request.Builder()
                        .url("https://roadsense-app-v2.pages.dev/api/login")
                        .post(body)
                        .build()

                    client.newCall(request).execute().use { response ->
                        response.isSuccessful
                    }
                } catch (e: Exception) {
                    false
                }
            }

            if (success) {
                // Save session flag for persistent auto-login
                getSharedPreferences("RoadSensePrefs", MODE_PRIVATE)
                    .edit().putBoolean("isLoggedIn", true).apply()
                startActivity(Intent(this@LoginActivity, MainActivity::class.java))
                finish()
            } else {
                Toast.makeText(this@LoginActivity, "Login failed", Toast.LENGTH_SHORT).show()
            }
        }
    }
}
