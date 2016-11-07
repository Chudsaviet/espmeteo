
#include "main.h"
#include "private.h"

ESP8266WiFiMulti WiFiMulti;
WiFiUDP Udp;
DHT dht(DHTPIN, DHTTYPE, 11);
Adafruit_BMP085 bmp;
uint loop_num = 0;
uint32_t ESP_CHIP_ID = ESP.getChipId();

char influxdb_line [INFLUXDB_LINE_MAX_SIZE];

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

int build_influxdb_line(char* buffer, size_t len, const char* sensor, const char* measure, char* value) {
    int result = snprintf(
        buffer,
        len,
        "%s,platform=esp8266,device=espmeteo,id=%X,sensor=%s value=%s",
        measure,
        ESP_CHIP_ID,
        sensor,
        value
    );
    yield();

    return result;
}

int build_influxdb_line(char* buffer, size_t len, const char* sensor, const char* measure, float value) {
    char value_chars [FLOAT_MAX_CHARS];

    dtostrf(value,5,2,value_chars); yield();

    int result = build_influxdb_line(
        buffer,
        len,
        sensor,
        measure,
        value_chars
    );
    yield();

    return result;
}

int build_influxdb_line(char* buffer, size_t len, const char* sensor, const char* measure, int value) {
    char value_chars [INT_MAX_CHARS];

    const char error[] = "Error converting int to char* in build_influxdb_line";
    int bytes = snprintf(value_chars,FLOAT_MAX_CHARS,"%d",value);
    if (bytes <= 0)
        module_panic(error);

    int result = build_influxdb_line(
        buffer,
        len,
        sensor,
        measure,
        value_chars
    );
    yield();

    return result;
}

void report_to_influxdb(Measures m) {
    const char temperature[] = "temperature";
    const char humidity[] = "humidity";
    const char pressure[] = "pressure";
    const char rssi[] = "rssi";
    const char dht22_name[] = "dht22";
    const char bmp180_name[] = "bmp180";
    const char esp8266_name[] = "esp8266";

    //report DHT22 temperature
    build_influxdb_line(
      influxdb_line,
      INFLUXDB_LINE_MAX_SIZE,
      dht22_name,
      temperature,
      m.dht22_temperature);
    yield();
    send_to_influxdb(influxdb_line); yield();

    //report DHT22 humidity
    build_influxdb_line(
      influxdb_line,
      INFLUXDB_LINE_MAX_SIZE,
      dht22_name,
      humidity,
      m.dht22_humidity);
    yield();
    send_to_influxdb(influxdb_line); yield();

    //report BMP180 temperature
    build_influxdb_line(
      influxdb_line,
      INFLUXDB_LINE_MAX_SIZE,
      bmp180_name,
      temperature,
      m.bmp180_temperature);
    yield();
    send_to_influxdb(influxdb_line); yield();

    //report BMP180 pressure
    build_influxdb_line(
      influxdb_line,
      INFLUXDB_LINE_MAX_SIZE,
      bmp180_name,
      pressure,
      m.bmp180_pressure);
    yield();
    send_to_influxdb(influxdb_line); yield();

    //report WiFi RSSI
    build_influxdb_line(
      influxdb_line,
      INFLUXDB_LINE_MAX_SIZE,
      esp8266_name,
      rssi,
      m.rssi);
    yield();
    send_to_influxdb(influxdb_line); yield();
}

void send_to_influxdb(char* line) {
    const char* action = "Send line to InfluxDB";
    Serial.println(action);
    Serial.println(line);
    assert(Udp.beginPacket(influxdb_ip, influxdb_port),action);
    assert(Udp.write(line),action);
    assert(Udp.endPacket(),action);
    yield();
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

    //After-setup delay
    delay(STANDART_SHORT_DELAY);
}

void loop() {
    Measures m = do_measures(); yield();

    print_measures(m);
    delay(STANDART_SHORT_DELAY);

    report_to_influxdb(m); yield();

    loop_num++;
    if (loop_num >= REBOOT_LOOPS)
        reboot_module();

    Serial.print("wait...\n");
    delay(LOOP_DELAY_MS);
}
