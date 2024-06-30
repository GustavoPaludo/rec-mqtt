#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>
#include <vector>
#include <map>

// BLE and WiFi settings
BLEScan* pBLEScan;
std::string esp32Address;
const char *ssid = "SSID_NETWORK";
const char *password = "PASSWORD_NETWORK";

// MQTT Broker settings
const char *mqtt_broker = "broker.emqx.io";
const char *topic_n_value = "emqx/nvalue";

const char *topic_device_info = "emqx/info";
const char *topic_device_extra = "emqx/extra";
const char *topic_device_quantity = "emqx/qtd";
const char *topic_device_filter = "emqx/device";
const char *topic_broker_ip = "emqx/brokerip";
const char *topic_esp_ip = "emqx/espip";

const char *mqtt_username = "gustavo";
const char *mqtt_password = "senha123";
const int mqtt_port = 1883;

// RSSI settings
int rssi_ref = -69; // RSSI de referência para 1 metro de distância
double N = 2; // Exponente de atenuação do sinal
const int BUFFER_SIZE = 5; // Tamanho máximo do buffer de RSSI

WiFiClient espClient;
PubSubClient client(espClient);

std::string filterAddress = ""; // Endereço do dispositivo a ser filtrado

class DeviceData {
public:
    std::string address;
    std::vector<int> rssiBuffer;

    DeviceData() {}

    DeviceData(std::string address) : address(address) {}

    void addRSSI(int rssi) {
        rssiBuffer.push_back(rssi);
        if (rssiBuffer.size() > BUFFER_SIZE) {
            rssiBuffer.erase(rssiBuffer.begin());
        }
    }

    float getAverageRSSI() {
        if (rssiBuffer.empty()) {
            Serial.println("Buffer de RSSI está vazio.");
            return 0;
        }
        
        float sum = 0;
        Serial.print("Buffer de RSSI (tamanho ");
        Serial.print(rssiBuffer.size());
        Serial.println("):");

        for (int i = 0; i < rssiBuffer.size(); ++i) {
            Serial.print(rssiBuffer[i]);
            Serial.print(" ");
            sum += rssiBuffer[i];
        }
        Serial.println();

        float average = sum / rssiBuffer.size();
        Serial.print("Soma dos RSSI: ");
        Serial.println(sum);
        Serial.print("Média dos RSSI: ");
        Serial.println(average);
        
        return average;
    }

    void printRSSIBuffer() {
        Serial.print("\nEndereço: ");
        Serial.println(address.c_str());
        Serial.println("Lista de RSSI:");
        for (int i = 0; i < rssiBuffer.size(); ++i) {
            Serial.print(rssiBuffer[i]);
            Serial.print(" ");
        }
        Serial.println();
    }

    double calculateDistance(int rssi) {
        if (rssi == 0) {
            return -1.0;
        }
        double distance = pow(10, ((rssi_ref - rssi) / (10.0 * N)));
        return distance;
    }
};

std::map<std::string, DeviceData> deviceMap;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        int rssi = advertisedDevice.getRSSI();
        std::string address = advertisedDevice.getAddress().toString();

        if (deviceMap.find(address) == deviceMap.end()) {
            deviceMap[address] = DeviceData(address);
        }

        DeviceData& deviceData = deviceMap[address];
        deviceData.addRSSI(rssi);

        int averageRSSI = deviceData.getAverageRSSI();
        double distance = deviceData.calculateDistance(averageRSSI);

        deviceData.printRSSIBuffer();

        if (filterAddress.empty() || address.find(filterAddress) != std::string::npos) {
            String message = "";
            message += "Nome: " + String(advertisedDevice.getName().c_str()) + " | \n";
            message += "Endereço: " + String(address.c_str()) + " | \n";
            message += "RSSI: " + String(averageRSSI) + " | \n";
            message += "Distância: " + String(distance) + " metros\n";

            client.publish(topic_device_info, message.c_str());
        }
    }
};

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Mensagem recebida no tópico [");
    Serial.print(topic);
    Serial.print("]: ");
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println(message);

    if (String(topic) == topic_device_filter) {
        filterAddress = message.c_str();
        Serial.print("Filtro atualizado para: ");
        Serial.println(filterAddress.c_str());
    } else if (String(topic) == topic_n_value) {
        N = message.toDouble();
        Serial.print("N alterado para: ");
        Serial.println(N);
    }
}

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
        Serial.println("Conectando wifi");
    }

    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(mqttCallback);
    while (!client.connected()) {
        String client_id = "esp32-client-" + WiFi.macAddress();
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            client.subscribe(topic_device_filter);
            client.subscribe(topic_n_value);
        } else {
            delay(2000);
        }
        Serial.println("Conectando broker");
    }
}

void loop() {
    client.loop();

    BLEScanResults foundDevices = pBLEScan->start(5, false);
    Serial.println("\nEscaneamento concluído");
    pBLEScan->clearResults();
    delay(2000);

    int numDevices = foundDevices.getCount();
    String message = String(numDevices) + " dispositivo(s)";
    client.publish(topic_device_quantity, message.c_str());

    String espIp = WiFi.localIP().toString();
    client.publish(topic_esp_ip, espIp.c_str());

    String brokerIp = mqtt_broker;
    client.publish(topic_broker_ip, brokerIp.c_str());
}
