#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>
#include <vector>
#include <map>

BLEScan* pBLEScan; // Objeto para escanear dispositivos BLE
std::string esp32Address; // Endereço MAC do dispositivo ESP32
const char *ssid = "NOME_DA_REDE"; // Nome da rede WiFi
const char *password = "SENHA_DA_REDE"; // Senha da rede WiFi

const char *mqtt_broker = "broker.emqx.io"; // Endereço IP do broker MQTT
const char *topic_device_info = "emqx/info"; // Tópico para informações de dispositivo
const char *topic_device_quantity = "emqx/qtd"; // Tópico para quantidade de dispositivos
const char *mqtt_username = "NOME_DO_USUARIO"; // Nome de usuário do MQTT
const char *mqtt_password = "SENHA_DO_USUARIO"; // Senha do MQTT
const int mqtt_port = 1883; // Porta do broker MQTT

// RSSI settings
int rssi_ref = -69; // RSSI de referência para 1 metro de distância
double N = 3; // Exponente de atenuação do sinal
const int BUFFER_SIZE = 5; // Tamanho máximo do buffer de RSSI

WiFiClient espClient; // Cliente WiFi
PubSubClient client(espClient); // Cliente MQTT

class DeviceData {
public:
    std::string address; // Endereço MAC do dispositivo BLE
    std::vector<int> rssiBuffer; // Buffer para armazenar valores de RSSI

    DeviceData() {}

    DeviceData(std::string address) : address(address) {} // Construtor com endereço MAC

    void addRSSI(int rssi) { // Método para adicionar valor de RSSI ao buffer
        rssiBuffer.push_back(rssi);
        if (rssiBuffer.size() > BUFFER_SIZE) { // Verifica se o buffer excedeu o tamanho máximo, se sim, remove mais antigo
            rssiBuffer.erase(rssiBuffer.begin());
        }
    }

    float getAverageRSSI() { // Método para calcular a média dos valores de RSSI no buffer
        if (rssiBuffer.empty()) { // Verifica se o buffer está vazio
            Serial.println("Buffer de RSSI está vazio.");
            return 0;
        }
        
        float sum = 0;
        Serial.print("Buffer de RSSI (tamanho ");
        Serial.print(rssiBuffer.size());
        Serial.println("):");

        for (int i = 0; i < rssiBuffer.size(); ++i) { // Itera sobre os valores no buffer
            Serial.print(rssiBuffer[i]);
            Serial.print(" ");
            sum += rssiBuffer[i]; // Adiciona o valor ao total da soma
        }
        Serial.println();

        float average = sum / rssiBuffer.size(); // Calcula a média dos valores no buffer
        Serial.print("Soma dos RSSI: ");
        Serial.println(sum);
        Serial.print("Média dos RSSI: ");
        Serial.println(average);
        
        return average; // Retorna a média dos valores de RSSI
    }

    void printRSSIBuffer() { // Método para imprimir o buffer de RSSI de cada dispositivo
        Serial.print("\nEndereço: ");
        Serial.println(address.c_str());
        Serial.println("Lista de RSSI:");
        for (int i = 0; i < rssiBuffer.size(); ++i) {
            Serial.print(rssiBuffer[i]);
            Serial.print(" ");
        }
        Serial.println();
    }

    double calculateDistance(int rssi) { // Método para calcular a distância com base no RSSI
        if (rssi == 0) {
            return -1.0;
        }
        double distance = pow(10, ((rssi_ref - rssi) / (10.0 * N))); // Calcula a distância usando a fórmula de atenuação do sinal
        return distance; // Retorna a distância calculada
    }
};

std::map<std::string, DeviceData> deviceMap; // Mapa para armazenar dados do dispositivo

class MyDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) { // Método chamado quando um dispositivo é detectado
        int rssi = advertisedDevice.getRSSI(); // Obtém o RSSI do dispositivo
        std::string address = advertisedDevice.getAddress().toString(); // Obtém o endereço MAC do dispositivo

        if (deviceMap.find(address) == deviceMap.end()) { // Verifica se o dispositivo já está no mapa
            deviceMap[address] = DeviceData(address); // Adiciona o dispositivo ao mapa
        }

        DeviceData& deviceData = deviceMap[address]; // Obtém uma referência aos dados do dispositivo
        deviceData.addRSSI(rssi); // Adiciona o RSSI ao buffer de RSSI

        float averageRSSI = deviceData.getAverageRSSI(); // Calcula a média dos valores de RSSI
        double distance = deviceData.calculateDistance(averageRSSI); // Calcula a distância com base no RSSI médio

        deviceData.printRSSIBuffer();

        String message = ""; // Cria uma mensagem para ser enviada ao broker MQTT
        message += "Nome: " + String(advertisedDevice.getName().c_str()) + " | \n";
        message += "Endereço: " + String(address.c_str()) + " | \n";
        message += "RSSI: " + String(averageRSSI) + " | \n";
        message += "Distância: " + String(distance) + " metros\n";

        client.publish(topic_device_info, message.c_str());
    }
};

void setup() {
    Serial.begin(115200); // Inicializa a comunicação serial

    BLEDevice::init(""); // Inicializa o dispositivo BLE

    pBLEScan = BLEDevice::getScan(); // Obtém o objeto de escaneamento BLE
    pBLEScan->setAdvertisedDeviceCallbacks(new MyDeviceCallbacks()); // Define o callback para dispositivos BLE detectados
    pBLEScan->setActiveScan(true); // Ativa o escaneamento
    pBLEScan->setInterval(100); // Define o intervalo do scanner
    pBLEScan->setWindow(99); // Define a janela de escaneamento
    esp32Address = BLEDevice::getAddress().toString(); // Obtém o endereço MAC do dispositivo ESP32

    WiFi.begin(ssid, password); // Conecta-se à rede WiFi especificada
    while (WiFi.status() != WL_CONNECTED) { // Aguarda até que a conexão WiFi seja estabelecida
        delay(500);
        Serial.println("Conectando wifi");
    }

    client.setServer(mqtt_broker, mqtt_port); // Configura o servidor MQTT
    while (!client.connected()) { // Tenta conectar-se ao servidor MQTT
        String client_id = "esp32-client-" + WiFi.macAddress();
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            // Conexão bem-sucedida
        } else {
            delay(2000);
        }
        Serial.println("Conectando broker");
    }
}

void loop() {
    client.loop(); // Mantém a conexão MQTT ativa

    BLEScanResults foundDevices = pBLEScan->start(5, false); // Inicia o escaneamento BLE a cada 5 segundos
    Serial.println("\nEscaneamento concluído");
    pBLEScan->clearResults();
    delay(2000);

    int numDevices = foundDevices.getCount(); // Obtém o número de dispositivos detectados
    String message = String(numDevices) + " dispositivo(s)";
    client.publish(topic_device_quantity, message.c_str());
}
