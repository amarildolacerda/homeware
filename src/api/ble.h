#pragma once

#include <BLEDevice.h>
#include "api.h"

// TODO: nao concluido teste nem implementacao

class BLEServer : public ApiDriver
{
private:
    BLEServer *pServer;
    BLECharacteristic *pCharacteristic;
    std::string response;
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

public:
    static void registerApi()
    {
        registerDriver("ble", create);
    }
    static ApiDriver *create()
    {
        return new BLEController(SERVICE_UUID, CHARACTERISTIC_UUID);
    }

    BLEController(const char *serviceUUID, const char *characteristicUUID)
    {
        BLEDevice::init("BLE Server");
        pServer = BLEDevice::createServer();
        BLEService *pService = pServer->createService(serviceUUID);
        pCharacteristic = pService->createCharacteristic(
            characteristicUUID,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
        pCharacteristic->setCallbacks(new BLECallbacks(this));
        pService->start();
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(pService->getUUID());
        pAdvertising->start();
    }

    class BLECallbacks : public BLECharacteristicCallbacks
    {
    private:
        BLEController *pController;

    public:
        BLECallbacks(BLEController *pController)
        {
            this->pController = pController;
        }

        void onWrite(BLECharacteristic *pCharacteristic)
        {
            std::string command = pCharacteristic->getValue();
            pController->response = getInstanceOfProtocol()->doCommand(command.c_str());
        }
    };

    void waitForConnection()
    {
        while (!pServer->getConnectedCount())
        {
            delay(10);
        }
    }

    std::string getResponse()
    {
        return response;
    }
};
