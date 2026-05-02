const URL = "https://roadsense-app-v2.pages.dev/api/hazards";
const SECRET_KEY = process.env.API_SECRET_KEY || "YOUR_API_SECRET_KEY_HERE";

const HAZARD_TYPES = ["pothole", "speed_breaker"];

// Guwahati Bounding Box
const GUWAHATI = {
  minLat: 26.11,
  maxLat: 26.20,
  minLng: 91.65,
  maxLng: 91.85
};

function getRandomInRange(min, max) {
  return Math.random() * (max - min) + min;
}

async function sendHazard() {
  const payload = {
    lat: getRandomInRange(GUWAHATI.minLat, GUWAHATI.maxLat),
    lng: getRandomInRange(GUWAHATI.minLng, GUWAHATI.maxLng),
    type: HAZARD_TYPES[Math.floor(Math.random() * HAZARD_TYPES.length)],
    severity: parseFloat(getRandomInRange(1, 10).toFixed(1)),
    secret_key: SECRET_KEY
  };

  try {
    const response = await fetch(URL, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
    
    const data = await response.json();
    console.log(`[${new Date().toLocaleTimeString()}] SENT: ${payload.type} | STATUS: ${response.status}`);
    console.log("RESPONSE:", JSON.stringify(data));
  } catch (error) {
    console.error("FAILED TO SEND:", error.message);
  }
}

console.log("ESP32 Simulator Started. Sending hazard every 5 seconds to roadsense-app-v2...");
setInterval(sendHazard, 5000);
sendHazard();
