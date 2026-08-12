#include <Wire.h>
#include <U8g2lib.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>

#define SCREEN_W 128
#define SCREEN_H 64

// ========== 用户配置区 ==========
const char* ssid = "YOUR_WIFI_SSID";           // 填写2.4G WiFi
const char* password = "YOUR_WIFI_PASSWORD";   // 输入WiFi 密码
const char* JUHE_KEY = "YOUR_JUHE_KEY";        // 填写聚合数据 Key
const char* STATUS_API = "https://your-domain.com/api/status-page/heartbeat/your-slug"; 

// 监控列表（最多建议 3 个，超出屏幕放不下）
const int MONITOR_COUNT = 3;
const char* monitorIds[MONITOR_COUNT]   = {"4", "3", "5"};       // Uptime Kuma 对应的监控项 ID（获取方式见 README）
const char* monitorNames[MONITOR_COUNT] = {"Blog1", "Blog2", "Blog3"}; // 显示在 OLED 屏幕上的名称（修改成你的监控项名字，仅限英文或数字）
// ================================

const unsigned long FETCH_INTERVAL = 120000UL;
unsigned long lastFetch = 0;
unsigned long lastDataFetch = 0;
const unsigned long DATA_VALID = 30000;

int monitorStatus[MONITOR_COUNT];
float monitorUptime[MONITOR_COUNT];

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

int currentPage = 0;
unsigned long lastSwitch = 0;
const unsigned long SWITCH_INTERVAL = 15000;
bool timeSynced = false;

// 汉字点阵：农、历、月
static const uint8_t glyph_nong[] = {
  0x80,0x00,0x80,0x00,0xfe,0x7f,0x82,0x40,0x81,0x20,0x40,0x00,0x40,0x10,0xa0,0x18,
  0x90,0x04,0x18,0x03,0x14,0x01,0x12,0x02,0x11,0x0c,0x50,0x70,0x30,0x20,0x10,0x00
};
static const uint8_t glyph_li[] = {
  0x00,0x10,0xfc,0x3f,0x84,0x00,0x84,0x00,0x84,0x00,0x84,0x20,0xfc,0x7f,0x84,0x20,
  0x84,0x20,0x84,0x20,0x84,0x20,0x44,0x20,0x42,0x20,0x22,0x22,0x11,0x14,0x08,0x08
};
static const uint8_t glyph_yue[] = {
  0x00,0x08,0xf0,0x1f,0x10,0x08,0x10,0x08,0x10,0x08,0xf0,0x0f,0x10,0x08,0x10,0x08,
  0x10,0x08,0xf0,0x0f,0x10,0x08,0x10,0x08,0x08,0x08,0x08,0x08,0x04,0x0a,0x02,0x04
};

char lunarMonth[8] = "--";
char lunarDay[8] = "--";

bool fetchLunarFromJuhe(int year, int month, int day, int* outMonth, int* outDay) {
    if (WiFi.status() != WL_CONNECTED) return false;
    char url[128];
    snprintf(url, sizeof(url), "http://apis.juhe.cn/birthEight/solarTolunar?key=%s&year=%d&month=%d&day=%d",
             JUHE_KEY, year, month, day);
    HTTPClient http;
    http.begin(url);
    int code = http.GET();
    if (code != 200) {
        http.end();
        return false;
    }
    String response = http.getString();
    http.end();
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, response);
    if (error) return false;
    
    int errCode = doc["error_code"] | -1;
    if (errCode != 0) return false;

    JsonObject result = doc["result"];
    int lunarM = result["month"] | 0;
    int lunarD = result["day"] | 0;
    if (lunarM > 0 && lunarD > 0) {
        *outMonth = lunarM;
        *outDay = lunarD;
        return true;
    }
    return false;
}

void updateLunarFromTime() {
    if (!timeSynced) {
        snprintf(lunarMonth, sizeof(lunarMonth), "--");
        snprintf(lunarDay, sizeof(lunarDay), "--");
        return;
    }
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    int year = tm_info->tm_year + 1900;
    int month = tm_info->tm_mon + 1;
    int day = tm_info->tm_mday;
    int m, d;
    if (fetchLunarFromJuhe(year, month, day, &m, &d)) {
        snprintf(lunarMonth, sizeof(lunarMonth), "%d", m);
        snprintf(lunarDay, sizeof(lunarDay), "%d", d);
        Serial.print("Lunar API Success: ");
        Serial.print(lunarMonth);
        Serial.print("/");
        Serial.println(lunarDay);
    } else {
        Serial.println("Lunar API Failed!");
    }
}

void drawChineseChar(int x, int y, const uint8_t* data) {
    u8g2.drawXBM(x, y, 16, 16, data);
}

void showStartupScreen() {
    u8g2.firstPage();
    do {
        u8g2.setFont(u8g2_font_ncenB14_tr);
        u8g2.setCursor(10, 30);
        u8g2.print("Uptime");
        u8g2.setCursor(20, 55);
        u8g2.print("Kuma");
    } while (u8g2.nextPage());
    delay(1500);
}

void oledPrint(String l1, String l2 = "") {
    u8g2.firstPage();
    do {
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.setCursor(0, 10);
        u8g2.print(l1);
        if (l2.length()) {
            u8g2.setCursor(0, 25);
            u8g2.print(l2);
        }
    } while (u8g2.nextPage());
}

String formatPercent(float value) {
    if (value < 0 || value > 1) return "--%";
    int percent = (int)(value * 1000 + 0.5);
    int intPart = percent / 10;
    int fracPart = percent % 10;
    char buf[10];
    snprintf(buf, sizeof(buf), "%d.%d%%", intPart, fracPart);
    return String(buf);
}

void drawSymbol(int x, int y, int status) {
    if (status == 1) {
        u8g2.drawLine(x, y + 3, x + 3, y + 6);
        u8g2.drawLine(x + 3, y + 6, x + 7, y);
    } else if (status == 2) {
        u8g2.drawLine(x, y, x + 7, y + 7);
        u8g2.drawLine(x, y + 7, x + 7, y);
    }
}

void drawStatusPage() {
    u8g2.firstPage();
    do {
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.setCursor(0, 10);
        u8g2.print("Uptime Kuma");
        u8g2.drawLine(0, 13, SCREEN_W, 13);

        int y = 20;
        for (int i = 0; i < MONITOR_COUNT; i++) {
            int status = monitorStatus[i];
            float uptime = monitorUptime[i];

            if (status == 3) {
                u8g2.setFont(u8g2_font_6x10_tf);
                u8g2.setCursor(0, y + 8);
                u8g2.print("?");
            } else {
                drawSymbol(0, y + 2, status);
            }

            u8g2.setFont(u8g2_font_6x10_tf);
            u8g2.setCursor(10, y + 8);
            u8g2.print(" ");
            u8g2.print(monitorNames[i]);
            u8g2.print(" ");
            u8g2.print(formatPercent(uptime));

            y += 16;
            if (y > SCREEN_H - 4) break;
        }
    } while (u8g2.nextPage());
}

void fetchData(bool force = false) {
    if (WiFi.status() != WL_CONNECTED) return;
    
    if (!force && lastDataFetch != 0 && (millis() - lastDataFetch < DATA_VALID)) {
        drawStatusPage();
        return;
    }

    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    HTTPClient https;
    https.begin(secureClient, STATUS_API);

    int code = https.GET();
    if (code == 200) {
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, https.getStream());
        if (!error) {
            JsonObject heartbeatObj = doc["heartbeatList"].as<JsonObject>();
            JsonObject uptimeObj = doc["uptimeList"].as<JsonObject>();
            if (!heartbeatObj.isNull()) {
                for (int i = 0; i < MONITOR_COUNT; i++) {
                    const char* id = monitorIds[i];
                    JsonArray heartbeats = heartbeatObj[id].as<JsonArray>();
                    int status = -1;
                    if (!heartbeats.isNull() && heartbeats.size() > 0) {
                        JsonObject latest = heartbeats[heartbeats.size() - 1];
                        status = latest["status"];
                    }
                    monitorStatus[i] = status;

                    float uptime = -1.0;
                    String key = String(id) + "_24";
                    if (!uptimeObj.isNull() && uptimeObj.containsKey(key)) {
                        uptime = uptimeObj[key].as<float>();
                    }
                    monitorUptime[i] = uptime;
                }
                lastDataFetch = millis();
                drawStatusPage();
                Serial.println("API OK");
            } else {
                oledPrint("No heartbeat");
            }
        } else {
            oledPrint("JSON Parse Err");
        }
    } else {
        oledPrint("HTTP Err: " + String(code));
    }
    https.end();
    secureClient.stop();
}

void drawSecondPage() {
    u8g2.firstPage();
    do {
        u8g2.setFont(u8g2_font_ncenB14_tr);
        u8g2.setCursor(0, 18);
        int h, m, s;
        if (timeSynced) {
            time_t now = time(nullptr);
            struct tm* tm_info = localtime(&now);
            h = tm_info->tm_hour;
            m = tm_info->tm_min;
            s = tm_info->tm_sec;
        } else {
            unsigned long seconds = millis() / 1000;
            h = (seconds / 3600) % 24;
            m = (seconds / 60) % 60;
            s = seconds % 60;
        }
        char timeStr[16];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", h, m, s);
        u8g2.print(timeStr);

        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.setCursor(0, 36);
        if (timeSynced) {
            time_t now = time(nullptr);
            struct tm* tm_info = localtime(&now);
            const char* weekdays[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
            char dateStr[32];
            snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d %s",
                    tm_info->tm_year + 1900,
                    tm_info->tm_mon + 1,
                    tm_info->tm_mday,
                    weekdays[tm_info->tm_wday]);
            u8g2.print(dateStr);
        } else {
            u8g2.print("NTP not synced");
        }

        int baseY = 58;
        int x = 0;
        drawChineseChar(x, baseY - 16, glyph_nong);
        x += 16;
        drawChineseChar(x, baseY - 16, glyph_li);
        x += 16;

        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.setCursor(x, baseY);
        u8g2.print(": ");
        x += 14;
        u8g2.setCursor(x, baseY);
        u8g2.print(lunarMonth);
        x += 10;
        drawChineseChar(x, baseY - 16, glyph_yue);
        x += 16;
        u8g2.setCursor(x, baseY);
        u8g2.print(lunarDay);

    } while (u8g2.nextPage());
}

void updateTimeOnly() {
    drawSecondPage();
}

void setup() {
    Serial.begin(115200);

    Wire.begin(4, 5);
    u8g2.begin();

    showStartupScreen();

    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30) {
        delay(500);
        Serial.print(".");
        wifiAttempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());
        u8g2.firstPage();
        do {
            u8g2.setFont(u8g2_font_ncenB10_tr);
            u8g2.setCursor(0, 20);
            u8g2.print("WiFi OK");
            u8g2.setCursor(0, 38);
            u8g2.print(WiFi.localIP().toString());
        } while (u8g2.nextPage());
        delay(1000);
    } else {
        Serial.println("\nWiFi FAILED! Continuing with no network.");
        oledPrint("WiFi FAIL", "No network");
    }
    delay(1000);

    Serial.print("Syncing time...");
    configTime(8 * 3600, 0, "ntp.ntsc.ac.cn", "ntp.aliyun.com", "time.windows.com");
    int retry = 0;
    const int MAX_RETRIES = 60;
    while (time(nullptr) < 8 * 3600 && retry < MAX_RETRIES) {
        delay(500);
        retry++;
        if (retry % 10 == 0) Serial.print(".");
    }
    
    if (time(nullptr) >= 8 * 3600) {
        timeSynced = true;
        Serial.println(" OK");
        updateLunarFromTime();
    } else {
        timeSynced = false;
        Serial.println(" FAIL");
        snprintf(lunarMonth, sizeof(lunarMonth), "--");
        snprintf(lunarDay, sizeof(lunarDay), "--");
    }

    fetchData(true);
    lastFetch = millis();
    lastSwitch = millis();
}

void loop() {
    unsigned long now = millis();

    // 零点跨夜刷新农历
    if (timeSynced) {
        static int lastDay = -1;
        time_t nowTime = time(nullptr);
        struct tm* t = localtime(&nowTime);
        
        if (lastDay == -1) {
            lastDay = t->tm_mday;
        } else if (t->tm_mday != lastDay) {
            lastDay = t->tm_mday;
            updateLunarFromTime();
        }
    }

    // 定时拉取 Uptime Kuma 数据
    if (now - lastFetch > FETCH_INTERVAL) {
        fetchData(true);
        lastFetch = now;
    }

    // 页面轮播
    if (now - lastSwitch > SWITCH_INTERVAL) {
        currentPage = (currentPage == 0) ? 1 : 0;
        lastSwitch = now;
        if (currentPage == 0) {
            drawStatusPage();
        } else {
            drawSecondPage();
        }
    }

    // 时钟页每秒刷新
    static unsigned long lastSecond = 0;
    if (currentPage == 1 && (now - lastSecond >= 1000)) {
        updateTimeOnly();
        lastSecond = now;
    }

    delay(100);
}