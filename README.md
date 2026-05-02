# RoadSense

Crowd-sourced road hazard reporting for Indian cities. Hazards (potholes, speed breakers) are submitted by ESP32 IoT devices and Android users, stored in a Cloudflare D1 database at the edge, and visualized as a live heatmap on a web dashboard and an Android map.

> **Status:** active · **Coverage:** Guwahati, Delhi · **Stack:** Cloudflare Pages + D1 · Kotlin · Mapbox

---

## Repo layout

This is a sanitized merge of two previously private repositories.

| Path | What it is | Stack |
| --- | --- | --- |
| [`backend/`](./backend) | Edge API, web dashboard, ESP32 simulator | Cloudflare Pages · Pages Functions · D1 · Mapbox GL JS |
| [`app/`](./app) | Android client | Kotlin · Mapbox Maps SDK v11 · OkHttp · Coroutines |

## How it fits together

```
   ESP32 device                              Android app
        │                                         │
        │  POST /api/hazards (secret_key)         │  POST /api/login
        │                                         │  POST /api/signup
        │                                         │  GET  /api/hazards
        ▼                                         ▼
   ┌──────────────────────────────────────────────────────┐
   │  Cloudflare Pages Functions  (backend/functions/api) │
   │           + D1 database  (schema.sql)                │
   └──────────────────────────┬───────────────────────────┘
                              │
                              ▼
                     Web dashboard (backend/public)
```

---

## Setup

You'll need a [Mapbox](https://account.mapbox.com/) account (public + secret tokens) and a [Cloudflare](https://dash.cloudflare.com/) account.

### Backend

```bash
cd backend
npm i -g wrangler

# create the D1 database, then paste the database_id into wrangler.toml
wrangler d1 create roadsense-db-v2

# apply schema (and optional seed)
wrangler d1 execute roadsense-db-v2 --file=./schema.sql
wrangler d1 execute roadsense-db-v2 --file=./add_delhi.sql

# set the shared secret used by the ESP32 simulator
wrangler pages secret put API_SECRET_KEY

# replace YOUR_MAPBOX_PUBLIC_TOKEN_HERE in public/index.html with your pk.* token
wrangler pages deploy public
```

### ESP32 simulator

```bash
cd backend
API_SECRET_KEY=your-secret-here node test_esp32.js
```

### Android app

1. Create `app/local.properties` with your Mapbox **secret** download token:
   ```properties
   sdk.dir=/path/to/your/Android/Sdk
   MAPBOX_DOWNLOADS_TOKEN=sk.your_mapbox_secret_token_here
   ```
2. Open `app/app/src/main/res/values/strings.xml` and replace `YOUR_MAPBOX_SECRET_TOKEN_HERE` with your runtime Mapbox secret token.
3. If your backend is not at `roadsense-app-v2.pages.dev`, update the URLs in:
   - `app/app/src/main/java/com/ricky/roadsense/MainActivity.kt`
   - `app/app/src/main/java/com/ricky/roadsense/LoginActivity.kt`
   - `app/app/src/main/java/com/ricky/roadsense/SignupActivity.kt`
4. Open in Android Studio (JDK 17, Android SDK 34) and run.

---

## API

| Method | Path | Auth | Purpose |
| --- | --- | --- | --- |
| `GET` | `/api/hazards` | — | List hazards (web map + Android) |
| `POST` | `/api/hazards` | `secret_key` in body | Submit a new hazard (ESP32) |
| `POST` | `/api/login` | password in body | User login |
| `POST` | `/api/signup` | password in body | User signup |
| `GET` | `/api/search` | — | Location / place search |

---

## Tree

```
.
├── backend/                  Cloudflare Pages + Functions + D1 + dashboard
│   ├── functions/api/          hazards · search · login · signup
│   ├── public/                 static dashboard (Mapbox GL JS)
│   ├── schema.sql              D1 schema
│   ├── add_delhi.sql           optional seed data
│   ├── test_esp32.js           ESP32 device simulator
│   └── wrangler.toml           Cloudflare config
└── app/                      Android app (Kotlin)
    ├── app/                    module sources
    ├── build.gradle.kts
    └── settings.gradle.kts
```

## Notes

- Every credential in this repo is a placeholder. Supply your own; never commit real tokens.
- This is a sanitized merge — original commit history is not preserved.
- Android package: `com.ricky.roadsense` (rename if forking for production).
