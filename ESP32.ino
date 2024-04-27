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
const char *ssid = "REDE-WIFI";
const char *password = "SENHA-WIFI";

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";
const char *topic = "topico";
const char *mqtt_username = "usuario";
const char *mqtt_password = "senha";
const int mqtt_port = 1883;

int rssi_ref = -77; // RSSI de referência para 1 metro de distância
double N = 1.5; // Exponente de atenuação do sinal

WiFiClient espClient;
PubSubClient client(espClient);

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        std::string deviceAddress = advertisedDevice.getAddress().toString();

        if (deviceAddress == esp32Address) {
            return;
        }

        int rssi = advertisedDevice.getRSSI();
        double distance = calculateDistance(rssi);
        int expected_rssi = calculateRSSI(distance);

        String message = "Dispositivo encontrado: ";
        message += advertisedDevice.toString().c_str();
        message += "\nRSSI: ";
        message += String(rssi);
        message += "\nDistância estimada: ";
        message += String(distance);
        message += " metros\n";
        message += "RSSI esperado para (d): ";
        message += String(expected_rssi);

        client.publish(topic, message.c_str());
    }

    double calculateDistance(int rssi) {
        if (rssi == 0) {
            return -1.0; // Valor inválido
        }

        double distance = pow(10, ((rssi_ref - rssi) / (10.0 * N)));
        return distance;
    }

    int calculateRSSI(double distance) {
        int expected_rssi = rssi_ref - (10 * N * log10(distance));
        return expected_rssi;
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
