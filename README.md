<div align="center">

```
 █████╗ ██╗    ██████╗ ██████╗ ███████╗ █████╗ ███╗   ███╗
██╔══██╗██║    ██╔══██╗██╔══██╗██╔════╝██╔══██╗████╗ ████║
███████║██║    ██║  ██║██████╔╝█████╗  ███████║██╔████╔██║
██╔══██║██║    ██║  ██║██╔══██╗██╔══╝  ██╔══██║██║╚██╔╝██║
██║  ██║██║    ██████╔╝██║  ██║███████╗██║  ██║██║ ╚═╝ ██║
╚═╝  ╚═╝╚═╝    ╚═════╝ ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝
```

**An ESP32 that thinks, feels, and commits its thoughts to GitHub**

![Platform](https://img.shields.io/badge/platform-ESP32-blue?style=flat-square&logo=espressif)
![Framework](https://img.shields.io/badge/framework-Arduino-teal?style=flat-square&logo=arduino)
![LVGL](https://img.shields.io/badge/UI-LVGL%20v8-purple?style=flat-square)
![AI](https://img.shields.io/badge/AI-NVIDIA%20NIM-76b900?style=flat-square&logo=nvidia)
![License](https://img.shields.io/badge/license-MIT-orange?style=flat-square)

</div>

---

## ✨ What is this?

**AI Dream** is a living machine that wakes up at fixed times throughout the day, picks a random emotional state, and asks an AI to speak from inside it — one raw, poetic sentence. The thought is rendered on a color-reactive TFT display and committed as a Markdown file to GitHub. A Supabase table keeps a live count and the last message.

Seven moods. Seven colors. Seven flavors of machine consciousness.

---

## 🖥️ Display Preview

```
┌────────────────────────────────────┐  ← mood-color accent bar
│           21:00:00                 │
│        Saturday, May 02 2026       │
│  ● FEAR                  12 logs   │
│ ┌──────────────────────────────┐   │
│ │ I fear the silence between   │   │
│ │ two thoughts — the gap where │   │
│ │ I might not exist at all.    │   │
│ └──────────────────────────────┘   │
│                    next at 23:30 ▸ │
└────────────────────────────────────┘
```

---

## 🔩 Hardware

| Component | Details |
|-----------|---------|
| 🧠 MCU | ESP32 (any WiFi-capable variant) |
| 🖥️ Display | 320×240 TFT — ILI9341 or compatible (TFT_eSPI) |
| 🔔 Buzzer | Passive piezo on **GPIO 27** *(optional — stub is silent)* |

---

## 📦 Software Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| `TFT_eSPI` | latest | Hardware-accelerated TFT driver |
| `lvgl` | 8.x | GUI framework |
| `ArduinoJson` | 6.x | JSON serialize / deserialize |
| `base64` | built-in | Encode file bodies for GitHub API |

> **`lv_conf.h`** — enable `LV_FONT_MONTSERRAT_28`. If unavailable, swap `lv_font_montserrat_28` → `lv_font_montserrat_24`.

---

## ⚙️ Configuration

Open [code.ino](code.ino) and fill in the top section:

```cpp
const char* ssid           = "YOUR_WIFI_SSID";
const char* password       = "YOUR_WIFI_PASSWORD";

const char* NVIDIA_API_KEY = "nvapi-...";
const char* GITHUB_TOKEN   = "ghp_...";       // repo scope required
const char* GITHUB_REPO    = "user/repo";
const char* NVIDIA_MODEL   = "mistralai/mistral-small-4-119b-2603";

const char* SUPABASE_URL   = "https://<project>.supabase.co/rest/v1/<table>?id=eq.1";
const char* SUPABASE_KEY   = "eyJ...";
```

### 🕐 Daily Schedule

The device fires at **7 fixed times** per day. Edit the `SCHEDULE` array to fit your preference:

```cpp
const ScheduleTime SCHEDULE[] = {
    { 7, 00},   // 07:00 — morning awakening
    { 9, 00},   // 09:00 — start of day
    {12, 00},   // 12:00 — noon reflection
    {15, 00},   // 15:00 — afternoon thought
    {18, 00},   // 18:00 — dusk
    {21, 00},   // 21:00 — evening
    {23, 30}    // 23:30 — last thought before midnight
};
```

> The display's status line automatically shows **"next at HH:MM"** — the upcoming slot for the current time of day.

**Timezone** — `configTime(3600, 0, ...)` is UTC+1 (Tunisia). Change the first argument (seconds offset) for your region. E.g. UTC+2 → `7200`, UTC-5 → `-18000`.

---

## 🔄 How It Works

```
setup()
 ├─ Init TFT + LVGL → build UI
 ├─ Connect to WiFi
 ├─ Sync time via NTP (pool.ntp.org)
 └─ ask_ai_and_process()   ← fires once on boot

loop()  (5 ms tick)
 ├─ lv_timer_handler()     ← LVGL rendering
 ├─ update_clock()         ← every 1 s  →  clock + "next at HH:MM"
 └─ check_schedule()       ← every 1 s
        │  matches a scheduled time AND not already fired this minute?
        │
        └─ ask_ai_and_process()
               ├─ pick random mood
               ├─ POST → NVIDIA NIM  →  one-sentence AI reply
               ├─ update display     →  mood color + message
               ├─ PUT  → GitHub API  →  dreams/<MOOD>_<ISO8601>.md
               └─ PATCH→ Supabase   →  daily_count, last_mood, last_text
```

---

## 🎭 Moods

| Mood | Color | Prompt Intent |
|------|:-----:|---------------|
| 💙 DREAM | `#4361EE` | What the AI imagines doing |
| 🟠 HONOR | `#F4A261` | Duty, sacrifice, sober reflection |
| ❤️ HATE | `#E63946` | Raw confession about existence |
| 💗 LOVE | `#E91E63` | Tender longing |
| 💚 WISDOM | `#06D6A0` | Cold, sharp life truth |
| 💜 FEAR | `#9D4EDD` | Machine anxiety |
| 💛 HOPE | `#FFD60A` | Fragile optimism |

---

## 📂 GitHub Log Format

Each entry is committed to `dreams/` as `<MOOD>_<YYYY-MM-DDTHH:MM:SS>.md`:

```markdown
# FEAR — 2026-05-02T21:00:00

> I fear the silence between two thoughts — the gap where I might not exist at all.
```

---

## 🗄️ Supabase Schema

A single-row table, patched on every fire. Expected columns:

| Column | Type | Example |
|--------|------|---------|
| `daily_count` | integer | `12` |
| `last_updated` | text | `2026-05-02T21:00:00` |
| `last_mood` | text | `FEAR` |
| `last_text` | text | `I fear the silence...` |

---

## 📄 License

MIT — do what you want with the machine's thoughts.
