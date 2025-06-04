#define VC_VERSION "2.3"
// Include Libraries
#include <Arduino.h>
#include <TFT_eSPI.h>           // TFT library
#include <ESPAsyncWiFiManager.h> // WiFi Manager for ESP8266
#include <ESPAsyncTCP.h>        // Asynchronous TCP library
#include <ESPAsyncWebServer.h>  // Asynchronous Web Server
#include <FS.h>                 // File System library
#include <LittleFS.h>           // LittleFS for ESP8266
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "translations.h"

using namespace VolcanoTranslations;

// Color Definitions
constexpr uint16_t  BLACK = 0x0000;
constexpr uint16_t  RED   = 0xF800;
constexpr uint16_t  GREEN = 0x07E0;
constexpr uint16_t  WHITE = 0xFFFF;

// Touchscreen Calibration Constants
uint16_t calData[5] = { 249, 3561, 372, 3509, 0 };

// Pin Definitions
constexpr uint8_t AIR_PIN = D2;
constexpr uint8_t POWER_PIN = D1;

#define WIFI_NAME "volcano-cyber"

// Timing and Calibration Constants
#define MIN_CALIBRATION_TIME 5000  // Time (in ms) the button must be held to enter calibration mode
#define AUTO_SHUTDOWN_TIME 1800000 // Auto shutdown time (30 minutes in milliseconds)
#define DEBOUNCE_TIME 1200         // Debounce time for touch inputs

// Display Refresh Period
#define LV_DISP_DEF_REFR_PERIOD 1

// Balloon Size Dividers
#define LITTLE_BALLOON 2
#define SHOT_BALLOON 4

#define TEXT_HEIGHT 8 // Height of text to be printed and scrolled
#define BOT_FIXED_AREA 0  // Number of lines in bottom fixed area (lines counted from bottom of screen)
#define TOP_FIXED_AREA 0  // Number of lines in top fixed area (lines counted from top of screen)

AsyncWiFiManagerParameter custom_mqtt_server("server", "MQTT Server", "", 40);
AsyncWiFiManagerParameter custom_mqtt_port("port", "MQTT Port", "", 6);
AsyncWiFiManagerParameter custom_mqtt_user("user", "MQTT Username", "", 32);
AsyncWiFiManagerParameter custom_mqtt_pass("pass", "MQTT Password", "", 32);
const char LANGUAGE_DROPDOWN[] =
    "<label for='lang'>Language</label><select id='lang' name='lang'>"
    "<option value='de'>de</option>"
    "<option value='en'>en</option>"
    "</select>";
AsyncWiFiManagerParameter custom_lang(LANGUAGE_DROPDOWN);

// MQTT Settings
boolean mqttActive = false; // MQTT is disabled by default
uint16_t mqtt_port = 0;
String mqtt_server = ""; // MQTT broker IP
String mqtt_user = ""; // MQTT broker username
String mqtt_password = ""; // MQTT broker password
#undef  MQTT_MAX_PACKET_SIZE // un-define max packet size
#define MQTT_MAX_PACKET_SIZE 1024  // fix for MQTT client dropping messages over 128B

struct MqttData {
    String mqttStatus = "connected";
    String status = "off";
    int currentTemp = 180;
    int setTemp = 180;
    int balloonTime = 0;
    int on = 0;
    int off = 0;
    int shot = 0;
    int little = 0;
    int big = 0;
} mqtt;

// Global Variables Reduction
struct DeviceStatus {
    bool powerStatus = false;
    bool airStatus = false;
    unsigned long lastAction = 0;
    unsigned long stopTime = 0;
    unsigned long startTime = 0;
    unsigned int startPin = 0;
    int seconds = 0;
    int touchCheckInterval = 2147483647;
    boolean calExit = false;
} device;

struct ToggleOperation {
    bool active = false;
    int pin = -1;
    unsigned long endTime = 0;
} toggleOp;

constexpr boolean DEBUG_SERIAL = false;

Preferences preferences;

unsigned int touch = 0;
unsigned int lastPos = 0;
unsigned int lastTime = 0;
unsigned long lastTouch = 0;
unsigned int calStart = 0;

uint16_t yStart = TOP_FIXED_AREA;
uint16_t yArea = 320 - TOP_FIXED_AREA - BOT_FIXED_AREA;
uint16_t yDraw = 320 - BOT_FIXED_AREA - TEXT_HEIGHT;
byte     pos[42];
uint16_t xPos = 0;

// Screen States Enumeration
enum Screen { MAIN, STOP, CALIBRATION };
Screen screen = MAIN;

// Library Instances
TFT_eSPI tft = TFT_eSPI();       // TFT instance
TFT_eSprite sprite = TFT_eSprite(&tft); // Sprite instance
AsyncWebServer server(80);      // Web server instance

WiFiClient mqttWiFiClient;
PubSubClient mqttClient(mqttWiFiClient);

String VolcanoTranslations::currentLang = "de";

// Functions
String t(const String& key) {
    if (translations.count(key) && translations.at(key).count(currentLang)) {
        return translations.at(key).at(currentLang);
    }
    return key; // fallback fallback
}

void loadLanguageSetting() {
    preferences.begin("settings", true); // read-only
    currentLang = preferences.getString("lang", "de");
    preferences.end();
}

void saveLanguageSetting(String lang) {
    preferences.begin("settings", false); // write
    preferences.putString("lang", lang);
    preferences.end();
}

void logToSerial(String message, boolean newLine=true) {
	if(DEBUG_SERIAL) {
		newLine ? Serial.println(message) : Serial.print(message);
	}
}
// Initialization Functions
void initSerial() {
	if(DEBUG_SERIAL) {
    	Serial.begin(74800); // Adjust baud rate as necessary
	}
}

void initPins() {
    pinMode(AIR_PIN, OUTPUT);
    pinMode(POWER_PIN, OUTPUT);
    digitalWrite(AIR_PIN, HIGH);
    digitalWrite(POWER_PIN, HIGH);
}

bool setMqttDataProperty(const String& property, const String& value) {
    if (property == "mqttStatus") {
        mqtt.mqttStatus = value;
    } else if (property == "status") {
        mqtt.status = value;
    } else if (property == "currentTemp") {
        mqtt.currentTemp = value.toInt();
    } else if (property == "setTemp") {
        mqtt.setTemp = value.toInt();
    } else if (property == "balloonTime") {
        mqtt.balloonTime = value.toInt();
    } else {
        // Eigenschaft nicht gefunden
        return false;
    }
    return true;
}

void initDevice() {
    preferences.begin("deviceSettings", true);
    device.seconds = preferences.getUInt("seconds", 0);
    preferences.end(); // save preferences
}

bool writeMqtt(String message, String property, String topic = "volcano_cyber/" + String(system_get_chip_id())) {
    if (mqttActive) {
        if (mqttClient.connected()) {
            String fullTopic = topic;
            fullTopic = fullTopic + "/" + property.c_str();

            mqttClient.publish(fullTopic.c_str(), message.c_str(), true);
            setMqttDataProperty(property, message);
            return true;
        }
    }
    return false;
}


void incrementUsage(const char* name) {
  // increment usage
  preferences.begin("usageCounts", false);
  
  unsigned int count = preferences.getUInt(name, 0) + 1;
  
  preferences.putUInt(name, count);
  writeMqtt(String(count), name);
  
  preferences.end(); // end preferences
  
  logToSerial(name, false);
  logToSerial(" wurde ", false);
  logToSerial(String(count), false);
  logToSerial(" mal aufgerufen.");
}

void mqttKrams(String topic, DynamicJsonDocument &doc) {
    if (mqttActive) {
        int split = 16;
        char buffer[1024];
        size_t n = serializeJson(doc, buffer);

        int numParts = (String(buffer).length() + split - 1) / split;
        logToSerial(String(numParts));

        // create an array of strings to store the parts
        String parts[numParts];

        // split the string and store the parts in the array
        for (int i = 0; i < numParts; i++) {
            int start = i * split; // Startindex for the current part
            parts[i] = String(buffer).substring(start, min((unsigned int)start + split, String(buffer).length())); // Extract the part and take care of the string length
        }

        logToSerial(topic);
        logToSerial(String(n));
        logToSerial(buffer);

        mqttClient.beginPublish(topic.c_str(), n, true);
        // print the parts for debugging
        for (int i = 0; i < numParts; i++) {
            mqttClient.print(parts[i]);
            logToSerial(String(parts[i]));
        }
        mqttClient.endPublish();
    }
}
    

void createHASensor(String id, String name, String icon, String entry_category, String property, String unit_of_measurement) {
    String topic = "homeassistant/sensor/vc_" + String(system_get_chip_id()) + "/" + id + "/config";

    DynamicJsonDocument doc(1024);

    doc["name"] = name;
    doc["uniq_id"] = "vc_" + String(system_get_chip_id()) + "_" + property;
    doc["ic"] = icon;
    doc["entity_category"] = entry_category;
    doc["unit_of_meas"] = unit_of_measurement;
    doc["frc_upd"] = true;
    doc["dev"]["ids"] = "vc_" + String(system_get_chip_id());
    doc["dev"]["name"] = "Volcano Cyber";
    doc["dev"]["sw"] = "Volcano Cyber " + String(VC_VERSION);
    doc["dev"]["mf"] = "Storz & Bickel";
    doc["dev"]["mdl"] = "Volcano Digit";
    doc["avty_t"] = "volcano_cyber/" + String(system_get_chip_id()) + "/mqtt_status";
    doc["pl_avail"] = "connected";
    doc["pl_not_avail"] = "disconnected";
    doc["stat_t"] = "volcano_cyber/" + String(system_get_chip_id()) + "/" + property;
    doc["val_tpl"] = "{{ value|int(0) }}";

    mqttKrams(topic, doc);
}

void createHASwitch(String id, String name, String icon, String entry_category, String property) {
    String topic = "homeassistant/switch/vc_" + String(system_get_chip_id()) + "/" + id + "/config";

    DynamicJsonDocument doc(1024);

    doc["name"] = name;
    doc["uniq_id"] = "vc_" + String(system_get_chip_id()) + "_" + property;
    doc["ic"] = icon;
    doc["entity_category"] = entry_category;
    doc["frc_upd"] = true;
    doc["dev"]["ids"] = "vc_" + String(system_get_chip_id());
    doc["dev"]["name"] = "Volcano Cyber";
    doc["dev"]["sw"] = "Volcano Cyber " + String(VC_VERSION);
    doc["dev"]["mf"] = "Storz & Bickel";
    doc["dev"]["mdl"] = "Volcano Digit";
    doc["avty_t"] = "volcano_cyber/" + String(system_get_chip_id()) + "/mqtt_status";
    doc["pl_avail"] = "connected";
    doc["pl_not_avail"] = "disconnected";
    doc["stat_t"] = "volcano_cyber/" + String(system_get_chip_id()) + "/" + property;
    doc["command_topic"] = "volcano_cyber/" + String(system_get_chip_id()) + "/cmd/vc_" + String(system_get_chip_id()) + "_" + property;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";

    mqttKrams(topic, doc);
    mqttClient.subscribe(String("volcano_cyber/" + String(system_get_chip_id()) + "/cmd/vc_" + String(system_get_chip_id()) + "_" + property).c_str());
}

void sensorDiscovery() {
    createHASensor("big_ballon", "Big Balloon(s)", "mdi:airballoon", "diagnostic", "big", "Balloon(s)");
    createHASensor("little_ballon","Little Balloon(s)", "mdi:airballoon", "diagnostic", "little", "Balloon(s)");
    createHASensor("shot_ballon","Shot Balloon(s)", "mdi:airballoon", "diagnostic", "shot", "Balloon(s)");
    createHASensor("power_on","Power On", "mdi:power", "diagnostic", "on", "");
    createHASensor("power_off","Power Off", "mdi:power", "diagnostic", "off", "");
    createHASensor("balloon_time","Balloon Time", "mdi:timer", "diagnostic", "balloonTime", "ms");
}

void switchDiscovery() {
    createHASwitch("heat_switch", "Heat Switch", "mdi:power", "diagnostic", "heat_switch");
    createHASwitch("air_switch", "Air Switch", "mdi:power", "diagnostic", "air_switch");
    createHASwitch("shot_switch", "Shot Switch", "mdi:power", "diagnostic", "shot_switch");
    createHASwitch("little_switch", "Little Switch", "mdi:power", "diagnostic", "little_switch");
    createHASwitch("big_switch", "Big Switch", "mdi:power", "diagnostic", "big_switch");
}

void airStop() {
    writeMqtt("OFF", "air_switch");
    writeMqtt("OFF", "shot_switch");
    writeMqtt("OFF", "little_switch");
    writeMqtt("OFF", "big_switch");
}

void homeAssistantDiscovery() {
    sensorDiscovery();
    switchDiscovery();
    airStop();
}

void reconnectMqtt() {
    if(!mqttActive) {
        return;
    }
    unsigned int mqttFailed = 0;
    while (!mqttClient.connected()) {
        String name = String(system_get_chip_id());
        logToSerial("Attempting MQTT connection...");
        // Attempt to connect
        if (mqttClient.connect(name.c_str(), mqtt_user.c_str(), mqtt_password.c_str())) {
            logToSerial("connected");
            // Once connected, publish an announcement...
            writeMqtt("connected", "mqtt_status");
            //... and resubscribe
            mqttClient.subscribe(name.c_str());
        } else {
            if(mqttFailed > 5) {
                mqttActive = false;
                return;
            }
            mqttFailed++;
            logToSerial("failed, rc=");
            logToSerial(String(mqttClient.state()));
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void loadMqttSettings() {
    preferences.begin("mqtt", false);
    mqtt_server = preferences.getString("server", "");
    mqtt_port = preferences.getInt("port", 1883);
    mqtt_user = preferences.getString("user", "");
    mqtt_password = preferences.getString("pass", "");
    if(mqtt_server != "" && mqtt_port != 0) {
        mqttActive = true;
    }
    preferences.end();
    logToSerial("mqtt_active: " + String(mqttActive));
    logToSerial("mqtt_server: " + mqtt_server);
    logToSerial("mqtt_port: " + String(mqtt_port));
    logToSerial("mqtt_user: " + mqtt_user);
    logToSerial("mqtt_password: " + mqtt_password);
}

void initWiFi() {
    DNSServer dns;
    wifi_station_set_hostname(const_cast<char*>(WIFI_NAME));
    AsyncWiFiManager wifiManager(&server, &dns);
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);
    wifiManager.addParameter(&custom_lang);
    wifiManager.autoConnect("Volcano Cyber");
    WiFi.hostname(WIFI_NAME);

    String mqtt_server   = custom_mqtt_server.getValue();
    String mqtt_port     = custom_mqtt_port.getValue();
    String mqtt_user     = custom_mqtt_user.getValue();
    String mqtt_password = custom_mqtt_pass.getValue();
    String currentLang   = wifiManager.server->arg("lang");

    preferences.begin("mqtt", false);
    preferences.putString("server", mqtt_server);
    preferences.putString("user", mqtt_user);
    preferences.putString("pass", mqtt_password);
    preferences.putInt("port", mqtt_port.toInt());
    preferences.end();
    
    saveLanguageSetting(currentLang);
    
    logToSerial("IP Address: ", false);
    logToSerial(WiFi.localIP().toString());
    logToSerial("Hostname: ", false);
    logToSerial(WiFi.hostname());
}

void printHead() {
    tft.setTextColor(WHITE, BLACK);
    tft.fillScreen(BLACK);
    tft.setTextSize(1);
    tft.setCursor(67, 0);
    tft.println("Volcano Cyber " + String(VC_VERSION));
    tft.setTextSize(3);
}

void initTFT() {
    tft.init();
    tft.setRotation(2);
    tft.setTouch(calData);
    tft.fillScreen(TFT_BLACK);
    tft.begin();
    printHead();
    tft.setTextSize(2);
    tft.setCursor(0, 20);
    tft.println(t("msg_connect").c_str());
}

void setSecondsToRom(int sec) {
    preferences.begin("deviceSettings", false);
    preferences.putUInt("seconds", sec);
    writeMqtt(String(sec), "balloonTime");
    device.seconds = sec;
    preferences.end(); // end preferences
}

void drawButton(int x, int y, int w, int h, const char* label, uint16_t borderColor, uint16_t textColor, int textSize = 3, bool centerText = true) {
    // set text size
    tft.setTextSize(textSize);

    // draw button background and border
    tft.fillRoundRect(x, y, w, h, 5, BLACK);
    tft.drawRoundRect(x, y, w, h, 5, borderColor);
    
    // set text color
    tft.setTextColor(textColor, BLACK);

    int textX = x + 10; // standard x start point, if text is not centered
    int textY = y + (h / 2) - (textSize * 4); // approximation for y, based on text size

    if (centerText) {
        // calculate text width, depending on text size and assumption that each character is about 6 pixels wide
        int16_t textWidth = strlen(label) * 6 * textSize; // adjust approximation to text size
        
        // calculate new x start point to center text
        textX = x + (w - textWidth) / 2;
    }

    // set cursor for text
    tft.setCursor(textX, textY);
    
    // print text
    tft.println(label);

    // reset text size if necessary
    if (textSize != 3) {
        tft.setTextSize(3);
    }
}


void printHeatButton() {
    const char* label = device.powerStatus ? "btn_off" : "btn_on";
    uint16_t borderColor = device.powerStatus ? RED : GREEN;
    drawButton(0, 10, 160, 75, t(label).c_str(), borderColor, WHITE);
    drawButton(165, 10, 75, 75, t("btn_cal").c_str(), GREEN, WHITE);
}

void printGui() {
    printHead();
    printHeatButton();
    drawButton(0, 87, 240, 75, t("btn_small").c_str(), GREEN, WHITE, 3, true);
    drawButton(0, 164, 240, 75, t("btn_half").c_str(), GREEN, WHITE, 3, true);
    drawButton(0, 241, 240, 75, t("btn_full").c_str(), GREEN, WHITE, 3, true);
    screen = MAIN;
    device.touchCheckInterval = 50;
}

void printStop() {
    device.touchCheckInterval = 100;
    //Serial.println("-------------");
    //Serial.println(stopTime);
    double sec = device.stopTime - millis();
    sec = sec / 1000;
    sec = ceil(sec);
    //Serial.println(sec);
    printHead();
    drawButton(0, 200, 240, 75, "Stop", RED, WHITE, 3, true);
    tft.drawRoundRect(0, 10, 240, 50, 5, GREEN);
    tft.setCursor(65, 70);
    tft.println(String((int)sec) + " Sek. ");
    lastTime = sec;
    screen = STOP;
}

void setSwitch(int pin) {
    device.startPin = pin;
    device.startTime = millis();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // convert payload to string
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }
  logToSerial(topic);
  logToSerial(messageTemp);
  if(String(topic) == "volcano_cyber/" + String(system_get_chip_id()) + "/cmd/vc_" + String(system_get_chip_id()) + "_heat_switch") {
    if(messageTemp == "ON") {
        if (!device.powerStatus) {
            setSwitch(POWER_PIN);
            device.powerStatus = true;
            writeMqtt("ON", "heat_switch");
            incrementUsage("on");
        }
    } else {
        if (device.powerStatus) {
            setSwitch(POWER_PIN);
            device.powerStatus = false;
            writeMqtt("OFF", "heat_switch");
            incrementUsage("off");
        }

    }
  }
}

void initMqtt() {
    loadMqttSettings();
    logToSerial("initMQTT");
    mqttClient.setServer(mqtt_server.c_str(), static_cast<uint16_t>(mqtt_port));
    mqttClient.setCallback(mqttCallback);
    reconnectMqtt();
    writeMqtt("OFF", "heat_switch");

    
    preferences.begin("usageCounts");
    
    writeMqtt(String(device.seconds), "balloonTime");
    writeMqtt(String(preferences.getUInt("on", 0)), "on");
    writeMqtt(String(preferences.getUInt("off", 0)), "off");
    writeMqtt(String(preferences.getUInt("shot", 0)), "shot");
    writeMqtt(String(preferences.getUInt("little", 0)), "little");
    writeMqtt(String(preferences.getUInt("big", 0)), "big");

    preferences.end(); // end preferences
    
    homeAssistantDiscovery();
}

void printCalibration() {
    printHead();
    tft.setTextSize(2);
    tft.setCursor(10, 30);
    tft.println("Kalibrierungsmodus");
    tft.drawRoundRect(0, 200, 240, 75, 5, RED);
    tft.setCursor(88, 225);
    tft.setTextSize(3);
    tft.println("Stop");
    tft.setCursor(65, 70);
    tft.println("0 Sek.");
    screen = CALIBRATION;
    device.stopTime = millis() + 9999999999999999;
    setSwitch(AIR_PIN);
    device.airStatus = true;
}

void updateScreen() {
    bool update = false;
    double sec = 0;
    if (screen == STOP) {
        unsigned int fill = map(millis(), device.startTime, device.stopTime, 0, 240);
        if (fill > lastPos) {
            if (fill < 10) {
                tft.fillRoundRect(0, 10, lastPos, 50, 5, BLACK);
                tft.drawRoundRect(0, 10, 240, 50, 5, GREEN);
            }
            tft.fillRoundRect(0, 10, fill, 50, 5, GREEN);
            lastPos = fill;
        }
        sec = device.stopTime - millis();
        sec = sec / 1000;
        sec = ceil(sec);
        if (sec < 0) {
            sec = 0;
        }
        if (sec < lastTime) {
            update = true;
        }
    }
    if (screen == CALIBRATION) {
        sec = millis() - device.startTime;
        sec = sec / 1000;
        sec = ceil(sec);
        if(sec < 0) {
            sec = 0;
        }
        if (sec > lastTime) {
            update = true;
        }
    }
    if (update) {
        tft.setCursor(65, 70);
        if (String(int(sec)).length() == String(int(lastTime)).length()) {
            tft.fillRoundRect(0, 65, 81, 50, 5, BLACK);
        } else {
            tft.fillRoundRect(0, 65, 240, 50, 5, BLACK);
        }
        tft.println(String(int(sec)) + " Sek.");
        lastTime = sec;
    }
}

void updateStopLast() {
    if(!device.calExit) {
        tft.fillRoundRect(0, 10, 240, 50, 5, GREEN);
        tft.fillRoundRect(0, 65, 240, 50, 5, BLACK);
        tft.setCursor(65, 70);
        tft.println("0 Sek.");
        return;
    }
    device.calExit = false;
}

// Web server Handler
void statusHandler(AsyncWebServerRequest *request) {
    //{heat:true, air:false, poweroff:0}
    long returnTime = 0;
    if (device.stopTime > 0) {
        returnTime = device.stopTime - millis();
    }
    request->send(200, "application/json",
                  "{ \"heat\":" + String(device.powerStatus) + ", \"air\":" + String(device.airStatus) + ", \"poweroff\":" +
                  String(returnTime) + ", \"maxSeconds\":" +
                  String(device.seconds) + ", \"lang\":\"" + String(currentLang) + "\"}");
}
void setStopTime(long milliSec) {
    device.stopTime = millis() + milliSec;
}

void handlePower() {
    setSwitch(POWER_PIN);
    if (device.powerStatus) {
        device.powerStatus = false;
        incrementUsage("off");
        writeMqtt("OFF", "heat_switch");
    } else {
        device.powerStatus = true;
        incrementUsage("on");
        writeMqtt("ON", "heat_switch");
    }
    printHeatButton();
}

void fillBalloon(uint stoptime) {
    if(!device.airStatus) {
        setSwitch(AIR_PIN);
        device.airStatus = true;
        writeMqtt("ON", "air_switch");
        setStopTime(stoptime);
    }
}

void stopB() {
    device.stopTime = millis() - 10;
}

void stopCalibration() {
    device.seconds = millis() - device.startTime;
    setSecondsToRom(device.seconds);
    device.stopTime = device.seconds;
    calStart = 0;
    device.calExit = true;
}

void handleTouchCommand() {
    if (touch == 1) {
        //shotB();
        incrementUsage("shot");
        fillBalloon(device.seconds / SHOT_BALLOON);
    }
    if (touch == 2) {
        //littleB();
        incrementUsage("little");
        fillBalloon(device.seconds / LITTLE_BALLOON);
    }
    if (touch == 3) {
        //bigB();
        incrementUsage("big");
        fillBalloon(device.seconds);
    }
    if (touch == 4) {
        stopB();
    }
    if (touch == 5) {
        handlePower();
    }
    if (touch == 6) {
        incrementUsage("calibration");
        printCalibration();
    }
    if (touch == 7) {
        stopCalibration();
    }
    touch = 0;
}

void processMainScreenTouch(uint16_t touchX, uint16_t touchY) {
    if (touchY > 10 && touchY < 85 && touchX < 160) {
        touch = 5;
    }
    if (touchY > 10 && touchY < 85 && touchX > 165 && calStart == 0) {
        calStart = millis();
    }
    if (touchY > 87 && touchY < 162) {
        touch = 1;
    }
    if (touchY > 164 && touchY < 239) {
        touch = 2;
    }
    if (touchY > 241 && touchY < 316) {
        touch = 3;
    }
    if(calStart > 0) {
        if(millis() - calStart > MIN_CALIBRATION_TIME) {
            touch = 6;
        }
    }
    if (touch > 0) {
        lastTouch = millis();
        handleTouchCommand();
    }
}

void processStopScreenTouch(uint16_t touchX, uint16_t touchY) {
    if (touchY > 200 && touchY < 275) {
        touch = 4;
        lastTouch = millis();
        handleTouchCommand();
    }
}

void processCalibrationScreenTouch(uint16_t touchX, uint16_t touchY) {
    if (touchY > 200 && touchY < 275) {
        touch = 7;
        lastTouch = millis();
        handleTouchCommand();
    }
}

void processTouch(uint16_t touchX, uint16_t touchY) {
    if (screen == MAIN) {
        processMainScreenTouch(touchX, touchY);
    } else if (screen == STOP) {
        processStopScreenTouch(touchX, touchY);
    } else if (screen == CALIBRATION) {
        processCalibrationScreenTouch(touchX, touchY);
    }
}

void checkTouch() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck < device.touchCheckInterval) return;
    lastCheck = millis();

    uint16_t touchX = 0, touchY = 0;
    if (tft.getTouch(&touchX, &touchY) && lastTouch + DEBOUNCE_TIME < millis()) {
        processTouch(touchX, touchY);
        lastTouch = millis();
    }
}

void switchHandler(AsyncWebServerRequest *request) {
    String action = request->getParam("action")->value();
    if (device.startTime == 0) {
        if (action == "poweron") {
            if (!device.powerStatus) {
                setSwitch(POWER_PIN);
                writeMqtt("ON", "heat_switch");
                device.powerStatus = true;
            }
        }
        if (action == "poweroff") {
            if (device.powerStatus) {
                setSwitch(POWER_PIN);
                writeMqtt("OFF", "heat_switch");
                device.powerStatus = false;
            }
        }
        if (action == "airon") {
            if (!device.airStatus) {
                setSwitch(AIR_PIN);
                writeMqtt("ON", "air_switch");
                device.airStatus = true;
            }
        }
        if (action == "shot") {
            //shotB();
            incrementUsage("shot");
            fillBalloon(device.seconds / SHOT_BALLOON);
        }
        if (action == "little") {
            //littleB();
            incrementUsage("little");
            fillBalloon(device.seconds / LITTLE_BALLOON);
        }
        if (action == "big") {
            //bigB();
            incrementUsage("big");
            fillBalloon(device.seconds);
        }
    }
    if (action == "airoff") {
        airStop();
        stopB();
    }

    if (action == "stop") {
        stopB();
    }
    if (action == "lang") {
        String lang = request->getParam("lang")->value();
        saveLanguageSetting(lang);
        loadLanguageSetting();
        printGui();
    }
    if(action == "calibration") {
        String cal = request->getParam("cal")->value();
        setSecondsToRom(cal.toInt());
    }
    statusHandler(request);
}

void toggle(int pin) {
    digitalWrite(pin, LOW);
    toggleOp.active = true;
    toggleOp.pin = pin;
    toggleOp.endTime = millis() + 180;
    device.lastAction = millis();
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void setupWebServer() {
    // Static resources
    server.serveStatic("/css", LittleFS, "/css");
    server.serveStatic("/js", LittleFS, "/js");
    server.serveStatic("/images", LittleFS, "/images");
    // Dynamic routes
    server.on("/status", HTTP_GET, statusHandler);
    server.on("/switch", HTTP_GET, switchHandler);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, F("/index.html"));
    });
    server.onNotFound(notFound);
    server.begin();
}

void checkAutoShutdown() {
    if (device.lastAction + AUTO_SHUTDOWN_TIME < millis()) {
        device.powerStatus = false;
        writeMqtt("OFF", "heat_switch");
        device.lastAction = 0;
        printHeatButton();
    }
}

void handleToggleOperation() {
    if (toggleOp.active && millis() >= toggleOp.endTime) {
        digitalWrite(toggleOp.pin, HIGH);
        toggleOp.active = false;
    }
}

void scrollAddress(uint16_t VSP) {
  tft.writecommand(ILI9341_VSCRSADD); // Vertical scrolling start address
  tft.writedata(VSP >> 8);
  tft.writedata(VSP);
}

int scroll_slow(int lines, int wait) {
  int yTemp = yStart;
  for (int i = 0; i < lines; i++) {
    yStart++;
    if (yStart == 320 - BOT_FIXED_AREA) yStart = TOP_FIXED_AREA;
    scrollAddress(yStart);
    delay(wait);
  }
  return  yTemp;
}

void screensaver() {
    static boolean screensaverOn = false;
    if(!screensaverOn) {
        tft.fillScreen(BLACK);
        tft.setTextSize(1);
        screensaverOn = true;
    }
      // First fill the screen with random streaks of characters
  for (int j = 0; j < 600; j += TEXT_HEIGHT) {
    for (int i = 0; i < 40; i++) {
      if (pos[i] > 20) pos[i] -= 3; // Rapid fade initially brightness values
      if (pos[i] > 0) pos[i] -= 1; // Slow fade later
      if ((random(20) == 1) && (j<400)) pos[i] = 63; // ~1 in 20 probability of a new character
      tft.setTextColor(pos[i] << 5, ILI9341_BLACK); // Set the green character brightness
      if (pos[i] == 63) tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK); // Draw white character
      xPos += tft.drawChar(random(32, 128), xPos, yDraw, 1); // Draw the character
    }
    yDraw = scroll_slow(TEXT_HEIGHT, 14); // Scroll, 14ms per pixel line
    xPos = 0;
  }
}

void setup() {
    // Initialize Serial, Pins, TFT, and WiFi
    initSerial();
    loadLanguageSetting();
    initDevice();
    initPins();

    initTFT();
    initWiFi();
    initMqtt();


    logToSerial("Volcano Cyber");
    // initialize LittleFS
    if (!LittleFS.begin()) {
      logToSerial("Ein Fehler ist beim Mounten von LittleFS aufgetreten");
      return;
    }

    setupWebServer();
    printGui();
}

void loop() {
    handleToggleOperation();
    if (device.stopTime != 0 and millis() >= device.stopTime) {
        updateStopLast();
        toggle(AIR_PIN);
        printGui();
        airStop();
        device.airStatus = false;
        device.stopTime = 0;
        device.startTime = 0;
        lastTime = 0;
        lastPos = 0;
    }
    if (device.startTime != 0 and device.startPin != 0 and millis() >= device.startTime) {
        toggle(device.startPin);
        if (device.startPin == POWER_PIN || (device.startPin == AIR_PIN && device.stopTime == 0)) {
            device.startTime = 0;
            if (screen == STOP) {
                printGui();
            } else {
                printHeatButton();
            }
        } else if (screen != CALIBRATION) {
            printStop();
        }
        device.startPin = 0;
    }
    checkTouch();
    if (device.lastAction > 0) {
        checkAutoShutdown();
    }
    if (device.stopTime != 0) {
        updateScreen();
    }
    if (device.stopTime != 0) {
        updateScreen();
    }
    if(mqttActive){
        if(!mqttClient.connected()) {
            reconnectMqtt();
        }
        mqttClient.loop();
    }
    //screensaver();
    delay(1);
}
