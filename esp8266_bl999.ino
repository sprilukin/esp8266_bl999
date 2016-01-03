#include <ESP8266WiFi.h>
#include <lib_bl999.h>

//WiFi setup
#define ssid       "............"
#define password   "..........."

//Thingspeak setup
#define thingspeakApiKey ".................."
#define thingSpeakAddress "api.thingspeak.com"
#define thingSpeakUpdateJsonEndpoint "/update.json"
#define thingSpeakHttpPort 80
#define thingSpeakUpdateInterval 16000 //16 sec

//bl999 setup
static BL999Info info;
static BL999Info infos[3];
#define bl999_pin 4

//wifi client
WiFiClient client;

void setupWifi() {
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(115200);

    //set digital pin to read info from
    bl999_set_rx_pin(bl999_pin);

    //start reading data from sensor
    bl999_rx_start();

    //setup wifi
    setupWifi();
}

void postData(BL999Info& info) {
    if (!client.connect(thingSpeakAddress, thingSpeakHttpPort)) {
        Serial.println("connection failed");
        return;
    }

    String postString = "field1="+String(info.channel)+"&field2="+String(info.powerUUID)+"&field3="+String(info.battery)+"&field4="+String(info.temperature/10.0)+"&field5="+String(info.humidity);

    client.println("POST " + String(thingSpeakUpdateJsonEndpoint) + " HTTP/1.1");
    client.println("Host: " + String(thingSpeakAddress));
    client.println("Connection: close");
    client.println("X-THINGSPEAKAPIKEY: " + String(thingspeakApiKey));
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: " + String(postString.length()));
    client.print("\r\n\r\n");
    client.print(postString);

    delay(500);

    // Read all the lines of the reply from server and print them to Serial
    while(client.available()){
        String line = client.readStringUntil('\r');
        //Serial.print(line);
    }

    Serial.println();
    Serial.println("closing connection");
}

void loop() {

    //blocks until message will not be read
    bl999_wait_rx();

    //read message to info and if check sum correct - output it to serial port
    if (bl999_get_message(info)) {
        Serial.println("====== Got message: ");
        Serial.println("Channel: " + String(info.channel));
        Serial.println("powerUUID: " + String(info.powerUUID));
        Serial.println("battery: " + String(info.battery == 0));
        Serial.println("temperature: " + String(info.temperature / 10.0));
        Serial.println("humidity: " + String(info.humidity));

        postData(info);
    }
}








