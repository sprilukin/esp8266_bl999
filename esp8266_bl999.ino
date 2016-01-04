#include <ESP8266WiFi.h>
#include <lib_bl999.h>

//WiFi setup
#define ssid       "....."
#define password   "........"

//Thingspeak setup
String thingspeakApiKeys[] = {
        "................",
        "................",
        "................"
};
#define thingSpeakAddress "api.thingspeak.com"
#define thingSpeakUpdateJsonEndpoint "/update.json"
#define thingSpeakHttpPort 80
//Update data for the specific channel not frequently than 16 sec
#define thingSpeakUpdateInterval 16000
//Update data for the specific channel if it's the same not frequently than 5 mins
#define thingSpeakUpdateSameDataInterval 300000 //5 min

//bl999 setup
static BL999Info current_bl999_data;
static BL999Info bl999_data[3];
static unsigned long lastPostTime[3] = {0, 0, 0};
static boolean hasUnsentData[3] = {false, false, false};
#define bl999_pin 4

//wifi client
WiFiClient client;
byte lastClientStatus = WL_DISCONNECTED;

//Status leds
#define whiteLed 12
#define greenLed 13
#define redLed 14

// === LED secion
void turnLedOn(byte pin) {
    digitalWrite(pin, HIGH);
}

void turnLedOff(byte pin) {
    digitalWrite(pin, LOW);
}

void setAllLedsState(byte white, byte green, byte red) {
    if (white == 1)  {
        turnLedOn(whiteLed);
    } else if (white == 0) {
        turnLedOff(whiteLed);
    }
    if (green == 1)  {
        turnLedOn(greenLed);
    } else if (green == 0) {
        turnLedOff(greenLed);
    }
    if (red == 1)  {
        turnLedOn(redLed);
    } else if (red == 0) {
        turnLedOff(redLed);
    }
}

void turnWifiInProgressLedState() {
    setAllLedsState(1, 1, 1);
}

void turnWifiOkLedState() {
    setAllLedsState(1, 0, 0);
}

void turnWifiErrorLedState() {
    setAllLedsState(0, 0, 1);
}

void turnSendInProgressState() {
    setAllLedsState(2, 0, 2);
}

void turnLastSendOkState() {
    setAllLedsState(2, 1, 0);
}

void turnLastSendErrorState() {
    setAllLedsState(2, 0, 1);
}

// === wifi section
void setupWifi() {
    byte status = WiFi.status();

    if (status == WL_DISCONNECTED || status == WL_CONNECTION_LOST) {

        turnWifiInProgressLedState();

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
            turnWifiOkLedState();
        } else {
            Serial.println("");
            Serial.println("WiFi connection failed");
            Serial.println("Reason: " + String(WiFi.status()));
            turnWifiErrorLedState();
        }
    }
}

// === BL999 section
void printBl999InfoToSerial(BL999Info &info) {
    Serial.println("====== Got message: ");
    Serial.println("Channel: " + String(info.channel));
    Serial.println("powerUUID: " + String(info.powerUUID));
    Serial.println("battery: " + String(info.battery));
    Serial.println("temperature: " + String(info.temperature / 10.0));
    Serial.println("humidity: " + String(info.humidity));
}

void copyCurrentBl999DataToArray(BL999Info &info) {
    unsigned long currentTime = millis();
    //lastPostTime[i]

    if (info.powerUUID == bl999_data[info.channel - 1].powerUUID &&
        info.battery == bl999_data[info.channel - 1].battery &&
        info.temperature == bl999_data[info.channel - 1].temperature &&
        info.humidity == bl999_data[info.channel - 1].humidity &&
        !hasUnsentData[info.channel - 1]) {
        //OK we got the same data as it was as was already sent to cloud
        if (millis() - lastPostTime[info.channel - 1] < thingSpeakUpdateSameDataInterval) {
            //nothing to do - data is the same as was sent last time
            //need to wait at least thingSpeakUpdateSameDataInterval millis
            //to send same data
            Serial.println("Data for channel ["
                           + String(info.channel)
                           + "] Was not changed during last "
                           + String(thingSpeakUpdateSameDataInterval / (1000 * 60))
                           + " min. It will not be sent");
            return;
        }
    }

    bl999_data[info.channel - 1].channel = info.channel;
    bl999_data[info.channel - 1].powerUUID = info.powerUUID;
    bl999_data[info.channel - 1].battery = info.battery;
    bl999_data[info.channel - 1].temperature = info.temperature;
    bl999_data[info.channel - 1].humidity = info.humidity;

    hasUnsentData[info.channel - 1] = true;
}

// === thingspeak section
boolean postData(BL999Info &info) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Can not send data - WiFi not connected");
        return false;
    }

    if (!client.connect(thingSpeakAddress, thingSpeakHttpPort)) {
        Serial.println("Can not send data - connection failed");
        return false;
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

    boolean result = false;

    // Read all the lines of the reply from server and print them to Serial
    while (client.available()) {
        String line = client.readStringUntil('\r');
        if (line.indexOf("Status: ") > -1) {
            Serial.println(line);
            if (line.indexOf("200 OK") > -1) {
                result = true;
            }
        }
    }

    Serial.println();
    Serial.println("closing connection");

    return result;
}

void postAllDatas() {
    Serial.println();
    Serial.println("Going to update all channels");
    for (byte i = 0; i < 3; i++) {
        if (hasUnsentData[i]) {
            Serial.println("Channel has unsent data: " + String(i + 1));
            unsigned long currentTime = millis();
            if (millis() - lastPostTime[i] > thingSpeakUpdateInterval) {
                Serial.println("Going to update channel: " + String(bl999_data[i].channel));
                turnSendInProgressState();
                if (postData(bl999_data[i])) {
                    hasUnsentData[i] = false; //mark it as sent
                    lastPostTime[i] = millis(); //set last sent time
                    turnLastSendOkState();
                } else {
                    turnLastSendErrorState();
                }
            }
        }
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(whiteLed, OUTPUT);
    pinMode(greenLed, OUTPUT);
    pinMode(redLed, OUTPUT);

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
        postAllDatas();
    }
}

