
#include "main.h"
#include "private.h"

ESP8266WiFiMulti WiFiMulti;
WiFiUDP Udp;
DHT dht(DHTPIN, DHTTYPE, 11);
Adafruit_BMP085 bmp;
int loop_num = 0;

char influxdb_line [INFLUXDB_LINE_MAX_SIZE];

void module_panic(String message) {
    Serial.println("PANIC: "+message);
    delay(500);
    reboot_module();
}

measures do_measures() {
    measures result;

    result.dht22_humidity = dht.readHumidity();
    result.dht22_temperature = dht.readTemperature();
    if(isnan(result.dht22_humidity)||isnan(result.dht22_temperature))
        module_panic("Error reading DHT22");
    delay(500);

    result.bmp180_pressure = bmp.readPressure();
    result.bmp180_temperature = bmp.readTemperature();
    if(isnan(result.bmp180_temperature))
        module_panic("Error reading BMP180");
    delay(500);

    result.rssi = WiFi.RSSI();

    return result;
}

void print_measures(measures m) {
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

char* build_influxdb_line(measures m) {
    char dht22_temperature[10];
    char dht22_humidity[10];
    char bmp180_temperature[10];

    dtostrf(m.dht22_temperature,5,2,dht22_temperature);
    dtostrf(m.dht22_humidity,5,2,dht22_humidity);
    dtostrf(m.bmp180_temperature,5,2,bmp180_temperature);

    int length = snprintf(influxdb_line,
        INFLUXDB_LINE_MAX_SIZE,
        "device=ESP8266%X dht22_temperature=%s,dht22_humuidity=%s,bmp180_pressure=%d,bmp180_temperature=%s,rssi=%d",
        ESP.getChipId(),
        dht22_temperature,
        dht22_humidity,
        m.bmp180_pressure,
        bmp180_temperature,
        m.rssi
    );

    return influxdb_line;
}

bool report_to_influxdb(measures m) {
    char* line = build_influxdb_line(m);

    String action = "Send packet to InfluxDB";
    assert(Udp.beginPacket(influxdb_ip, influxdb_port),action);
    assert(Udp.write(line),action);
    assert(Udp.endPacket(),action);
}

void assert(int err, String name) {
    if (err <= 0)
      module_panic(name + " failed");
}

void reboot_module() {
    Serial.println("Rebooting module...");
    ESP.restart();
}

void connect_wifi() {
    WiFiMulti.addAP(ssid, wpa_key);

    Serial.print("\n");
    Serial.print("\n");
    Serial.print("Wait for WiFi...\n");

    int attempt = 0;
    while(WiFiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
        attempt++;
        if (attempt > 120)
            module_panic("Wifi connection failed");
    }

    Serial.print("\n");
    Serial.print("WiFi connected\n");
    Serial.print("IP address\n");
    Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(115200);
    delay(10);

    connect_wifi();
    delay(500);

    dht.begin();
    delay(500);

    //Init I2C
    Wire.begin(2,0);
    delay(500);
    assert(bmp.begin(),"Initialize BMP180");
    delay(500);
}


void loop() {
    measures m = do_measures();
    delay(500);
    print_measures(m);
    report_to_influxdb(m);
    delay(500);

    loop_num++;
    if (loop_num >= REBOOT_LOOPS)
        reboot_module();

    Serial.print("wait...\n");
    delay(LOOP_DELAY_MS);
}
