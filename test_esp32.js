// ESP32 Simulator for RoadSense
const SECRET_KEY = "guwahati_tracker_v1";
const API_URL = "https://roadsense-dashboard.pages.dev/api/hazards";

// Guwahati Bounding Box
const LAT_RANGE = [26.10, 26.20];
const LNG_RANGE = [91.65, 91.85];

function getRandom(min, max) {
  return Math.random() * (max - min) + min;
}

async function sendHazard() {
  const data = {
    lat: getRandom(LAT_RANGE[0], LAT_RANGE[1]),
    lng: getRandom(LNG_RANGE[0], LNG_RANGE[1]),
    type: Math.random() > 0.5 ? 'pothole' : 'speed_breaker',
    severity: getRandom(1.5, 9.8).toFixed(1),
    secret_key: SECRET_KEY
  };

  try {
    const response = await fetch(API_URL, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data)
    });

    const result = await response.json();
    console.log(`[${new Date().toLocaleTimeString()}] Sent: ${data.type} @ ${data.lat.toFixed(4)},${data.lng.toFixed(4)} | Response:`, result);
  } catch (error) {
    console.error("Error sending data:", error.message);
  }
}

console.log("RoadSense ESP32 Simulator Started...");
console.log(`Sending data to ${API_URL} every 5 seconds.`);

setInterval(sendHazard, 5000);
sendHazard(); // Send first one immediately
