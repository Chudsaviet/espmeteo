#include "main.h"
#include "private.h"

ESP8266WiFiMulti WiFiMulti;
WiFiClient espClient;
PubSubClient mqtt(espClient);
DHT dht(DHTPIN, DHTTYPE, 11);
Adafruit_BMP085 bmp;
uint loop_num = 0;
uint32_t ESP_CHIP_ID = ESP.getChipId();
char mqtt_message_body[MQTT_MESSAGE_MAX_SIZE];
const char* mqtt_message_template="{device:ESP8266,temperature:%s}";

void module_panic(const char* message) {
    const char panic[] = "Panic, action failed:";
    Serial.println(panic);
    Serial.println(message);
    delay(STANDART_SHORT_DELAY*10);
    reboot_module();
}

Measures do_measures() {
    Measures result;

    const char* dht22_error = "Error reading DHT22";
    result.dht22_humidity = dht.readHumidity(); yield();
    result.dht22_temperature = dht.readTemperature(); yield();
    if(isnan(result.dht22_humidity)||isnan(result.dht22_temperature))
        module_panic(dht22_error);

    const char* bmp180_error = "Error reading BMP180";
    result.bmp180_pressure = bmp.readPressure(); yield();
    result.bmp180_temperature = bmp.readTemperature(); yield();
    if(isnan(result.bmp180_temperature))
        module_panic(bmp180_error);

    result.rssi = WiFi.RSSI(); yield();

    return result;
}

void print_measures(Measures m) {
    Serial.print("DHT22 humidity: ");
    Serial.println(m.dht22_humidity);
    Serial.print("DHT22 temperature: ");
    Serial.println(m.dht22_temperature);
    Serial.print("BMP180 pressure: ");
    Serial.println(m.bmp180_pressure);
    Serial.print("BMP180 temperature: ");
    Serial.println(m.bmp180_temperature);
    Serial.print("RSSI: ");
    Serial.println(m.rssi);
}

void assert(int err, const char* name) {
    if (err <= 0)
      module_panic(name);
}

void reboot_module() {
    Serial.println("Rebooting module...");
    delay(STANDART_SHORT_DELAY);
    ESP.restart();
}

void connect_wifi() {
    WiFiMulti.addAP(ssid, wpa_key);

    Serial.print("\n");
    Serial.print("\n");
    Serial.print("Wait for WiFi...\n");

    int attempt = 0;
    char error[] = "Wifi connection failed";
    while(WiFiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(STANDART_SHORT_DELAY);
        attempt++;
        if (attempt > WIFI_MAX_ATTEMPTS)
            module_panic(error);
    }

    Serial.print("\n");
    Serial.print("WiFi connected\n");
    Serial.print("IP address\n");
    Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(115200);
    delay(STANDART_SHORT_DELAY);

    connect_wifi(); yield();

    //Init DHT22
    dht.begin(); yield();

    //Init I2C
    Wire.begin(2,0); yield();
    delay(STANDART_SHORT_DELAY);

    //Init BMP180
    char error[] = "Initialize BMP180";
    assert(bmp.begin(),error); yield();

    //Init MQTT client
    mqtt.setServer(mqtt_server, mqtt_port);
    reconnect_mqtt(); yield();

    //After-setup delay
    delay(STANDART_SHORT_DELAY);
}

void reconnect_mqtt() {
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");

    if (mqtt.connect("ESP8266Client")) {
      Serial.println("MQTT connected");
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
    Measures m = do_measures(); yield();

    print_measures(m);
    delay(STANDART_SHORT_DELAY);

    loop_num++;
    if (loop_num >= REBOOT_LOOPS)
        reboot_module();

    if (!mqtt.connected()) {
      reconnect_mqtt();
    }
    mqtt.loop();

    Serial.print("wait...\n");
    delay(LOOP_DELAY_MS);
}
