package com.ricky.roadsense

import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.ricky.roadsense.databinding.ActivitySignupBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject

class SignupActivity : AppCompatActivity() {

    private lateinit var binding: ActivitySignupBinding
    private val client = OkHttpClient()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivitySignupBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.signupButton.setOnClickListener {
            val username = binding.usernameEdit.text.toString()
            val email = binding.emailEdit.text.toString()
            val password = binding.passwordEdit.text.toString()

            if (username.isNotEmpty() && email.isNotEmpty() && password.isNotEmpty()) {
                signup(username, email, password)
            } else {
                Toast.makeText(this, "Please fill all fields", Toast.LENGTH_SHORT).show()
            }
        }

        binding.loginLink.setOnClickListener {
            finish()
        }
    }

    private fun signup(username: String, email: String, password: String) {
        lifecycleScope.launch {
            val success = withContext(Dispatchers.IO) {
                try {
                    val json = JSONObject().apply {
                        put("name", username)
                        put("email", email)
                        put("password", password)
                    }
                    val body = json.toString().toRequestBody("application/json".toMediaType())
                    val request = Request.Builder()
                        .url("https://roadsense-app-v2.pages.dev/api/signup")
                        .post(body)
                        .build()

                    client.newCall(request).execute().use { response ->
                        if (!response.isSuccessful) {
                            val errorBody = response.body?.string() ?: "No error body provided"
                            android.util.Log.e("CLOUDFLARE_ERROR", "HTTP Code: ${response.code} - Reason: $errorBody")
                        }
                        response.isSuccessful
                    }
                } catch (e: Exception) {
                    false
                }
            }

            if (success) {
                startActivity(Intent(this@SignupActivity, MainActivity::class.java))
                finishAffinity()
            } else {
                Toast.makeText(this@SignupActivity, "Signup failed", Toast.LENGTH_SHORT).show()
            }
        }
    }
}
