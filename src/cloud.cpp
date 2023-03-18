#include <cloud.h>

CloudDriverPair cloudDriverList[32];
int numClouds = 0;

CloudDrivers cloudDrivers;
CloudDrivers getCloudDrivers()
{
    return cloudDrivers;
}

void registerCloudDriver(String sensorType, CloudDriver *(*create)())
{
    cloudDriverList[numClouds] = {sensorType, create};
    numClouds++;
}
CloudDriver *createCloudDriver(const String sensorType, const int pin)
{
    for (CloudDriverPair drvCloud : cloudDriverList)
        if (drvCloud.sensorType == sensorType)
        {
            CloudDriver *drv = drvCloud.create();
            if (drv)
            {
                drv->sensorType = sensorType;
                drv->pin = pin;
                return drv;
            }
        }
    return nullptr;
}

int cloudDriversCount = 0;

void CloudDrivers::add(CloudDriver *driver)
{
    Serial.print("guardando instância sensor na pilha: ");
    Serial.println(cloudDriversCount);
    items[cloudDriversCount++] = driver;
}
