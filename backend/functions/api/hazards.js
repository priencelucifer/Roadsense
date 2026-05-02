// Vanilla Geohash Encoder
function encodeGeohash(lat, lon, precision = 7) {
  const BITS = [16, 8, 4, 2, 1];
  const BASE32 = "0123456789bcdefghjkmnpqrstuvwxyz";
  let isEven = true;
  let latMin = -90, latMax = 90;
  let lonMin = -180, lonMax = 180;
  let geohash = "";
  let bit = 0;
  let ch = 0;

  while (geohash.length < precision) {
    let mid;
    if (isEven) {
      mid = (lonMin + lonMax) / 2;
      if (lon > mid) {
        ch |= BITS[bit];
        lonMin = mid;
      } else {
        lonMax = mid;
      }
    } else {
      mid = (latMin + latMax) / 2;
      if (lat > mid) {
        ch |= BITS[bit];
        latMin = mid;
      } else {
        latMax = mid;
      }
    }
    isEven = !isEven;
    if (bit < 4) {
      bit++;
    } else {
      geohash += BASE32[ch];
      bit = 0;
      ch = 0;
    }
  }
  return geohash;
}

export async function onRequestGet(context) {
  const db = context.env.DB;
  const { results } = await db.prepare("SELECT * FROM hazards ORDER BY created_at DESC").all();
  return new Response(JSON.stringify(results), {
    headers: { "Content-Type": "application/json" },
  });
}

export async function onRequestPost(context) {
  try {
    const body = await context.request.json();
    const { lat, lng, type, severity, secret_key } = body;

    // 1. Validate Secret Key
    if (secret_key !== context.env.API_SECRET_KEY) {
      return new Response(JSON.stringify({ error: "Unauthorized" }), { status: 401 });
    }

    // 2. Validate Inputs
    if (!lat || !lng || !type || !severity) {
      return new Response(JSON.stringify({ error: "Missing required fields" }), { status: 400 });
    }

    // 3. Encode Geohash
    const geohash = encodeGeohash(lat, lng);

    // 4. Insert into D1
    const db = context.env.DB;
    await db.prepare("INSERT INTO hazards (lat, lng, type, severity, geohash) VALUES (?, ?, ?, ?, ?)")
      .bind(lat, lng, type, severity, geohash)
      .run();

    return new Response(JSON.stringify({ success: true, geohash }), {
      headers: { "Content-Type": "application/json" },
    });
  } catch (err) {
    return new Response(JSON.stringify({ error: err.message }), { status: 500 });
  }
}
