# RoadSense

Crowd-sourced road hazard reporting for Indian cities (initially Guwahati and Delhi). Hazards (potholes, speed breakers, etc.) are detected by an ESP32 IoT device or reported by Android users, stored in a Cloudflare D1 database, and visualized as a live heatmap on both the web dashboard and the Android app.

This repo combines two previously separate projects:

| Subdirectory | Original repo | Stack |
|---|---|---|
| [`backend/`](./backend) | `Roadsense` | Cloudflare Pages + Functions + D1 (SQLite), HTML/JS frontend, Mapbox GL JS |
| [`app/`](./app) | `Roadsense_Application` | Android (Kotlin), Mapbox Maps SDK v11, OkHttp |

## Architecture

```
         ┌───────────────┐                ┌───────────────┐
         │  ESP32 device │                │  Android app  │
         │ (test_esp32.js│                │     (app/)    │
         │   simulator)  │                │               │
         └───────┬───────┘                └───────┬───────┘
                 │  POST /api/hazards             │
                 │  (secret_key auth)             │ POST /api/login
                 │                                │ POST /api/signup
                 ▼                                │ GET  /api/hazards
         ┌──────────────────────────────────────────────────┐
         │  Cloudflare Pages Functions  (backend/functions) │
         │      + D1 database (schema.sql)                  │
         └──────────────────────────┬───────────────────────┘
                                    │
                                    ▼
                           ┌────────────────┐
                           │  Web dashboard │
                           │  (backend/     │
                           │   public/)     │
                           └────────────────┘
```

## Setup

You will need your own Mapbox account and a Cloudflare account.

### 1. Backend (`backend/`)

Prerequisites: Node.js, [Wrangler CLI](https://developers.cloudflare.com/workers/wrangler/install-and-update/).

1. Create a Cloudflare D1 database and update `database_id` in `backend/wrangler.toml`.
2. Apply the schema:
   ```bash
   cd backend
   wrangler d1 execute roadsense-db-v2 --file=./schema.sql
   wrangler d1 execute roadsense-db-v2 --file=./add_delhi.sql   # optional seed
   ```
3. Set the API secret used by the ESP32 device:
   ```bash
   wrangler pages secret put API_SECRET_KEY
   ```
4. Edit `backend/public/index.html` and replace `YOUR_MAPBOX_PUBLIC_TOKEN_HERE` with your **public** Mapbox token (`pk.…`).
5. Deploy:
   ```bash
   wrangler pages deploy public
   ```

### 2. ESP32 simulator (`backend/test_esp32.js`)

```bash
cd backend
API_SECRET_KEY=your-secret-here node test_esp32.js
```

### 3. Android app (`app/`)

Prerequisites: Android Studio, JDK 17, Android SDK 34.

1. Create `app/local.properties`:
   ```properties
   sdk.dir=/path/to/your/Android/Sdk
   MAPBOX_DOWNLOADS_TOKEN=sk.your_mapbox_secret_token_here
   ```
   The `MAPBOX_DOWNLOADS_TOKEN` is required to download the Mapbox Maven artifacts.
2. Open `app/app/src/main/res/values/strings.xml` and replace `YOUR_MAPBOX_SECRET_TOKEN_HERE` with your Mapbox **secret** (`sk.…`) token used at runtime.
3. If your backend is deployed somewhere other than `roadsense-app-v2.pages.dev`, update the URLs in `app/app/src/main/java/com/ricky/roadsense/`:
   - `MainActivity.kt` (hazards endpoints)
   - `LoginActivity.kt` (login endpoint)
   - `SignupActivity.kt` (signup endpoint)
4. Build and run from Android Studio.

## API

| Method | Path | Auth | Purpose |
|---|---|---|---|
| GET | `/api/hazards` | none | List all hazards (used by web map and Android app) |
| POST | `/api/hazards` | `secret_key` in body | Report a new hazard (ESP32) |
| POST | `/api/login` | password in body | User login |
| POST | `/api/signup` | password in body | User signup |
| GET | `/api/search` | none | Location/place search |

## Repository layout

```
.
├── backend/                  # Cloudflare Pages + Functions + D1 + web dashboard
│   ├── functions/api/        # Pages Functions (hazards, search, login, signup)
│   ├── public/               # Static dashboard (Mapbox GL JS)
│   ├── schema.sql            # D1 schema
│   ├── add_delhi.sql         # Optional seed data
│   ├── test_esp32.js         # ESP32 device simulator
│   └── wrangler.toml         # Cloudflare config
└── app/                      # Android app (Kotlin)
    ├── app/                  # Module
    ├── build.gradle.kts
    └── settings.gradle.kts
```

## Notes

- All credentials in this repo are placeholders. Real tokens must be supplied via the steps above and never committed.
- This is a public, sanitized merge of two previously private repositories. The original commit history is not preserved.
