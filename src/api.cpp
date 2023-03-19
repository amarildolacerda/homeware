#include <api.h>

ApiDriverPair apiDriverList[32];
int numClouds = 0;

ApiDrivers apiDrivers;
ApiDrivers getApiDrivers()
{
    return apiDrivers;
}

void registerApiDriver(String sensorType, ApiDriver *(*create)())
{
    apiDriverList[numClouds] = {sensorType, create};
    numClouds++;
}
ApiDriver *createApiDriver(const String sensorType, const int pin)
{
    for (ApiDriverPair drvCloud : apiDriverList)
        if (drvCloud.sensorType == sensorType)
        {
            ApiDriver *drv = drvCloud.create();
            if (drv)
            {
                drv->sensorType = sensorType;
                drv->pin = pin;
                drv->beforeSetup();
                return drv;
            }
        }
    return nullptr;
}

int apiDriversCount = 0;

void ApiDrivers::add(ApiDriver *driver)
{
    Serial.print("guardando instância sensor na pilha: ");
    Serial.println(apiDriversCount);
    items[apiDriversCount++] = driver;
}
