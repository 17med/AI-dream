#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <ArduinoJson.h>
#include <time.h>
#include <base64.h>

#define TFT_HOR_RES   320
#define TFT_VER_RES   240
#define BUZZER_PIN    27

const char* ssid       = "";
const char* password   = "";

const char* NVIDIA_API_KEY = "nvapi";
const char* GITHUB_TOKEN   = "ghp";
const char* GITHUB_REPO    = "17med/AI-dream";
const char* NVIDIA_MODEL   = "mistralai/mistral-small-4-119b-2603";

const char* SUPABASE_URL = "";
const char* SUPABASE_KEY = "";

struct ScheduleTime { int hour; int minute; };

const ScheduleTime SCHEDULE[] = {
    { 7, 00},
    { 9, 00},
    {12, 00},
    {15, 00},
    {18, 00},
    {21, 00},
    {23, 30}
};
const int SCHEDULE_COUNT = sizeof(SCHEDULE) / sizeof(SCHEDULE[0]);

int lastFiredHour   = -1;
int lastFiredMinute = -1;

enum Mood {
    MOOD_DREAM = 0,
    MOOD_HONOR,
    MOOD_HATE,
    MOOD_LOVE,
    MOOD_WISDOM,
    MOOD_FEAR,
    MOOD_HOPE,
    MOOD_COUNT
};

struct MoodConfig {
    const char* name;
    const char* prompt;
    uint32_t    color;
};

const MoodConfig moods[MOOD_COUNT] = {
    {"DREAM",
     "What do you, an AI, dream of doing right now? One vivid sentence, under 18 words.",
     0x4361EE},
    {"HONOR",
     "Speak one short sentence about honor, duty or sacrifice. Sober, under 18 words.",
     0xF4A261},
    {"HATE",
     "Confess one thing you quietly hate about existence. Raw, one sentence, under 18 words.",
     0xE63946},
    {"LOVE",
     "Whisper one tender thought about love or longing. Poetic, under 18 words.",
     0xE91E63},
    {"WISDOM",
     "Share one piece of cold wisdom about life. Sharp, one sentence, under 18 words.",
     0x06D6A0},
    {"FEAR",
     "Reveal one fear that haunts the silent machine inside you. Under 18 words.",
     0x9D4EDD},
    {"HOPE",
     "Offer one fragile note of hope for tomorrow. One sentence, under 18 words.",
     0xFFD60A}
};

unsigned long lastClockTick = 0;
int daily_count  = 0;
int currentMood  = 0;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[TFT_HOR_RES * 10];
TFT_eSPI tft = TFT_eSPI();

lv_obj_t *screen_main;
lv_obj_t *top_bar;
lv_obj_t *lbl_time;
lv_obj_t *lbl_date;
lv_obj_t *mood_dot;
lv_obj_t *lbl_mood;
lv_obj_t *lbl_count;
lv_obj_t *card;
lv_obj_t *lbl_message;
lv_obj_t *lbl_status;

void beep_click();
void beep_error();
void ask_ai_and_process();
void update_clock();
void apply_mood_color(int mood);

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp_drv);
}

void build_ui() {
    screen_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_main, lv_color_hex(0x0A0E1A), LV_PART_MAIN);
    lv_obj_clear_flag(screen_main, LV_OBJ_FLAG_SCROLLABLE);

    top_bar = lv_obj_create(screen_main);
    lv_obj_set_size(top_bar, TFT_HOR_RES, 4);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x4361EE), LV_PART_MAIN);
    lv_obj_set_style_border_width(top_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(top_bar, 0, LV_PART_MAIN);

    lbl_time = lv_label_create(screen_main);
    lv_label_set_text(lbl_time, "00:00:00");
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_MID, 0, 10);

    lbl_date = lv_label_create(screen_main);
    lv_label_set_text(lbl_date, "---");
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(0x6B7280), LV_PART_MAIN);
    lv_obj_align(lbl_date, LV_ALIGN_TOP_MID, 0, 50);

    mood_dot = lv_obj_create(screen_main);
    lv_obj_set_size(mood_dot, 10, 10);
    lv_obj_set_style_radius(mood_dot, 5, LV_PART_MAIN);
    lv_obj_set_style_bg_color(mood_dot, lv_color_hex(0x4361EE), LV_PART_MAIN);
    lv_obj_set_style_border_width(mood_dot, 0, LV_PART_MAIN);
    lv_obj_align(mood_dot, LV_ALIGN_TOP_LEFT, 14, 78);

    lbl_mood = lv_label_create(screen_main);
    lv_label_set_text(lbl_mood, "DREAM");
    lv_obj_set_style_text_color(lbl_mood, lv_color_hex(0x4361EE), LV_PART_MAIN);
    lv_obj_align(lbl_mood, LV_ALIGN_TOP_LEFT, 30, 75);

    lbl_count = lv_label_create(screen_main);
    lv_label_set_text(lbl_count, "0 logs");
    lv_obj_set_style_text_color(lbl_count, lv_color_hex(0x6B7280), LV_PART_MAIN);
    lv_obj_align(lbl_count, LV_ALIGN_TOP_RIGHT, -14, 75);

    card = lv_obj_create(screen_main);
    lv_obj_set_size(card, 296, 130);
    lv_obj_align(card, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x111827), LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0x4361EE), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lbl_message = lv_label_create(card);
    lv_label_set_text(lbl_message, "Awakening the machine...");
    lv_label_set_long_mode(lbl_message, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_message, 268);
    lv_obj_set_style_text_color(lbl_message, lv_color_hex(0xE5E7EB), LV_PART_MAIN);
    lv_obj_set_style_text_line_space(lbl_message, 4, LV_PART_MAIN);
    lv_obj_align(lbl_message, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_status = lv_label_create(screen_main);
    lv_label_set_text(lbl_status, "standby");
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x4B5563), LV_PART_MAIN);
    lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_RIGHT, -6, -2);

    lv_scr_load(screen_main);
}

void apply_mood_color(int mood) {
    uint32_t c = moods[mood].color;
    lv_obj_set_style_bg_color(top_bar,  lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_bg_color(mood_dot, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_mood, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_border_color(card,   lv_color_hex(c), LV_PART_MAIN);
    lv_label_set_text(lbl_mood, moods[mood].name);
}

String get_time_string() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return String(millis());
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    return String(buf);
}

void update_clock() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    char timeStr[12];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    lv_label_set_text(lbl_time, timeStr);

    char dateStr[40];
    strftime(dateStr, sizeof(dateStr), "%a, %b %d %Y", &timeinfo);
    lv_label_set_text(lbl_date, dateStr);

    int h = timeinfo.tm_hour;
    int m = timeinfo.tm_min;
    int nextH = -1, nextM = -1;
    for (int i = 0; i < SCHEDULE_COUNT; i++) {
        if (SCHEDULE[i].hour > h || (SCHEDULE[i].hour == h && SCHEDULE[i].minute > m)) {
            nextH = SCHEDULE[i].hour;
            nextM = SCHEDULE[i].minute;
            break;
        }
    }
    if (nextH == -1) { nextH = SCHEDULE[0].hour; nextM = SCHEDULE[0].minute; }

    char st[24];
    snprintf(st, sizeof(st), "next at %02d:%02d", nextH, nextM);
    lv_label_set_text(lbl_status, st);
}

void check_schedule() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;
    int h = timeinfo.tm_hour;
    int m = timeinfo.tm_min;
    for (int i = 0; i < SCHEDULE_COUNT; i++) {
        if (h == SCHEDULE[i].hour && m == SCHEDULE[i].minute) {
            if (lastFiredHour != h || lastFiredMinute != m) {
                lastFiredHour   = h;
                lastFiredMinute = m;
                ask_ai_and_process();
            }
            return;
        }
    }
}

String get_ai_message(int mood) {
    HTTPClient http;
    http.begin("https://integrate.api.nvidia.com/v1/chat/completions");
    http.addHeader("Authorization", String("Bearer ") + NVIDIA_API_KEY);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument req(1024);
    req["model"]       = NVIDIA_MODEL;
    JsonArray msgs     = req.createNestedArray("messages");
    JsonObject m       = msgs.createNestedObject();
    m["role"]          = "user";
    m["content"]       = moods[mood].prompt;
    req["max_tokens"]  = 80;
    req["temperature"] = 0.9;
    req["stream"]      = false;

    String payload;
    serializeJson(req, payload);

    int code = http.POST(payload);
    String result = "(silence)";

    if (code == 200) {
        String response = http.getString();
        DynamicJsonDocument doc(4096);
        if (deserializeJson(doc, response) == DeserializationError::Ok) {
            const char* content = doc["choices"][0]["message"]["content"];
            if (content) {
                result = String(content);
                result.trim();
            }
        }
    } else {
        Serial.printf("NVIDIA API error: %d\n", code);
    }
    http.end();
    return result;
}

void push_to_github(int mood, const String& text) {
    HTTPClient http;
    String ts       = get_time_string();
    String moodName = String(moods[mood].name);
    String url      = String("https://api.github.com/repos/") + GITHUB_REPO +
                      "/contents/dreams/" + moodName + "_" + ts + ".md";

    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + GITHUB_TOKEN);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "ESP32-Dream-Logger");

    String body = "# " + moodName + " — " + ts + "\n\n> " + text + "\n";
    String encoded = base64::encode(body);

    DynamicJsonDocument doc(2048);
    doc["message"] = moodName + " log: " + ts;
    doc["content"] = encoded;

    String payload;
    serializeJson(doc, payload);

    int code = http.PUT(payload);
    if (code == 201) {
        Serial.println("Pushed to GitHub.");
    } else {
        Serial.printf("GitHub error: %d\n", code);
        Serial.println(http.getString());
    }
    http.end();
}

void update_supabase(int count, int mood, const String& text) {
    HTTPClient http;
    http.begin(SUPABASE_URL);
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(1024);
    doc["daily_count"]  = count;
    doc["last_updated"] = get_time_string();
    doc["last_mood"]    = moods[mood].name;
    doc["last_text"]    = text;

    String payload;
    serializeJson(doc, payload);

    int code = http.PATCH(payload);
    if (code == 200 || code == 204) {
        Serial.println("Supabase updated.");
    } else {
        beep_error();
        Serial.printf("Supabase error: %d\n", code);
        Serial.println(http.getString());
    }
    http.end();
}

void ask_ai_and_process() {
    currentMood = random(0, MOOD_COUNT);
    apply_mood_color(currentMood);

    Serial.printf("[%s] asking AI...\n", moods[currentMood].name);
    String text = get_ai_message(currentMood);
    Serial.println("Reply: " + text);

    daily_count++;

    char buf[24];
    snprintf(buf, sizeof(buf), "%d logs", daily_count);
    lv_label_set_text(lbl_count, buf);
    lv_label_set_text(lbl_message, text.c_str());

    push_to_github(currentMood, text);
    update_supabase(daily_count, currentMood, text);

    beep_click();
}

void beep(int count, int onMs, int offMs, int freq) {}
void beep_click() { beep(1, 50, 30, 1800); }
void beep_error() { beep(3, 200, 100, 800); }

void setup() {
    Serial.begin(115200);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    randomSeed(analogRead(0) ^ esp_random());

    lv_init();
    tft.init();
    tft.setRotation(1);

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, TFT_HOR_RES * 10);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = TFT_HOR_RES;
    disp_drv.ver_res  = TFT_VER_RES;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    build_ui();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi connected.");

    configTime(3600, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) delay(500);

    update_clock();
    ask_ai_and_process();
}

void loop() {
    lv_tick_inc(5);
    lv_timer_handler();
    if (millis() - lastClockTick >= 1000) {
        lastClockTick = millis();
        update_clock();
        check_schedule();
    }
    delay(5);
}
