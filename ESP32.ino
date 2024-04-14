#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>

BLEScan* pBLEScan;
std::string esp32Address;

// WiFi
const char *ssid = "TROJAN.EXE";
const char *password = "26131011";

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";
const char *topic = "emqx/esp32";
const char *mqtt_username = "gustavo";
const char *mqtt_password = "senha123";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
int total = 0;
int qtd = 0;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        std::string deviceAddress = advertisedDevice.getAddress().toString();

        if (deviceAddress == esp32Address) {
            return;
        }

        int rssi = advertisedDevice.getRSSI();
        double distance = calculateDistance(rssi);

        String message = "Dispositivo encontrado: ";
        message += advertisedDevice.toString().c_str();
        message += "\nRSSI: ";
        message += String(rssi);
        message += "\nDistância estimada: ";
        message += String(distance);
        message += " metros\n";

        total+= rssi;
        qtd++;
        message += "Média: ";
        message += (total/qtd);
        message += "\n\n";

        client.publish(topic, message.c_str());
    }

    double calculateDistance(int rssi) {
        int txPower = -80;

        if (rssi == 0) {
            return -1.0;
        }

        double ratio = rssi * 1.0 / txPower;
        if (ratio < 1.0) {
            return pow(ratio, 10);
        } else {
            double distance = (0.89976) * pow(ratio, 7.7095) + 0.111;
            return distance;
        }
    }
};

void setup() {
    Serial.begin(115200);

    BLEDevice::init("");

    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    esp32Address = BLEDevice::getAddress().toString();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    client.setServer(mqtt_broker, mqtt_port);
    while (!client.connected()) {
        String client_id = "esp32-client-" + WiFi.macAddress();
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
        } else {
            delay(2000);
        }
    }
}

void loop() {
    client.loop();

    BLEScanResults foundDevices = pBLEScan->start(5, false);
    Serial.println("Escaneamento concluído");
    pBLEScan->clearResults();
    delay(2000);

    int numDevices = foundDevices.getCount();
    String message = "Número de dispositivos Bluetooth próximos: ";
    message += String(numDevices);
    client.publish(topic, message.c_str());
}