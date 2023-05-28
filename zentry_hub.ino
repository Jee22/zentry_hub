/**
 * A BLE client example that is rich in capabilities.
 * There is a lot new capabilities implemented.
 * author unknown
 * updated by chegewara
 */

#include <WiFi.h>
#include <MQTT.h>



#define ROUTER_NAME "ESP32"
const char ssid[] = "ssid";
const char pass[] = "pass";

WiFiClient net;
MQTTClient mqttClient;

unsigned long lastMillis = 0;


void connect() {
    Serial.print("checking wifi...");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }

    Serial.print("\nconnecting...");
    while (!mqttClient.connect("arduino", "public", "public")) {
        Serial.print(".");
        delay(1000);
    }

    Serial.println("\nconnected!");

    mqttClient.subscribe("/hello");

    const String topic = "router/connect/";
    const String routerMacAddress = WiFi.macAddress();
    mqttClient.publish((topic + routerMacAddress).c_str(), ROUTER_NAME);
    // client.unsubscribe("/hello");
}


void messageReceived(String& topic, String& payload) {
    Serial.println("incoming: " + topic + " - " + payload);

    // Note: Do not use the client in the callback to publish, subscribe or
    // unsubscribe as it may cause deadlocks when other things arrive while
    // sending and receiving acknowledgments. Instead, change a global variable,
    // or push to a queue and handle it in the loop after calling `client.loop()`.
}


void mqttSetup() {
    WiFi.begin(ssid, pass);

    // Note: Local domain names (e.g. "Computer.local" on OSX) are not supported
    // by Arduino. You need to set the IP address directly.
    mqttClient.begin("43.200.127.148", net);
    mqttClient.onMessage(messageReceived);

    connect();
}


#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>



#define MAX_DEVICES 10 // change this value based on your requirement

// The remote service we wish to connect to.
static BLEUUID serviceUUID("EF687700-9B35-4933-9B10-52FFA9740042");
// The characteristic of the remote service we are interested in.
static BLEUUID charUUID("EF687777-9B35-4933-9B10-52FFA9740042");

static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* registeredDevices[MAX_DEVICES];

static bool isValidDevice(BLEAdvertisedDevice* pAdvertisedDevice);

static bool isDeviceAlreadyRegistered(BLEAdvertisedDevice* pAdvertisedDevice);

BLEClient* pClient;

int deviceCount = 0;
BLEScan* pBLEScan;

/** Todo: Device struct */
String connectedDevices[MAX_DEVICES];
String deviceNames[MAX_DEVICES];
String deviceAddresses[MAX_DEVICES];

class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient* client) {
        Serial.println("Connected@@@@@@@@@");
    }


    void onDisconnect(BLEClient* client) {
        Serial.println("Disconnected from BLE Server.");
    }
};


static bool isValidDevice(BLEAdvertisedDevice* pAdvertisedDevice) {
    return pAdvertisedDevice->getName() == "Zentry_air";
//    return haveServiceUUID() &&
//    pAdvertisedDevice->isAdvertisingService(serviceUUID);
}


static bool isDeviceAlreadyRegistered(BLEAdvertisedDevice* pAdvertisedDevice) {
    for (int i = 0; i < MAX_DEVICES; i++) {
//        if (pAdvertisedDevice->getAddress().toString() == deviceAddresses[i].c_str()) {
        if (pAdvertisedDevice->getAddress().toString() == registeredDevices[i]->getAddress().toString()) {

            Serial.println("Device already registered");
            return true;
        }
    }
    return false;
}


class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        // We have found a device, let us now see if it contains the service we are looking for.
        if (deviceCount < MAX_DEVICES) {
            BLEAdvertisedDevice* pAdvertisedDevice = new BLEAdvertisedDevice(advertisedDevice);

            if (isValidDevice(pAdvertisedDevice)) {
                Serial.println("Found TARGET device. MyAdvertisedDeviceCallbacks ...");
                Serial.println(advertisedDevice.getAddress().toString().c_str());

                //check deviceAddresses array and if it does not contain the address, add it
                if (isDeviceAlreadyRegistered(pAdvertisedDevice)) {
                    return;
                }

                deviceAddresses[deviceCount] = advertisedDevice.getAddress().toString().c_str();
                deviceNames[deviceCount] = advertisedDevice.getName().c_str();
                registeredDevices[deviceCount] = pAdvertisedDevice;
                deviceCount++;
            }
        }
    } // onResult
}; // MyAdvertisedDeviceCallbacks


void setup() {
    Serial.begin(115200);
    Serial.println("Starting Arduino BLE Client application...");
    mqttSetup();


    BLEDevice::init(ROUTER_NAME);
    // // Retrieve a Scanner and set the callback we want to use to be informed when we
    // // have detected a new device.  Specify that we want active scanning and start the
    // // scan to run for 5 seconds.
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);

    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());


} // End of setup.

void scanDevices() {
    if (pClient->isConnected()) {
        pClient->disconnect();
    }

    Serial.println("Start scanning");
    pBLEScan->start(5, false);
    Serial.println("Scan done!");

}


void selectDevice(int deviceNumber) {
    BLEDevice::getScan()->stop();

    if (pClient->isConnected()) {
        pClient->disconnect();
    }

    pClient->connect(registeredDevices[deviceNumber]);
    if (!pClient->isConnected()) {
        return;
    }

    Serial.print("MAC: ");
    Serial.println(pClient->getPeerAddress().toString().c_str());

    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        pClient->disconnect();
        return;
    }

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
        pClient->disconnect();
        return;
    }

    Serial.println(" - Found our characteristic");
    // Read the value of the characteristic.
    if (pRemoteCharacteristic->canRead()) {
        float temperature = pRemoteCharacteristic->readFloat();
        Serial.print("The characteristic value was: ");
        Serial.printf("[%f]\r\n", temperature);

        auto sensorAddress = pClient->getPeerAddress().toString().c_str();
        auto topic = (String("sensor/temperature/") + sensorAddress).c_str();

        char payload[10];
        dtostrf(temperature, 6, 2, payload); // parse float to string

        mqttClient.publish(topic, payload);
    }


    pClient->disconnect();


}


void measurementTemperatures() {
    for (int i = 0; i < deviceCount; i++) {
        Serial.print("Select device: ");
        Serial.println(i);
        selectDevice(i);
    }
}


// This is the Arduino main loop function.
void loop() {

    mqttClient.loop();
    delay(10);  // <- fixes some issues with WiFi stability

    if (!mqttClient.connected()) {
        connect();
    }

    if (millis() - lastMillis > 3000) {
        lastMillis = millis();
        mqttClient.publish("hello/world", "hi");
    }

    Serial.println("Start scanning in loop...");
    scanDevices();
    measurementTemperatures();

//    delay(3000); // Delay a second between loops.
    Serial.println("End of loop");
} // End of loop

