#include <ESP8266WiFi.h>
#include <lib_bl999.h>

//WiFi setup
#define ssid       ".........."
#define password   ".........."

//Thingspeak setup
String thingspeakApiKeys[] = {
        "...........",
        "...........",
        "..........."
};
#define thingSpeakAddress "api.thingspeak.com"
#define thingSpeakUpdateJsonEndpoint "/update.json"
#define thingSpeakHttpPort 80
#define thingSpeakUpdateInterval 16000 //16 sec

//bl999 setup
static BL999Info current_bl999_data;
static BL999Info bl999_data[3];
static unsigned long lastPostTime[3] = {0, 0, 0};
static boolean hasUnsentData[3] = {false, false, false};
#define bl999_pin 4

//wifi client
WiFiClient client;
byte lastClientStatus = WL_DISCONNECTED;

void setupWifi() {
    byte status = WiFi.status();

    if (status == WL_DISCONNECTED || status == WL_CONNECTION_LOST) {
        WiFi.begin(ssid, password);

        while (WiFi.status() == WL_DISCONNECTED) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("");
            Serial.println("WiFi connected");
            Serial.println("IP address: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("");
            Serial.println("WiFi connection failed");
            Serial.println("Reason: " + String(WiFi.status()));
        }
    }
}

void printBl999InfoToSerial(BL999Info &info) {
    Serial.println("====== Got message: ");
    Serial.println("Channel: " + String(info.channel));
    Serial.println("powerUUID: " + String(info.powerUUID));
    Serial.println("battery: " + String(info.battery));
    Serial.println("temperature: " + String(info.temperature / 10.0));
    Serial.println("humidity: " + String(info.humidity));
}

void copyCurrentBl999DataToArray(BL999Info &info) {
//    bl999_data[info.channel - 1] = {
//            info.channel,
//            info.powerUUID,
//            info.battery,
//            info.temperature,
//            info.humidity
//    };

    bl999_data[info.channel - 1].channel = info.channel;
    bl999_data[info.channel - 1].powerUUID = info.powerUUID;
    bl999_data[info.channel - 1].battery = info.battery;
    bl999_data[info.channel - 1].temperature = info.temperature;
    bl999_data[info.channel - 1].humidity = info.humidity;
}

void postData(BL999Info &info) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Can not send data - WiFi not connected");
        return;
    }

    if (!client.connect(thingSpeakAddress, thingSpeakHttpPort)) {
        Serial.println("Can not send data - connection failed");
        return;
    }

    String postString =
            "field1=" + String(info.battery) +
            "&field2=" + String(info.temperature / 10.0) +
            "&field3=" + String(info.humidity);

    client.println("POST " + String(thingSpeakUpdateJsonEndpoint) + " HTTP/1.1");
    client.println("Host: " + String(thingSpeakAddress));
    client.println("Connection: close");
    client.println("X-THINGSPEAKAPIKEY: " + String(thingspeakApiKeys[info.channel - 1]));
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: " + String(postString.length()));
    client.print("\r\n\r\n");
    client.print(postString);

    delay(500);

    // Read all the lines of the reply from server and print them to Serial
    while (client.available()) {
        String line = client.readStringUntil('\r');
        Serial.print(line);
    }

    Serial.println();
    Serial.println("closing connection");
}

void postAllDatas() {
    Serial.println("Going to update all channels");
    for (byte i = 0; i < 3; i++) {
        if (hasUnsentData[i]) {
            Serial.println("Channel has unsent data: " + String(i + 1));
            unsigned long currentTime = millis();
            if (millis() - lastPostTime[i] > thingSpeakUpdateInterval) {
                Serial.println("Going to update channel: " + String(bl999_data[i].channel));
                postData(bl999_data[i]);
                hasUnsentData[i] = false; //mark it as sent
                lastPostTime[i] = millis(); //set last sent time
            }
        }
    }
}

void setup() {
    Serial.begin(115200);

    //set digital pin to read info from
    bl999_set_rx_pin(bl999_pin);

    //start reading data from sensor
    bl999_rx_start();
}

void loop() {
    //setup wifi
    //in loop to re init connection if necessary
    setupWifi();

    //blocks until message will not be read
    bl999_wait_rx();

    //read message to info and if check sum correct - output it to serial port
    if (bl999_get_message(current_bl999_data)) {
        printBl999InfoToSerial(current_bl999_data);
        copyCurrentBl999DataToArray(current_bl999_data);
        hasUnsentData[current_bl999_data.channel - 1] = true;
        postAllDatas();
    }
}




