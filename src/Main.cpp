#include <DS3231.h>  //Время
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <OneWire.h>
#include <Wire.h>

#include <RtcDS3231.h>
#include <Ticker.h>
#include <WiFiUdp.h>
#include <uEEPROMLib.h>
#include "TimeAlarms.h"

#define FIREBASE_HOST "aqua-3006a.firebaseio.com"
#define FIREBASE_AUTH "eRxKqsNsandXnfrDtd3wjGMHMc05nUeo5yeKmuni"

#define WIFI_SSID "MikroTik"
#define WIFI_PASSWORD "11111111"

#define GMT 3

#define ONE_WIRE_BUS 14  //Пин, к которому подключены датчики DS18B20 D5 GPIO15

#define ALARMS_COUNT 6  //Количество таймеров, которые нужно удалять. Помимо их есть еще два основных - каждую минуту и каждые пять.
//Указанное кол-во надо увеличить в случае появления нового расписания для нового устройства, например, дозаторы, CO2, нагреватель и
//прочие устройства которые будут запланированы на включение или выключение
//----------------------------------------------------------------------------------
uint8_t wifiMaxTry = 5;  //Попытки подключения к сети
uint8_t wifiConnectCount = 0;

// uEEPROMLib eeprom;
uEEPROMLib eeprom(0x57);
unsigned int pos;

unsigned long lastTime = 0;

uint32_t count = 0;
unsigned long lastmillis;
byte upM = 0, upH = 0;
int upD = 0;

DS3231 clockRTC;
RTCDateTime dt;
RtcDS3231 rtc;

const char* WiFi_hostname = "ESP8266-0";
IPAddress timeServerIP;
const char* ntpServerName = "pool.ntp.org";
const int NTP_PACKET_SIZE = 48;

const long timeZoneOffset = GMT * 3600;
byte packetBuffer[NTP_PACKET_SIZE];
boolean update_status = false;
byte count_sync = 0;
byte hour1 = 0;
unsigned long hour_sync = 0;
byte minute1 = 0;
unsigned long minute_sync = 0;
byte second1 = 0;

WiFiUDP udp;

unsigned long sendDataPrevMillis = 0;

String path = "/ESP8266_Test/Stream";
String path2 = "/";

OneWire ds(ONE_WIRE_BUS);
byte data[12];
float temp1, temp2;
//Адреса датчиков
byte addr1[8] = {0x28, 0xFF, 0x17, 0xF0, 0x8B, 0x16, 0x03, 0x13};  //адрес датчика DS18B20
byte addr2[8] = {0x28, 0xFF, 0x5F, 0x1E, 0x8C, 0x16, 0x03, 0xE2};  //адрес датчика DS18B20

float DS18B20(byte* adres) {
    unsigned int raw;
    ds.reset();
    ds.select(adres);
    ds.write(0x44, 1);
    ds.reset();
    ds.select(adres);
    ds.write(0xBE);
    for (byte i = 0; i < 9; i++) {
        data[i] = ds.read();
    }
    raw = (data[1] << 8) | data[0];
    float celsius = (float)raw / 16.0;
    return celsius;
}

void getTemperature() {
    temp1 = DS18B20(addr1);
    temp2 = DS18B20(addr2);
    Serial.println(temp1);
    Serial.println(temp2);
}

void uptime() {
    if (lastmillis + 60000 < millis()) {
        lastmillis = millis();
        upM++;
        if (upM == 60) {
            upM = 0;
            upH++;
        }
        if (upH == 24) {
            upH = 0;
            upD++;
        }
        char buf[21];
        sprintf(buf, "Uptime %d day(s) %d:%d", upD, upH, upM);
        Serial.println(buf);
    }
}
void initRTC() {
    clockRTC.begin();
    dt = clockRTC.getDateTime();
    Serial.println("Часы запущены. Время " + String(clockRTC.dateFormat("H:i:s Y-m-d", dt)));
}
void eeprom_test() {
    delay(2000);

    Serial.println("Serial OK");

    delay(2500);
    Serial.println("Delay OK");

#ifdef ARDUINO_ARCH_ESP8266
    Wire.begin();  // D3 and D4 on ESP8266
#else
    Wire.begin();
#endif

#ifdef ARDUINO_ARCH_AVR
    int inttmp = 32123;
#else
    // too logng for AVR 16 bits!
    int inttmp = 24543557;
#endif
    float floattmp = 3.1416;
    char chartmp = 'A';

    char string[17] = "ForoElectro.Net\0";

    // Testing template
    if (!eeprom.eeprom_write(0, inttmp)) {
        Serial.println("Failed to store INT");
    } else {
        Serial.println("INT correctly stored");
    }
    if (!eeprom.eeprom_write(4, floattmp)) {
        Serial.println("Failed to store FLOAT");
    } else {
        Serial.println("FLOAT correctly stored");
    }
    if (!eeprom.eeprom_write(8, chartmp)) {
        Serial.println("Failed to store CHAR");
    } else {
        Serial.println("CHAR correctly stored");
    }

    if (!eeprom.eeprom_write(9, (byte*)&string[0], 16)) {
        Serial.println("Failed to store STRING");
    } else {
        Serial.println("STRING correctly stored");
    }

    inttmp = 0;
    floattmp = 0;
    chartmp = 0;
    string[0] = string[1] = string[2] = string[3] = string[4] = 0;

    Serial.print("int: ");
    eeprom.eeprom_read(0, &inttmp);
    Serial.println(inttmp);

    Serial.print("float: ");
    eeprom.eeprom_read(4, &floattmp);
    Serial.println((float)floattmp);

    Serial.print("char: ");
    eeprom.eeprom_read(8, &chartmp);
    Serial.println(chartmp);

    Serial.print("chararray: ");
    eeprom.eeprom_read(9, (byte*)&string[0], 16);
    Serial.println(string);

    Serial.println();

    for (pos = 26; pos < 1000; pos++) {
        eeprom.eeprom_write(pos, (unsigned char)(pos % 256));
    }

    pos = 0;
}

void sendNTPpacket(IPAddress& address) {
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;
    packetBuffer[1] = 0;
    packetBuffer[2] = 6;
    packetBuffer[3] = 0xEC;
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    udp.beginPacket(address, 123);
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
}

// Синхронизация времени
void syncTime() {
    udp.begin(2390);
    WiFi.hostByName(ntpServerName, timeServerIP);
    sendNTPpacket(timeServerIP);
    delay(1000);
    int cb = udp.parsePacket();
    if (!cb) {
        Serial.printf_P(PSTR("Нет ответа от сервера времени %s\n"), ntpServerName);
        update_status = false;
        count_sync++;
        minute_sync = rtc.getEpoch();
    } else {
        update_status = true;
        count_sync = 0;
        hour_sync = rtc.getEpoch();
        Serial.printf_P(PSTR("Получен ответ от сервера времени %s\n"), ntpServerName);
        udp.read(packetBuffer, NTP_PACKET_SIZE);
        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        unsigned long secsSince1900 = highWord << 16 | lowWord;
        const unsigned long seventyYears = 2208988800UL;
        unsigned long epoch = secsSince1900 - seventyYears;
        // 2 секунды разница с большим братом
        epoch = epoch + 2 + GMT * 3600;
        hour1 = (epoch % 86400L) / 3600;
        minute1 = (epoch % 3600) / 60;
        second1 = epoch % 60;

        char str[20];
        rtc.dateTimeToStr(str);
        Serial.printf_P(PSTR("%s\n"), str);
        uint32_t rtcEpoch = rtc.getEpoch();
        Serial.printf_P(PSTR("RTC эпоха: %d\n"), rtcEpoch);
        Serial.printf_P(PSTR("NTP эпоха: %d\n"), epoch);

        if (abs(rtcEpoch - epoch) > 2) {
            Serial.printf_P(PSTR("Обновляем RTC (разница между эпохами = "));
            if ((rtcEpoch - epoch) > 10000) {
                Serial.printf_P(PSTR("%s\n"), abs(epoch - rtcEpoch));
            } else {
                Serial.printf_P(PSTR("%s\n"), abs(rtcEpoch - epoch));
            }
            rtc.setEpoch(epoch);
        } else {
            Serial.printf_P(PSTR("Дата и время RTC не требуют синхронизации\n"));
        }
    }
}

void readOptionsEEPROM() {
    Serial.printf_P(PSTR("Загрузка настроек из внутренней памяти\n"));
    // todo
};

void readOptionsFirebase() {
    Serial.printf_P(PSTR("Загрузка настроек из Firebase\n"));
    // todo
};
//Сохранение показаний датчиков температуры
void writeTemperature() {
    Serial.printf_P(PSTR("writeTemperature\n"));
    if (WiFi.isConnected()) {
        FirebaseData firebaseData;
        Serial.printf_P(PSTR("Сохраняем в Firebase\n"));
        Serial.printf_P(PSTR("Сохранение температуры с первого датчика\n"));
        if (Firebase.setFloat(firebaseData, path2 + "Online/temp1", temp1)) {
            Serial.printf_P(PSTR("Успешно\n"));
        } else {
            Serial.printf_P(PSTR("Ошибка"));
            Serial.printf_P(PSTR("%s\n"), firebaseData.errorReason().c_str());
        }
        /*   Serial.printf_P(PSTR("Сохранение температуры со второго датчика\n"));
           if (Firebase.setFloat(firebaseData, path2 + "Online/temp2", temp2)) {
               Serial.printf_P(PSTR("Успешно\n"));
           } else {
               Serial.printf_P(PSTR("Ошибка"));
               Serial.printf_P(PSTR("%s\n"), firebaseData.errorReason().c_str());
           }
           Serial.printf_P(PSTR("Сохранение даты/времени измерения температур\n"));
           if (Firebase.setString(firebaseData, path2 + "Online/DateTime", String(clockRTC.dateFormat("H:i:s Y-m-d", dt)))) {
               Serial.printf_P(PSTR("Успешно\n"));
           } else {
               Serial.printf_P(PSTR("Ошибка"));
               Serial.printf_P(PSTR("%s\n"), firebaseData.errorReason().c_str());
           }*/
        /*
        String str = "";
         FirebaseJson json;
         json.addDouble("temp1", temp1)
             .addDouble("temp2", temp2)
             .addString("datetime", String(clockRTC.dateFormat("H:i:s Y-m- d", dt)));

         if (Firebase.pushJSON(firebaseData, "/test/append", json)) {
             Serial.println(firebaseData.dataPath());
             Serial.println(firebaseData.pushName());
             Serial.println(firebaseData.dataPath() + "/" + firebaseData.pushName());
         } else {
             Serial.println(firebaseData.errorReason());
         }
        */
    } else {
        Serial.printf_P(PSTR("Сохраняем в EEPROM\n"));
    }
}
void timer10sec() {
    time_t now1 = Alarm.getNextTrigger(0) - 18000;
    Serial.println("Timer 1: now - " + String(clockRTC.dateFormat("H:i:s", clockRTC.getDateTime())) + " next " +
                   String(ctime(&now1)));
    // getTemperature();
    // 
    return;
}
void timer20sec() {
    writeTemperature();
    time_t now1 = Alarm.getNextTrigger(0) - 18000;
    Serial.println("Timer 2: now - " + String(clockRTC.dateFormat("H:i:s", clockRTC.getDateTime())) + " next " +
                   String(ctime(&now1)));
}
void timer30sec() {
    time_t now1 = Alarm.getNextTrigger(0) - 18000;
    Serial.println("Timer 3: now - " + String(clockRTC.dateFormat("H:i:s", clockRTC.getDateTime())) + " next " +
                   String(ctime(&now1)));
}

//Сохрнение температур в журанал и в мгновенные значения
void fiveMinuteTimer() {
    /*
      Serial.println("Вызван таймер");
      dt = clockRTC.getDateTime();
      Serial.println(String(clockRTC.dateFormat("H:i:s Y-m-d", dt)));
    */
    Serial.println(ESP.getFreeHeap());
    getTemperature();
    Serial.println(ESP.getFreeHeap());
    writeTemperature();
    Serial.println(ESP.getFreeHeap());
    /* saveTemps();
     if (getFlagRereadSettings()) {
       clearAlarms();
       LoadVariables();
       DPRINTLN("Alarms count = " + String(Alarm.count()));
       Firebase.setBool("UpdateSettings", false);
       if (isFailed) {
         DPRINTLN("Error while setting UpdateSettings");
         DPRINTLN(Firebase.error());
       }
     }
     if (getFlagTimeSync()) {
       udp.begin(localPort);
       oldloop();                  // синхронизируем время

       
        // clearAlarms();
       //  LoadVariables();
       //  DPRINTLN("Alarms count = " + String(Alarm.count()));
       
       Firebase.setBool("TimeSync", false);
       if (isFailed) {
         DPRINTLN("Error while setting TimeSync");
         DPRINTLN(Firebase.error());
       }
     }
     */
}
void oneMinuteTimer() {
    dt = clockRTC.getDateTime();
    Serial.println(String(clockRTC.dateFormat("H:i:s Y-m-d", dt)));
    uptime();
    // getTemperature();
}
void setup() {
    Serial.begin(115200);
    Serial.println();

    //Запуск часов реального времени
    initRTC();

    lastmillis = millis();
    lastTime = millis();

    // eeprom_test();
    Serial.printf_P(PSTR("Подключение к WiFi: %s\n"), WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while ((WiFi.status() != WL_CONNECTED) && (wifiConnectCount != wifiMaxTry)) {
        Serial.printf_P(PSTR("."));
        delay(1000);
        Serial.printf_P(PSTR("\n"));
        wifiConnectCount++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        wifiConnectCount = 5;
        Serial.printf_P(PSTR("Не удалось подключиться к WiFi: %s"), WIFI_SSID);
        readOptionsEEPROM();
    } else {
        Serial.printf_P(PSTR("Успешное подключение к WiFi: %s\n"), WIFI_SSID);
        Serial.println("IP адрес: " + WiFi.localIP().toString() + "\n");

        Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
        Firebase.reconnectWiFi(true);
        syncTime();  // синхронизируем время
        readOptionsFirebase();
    }
    // Serial.printf_P(PSTR("Количество таймеров до: %d\n"), Alarm.count());
    Alarm.timerRepeat(60, timer10sec);
    Alarm.timerRepeat(60*2, timer20sec);
    Alarm.timerRepeat(60*3, timer30sec);
    // Alarm.timerRepeat(10, fiveMinuteTimer);
    // Alarm.timerRepeat(3, oneMinuteTimer);  // вывод uptime каждую минуту
    // Alarm.timerRepeat(5, uptime);  // вывод uptime каждую минуту
    // Serial.printf_P(PSTR("Количество таймеров после: %d\n"), Alarm.count());
    //  fiveMinuteTimer();
}

void loop() {
    Alarm.delay(0);  // must use Alarm delay to use the TimeAlarms library

    if (millis() - lastTime > 15000) {
        Serial.println(".........TIMER INFO...........");
        lastTime = millis();
        Serial.println(String(clockRTC.dateFormat("H:i:s", clockRTC.getDateTime())));
        Serial.printf_P(PSTR("Количество таймеров: %d\n"), Alarm.count());
        time_t now1 = Alarm.getNextTrigger(0) - 18000;
        time_t now2 = Alarm.getNextTrigger(1) - 18000;
        time_t now3 = Alarm.getNextTrigger(2) - 18000;

        Serial.printf_P(PSTR("таймер 0: %s\n"), ctime(&now1));
        Serial.printf_P(PSTR("таймер 1: %s\n"), ctime(&now2));
        Serial.printf_P(PSTR("таймер 2: %s\n"), ctime(&now3));
        Serial.println("END......TIMER INFO...........");
        // Serial.printf_P(PSTR("таймер 1: %s\n"), Alarm.getNextTrigger(1));
    }
}