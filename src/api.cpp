#include "api.h"

ApiDriverPair apiDriverList[32];
int numClouds = 0;

ApiDrivers apiDrivers;
ApiDrivers getApiDrivers()
{
    return apiDrivers;
}

void registerApiDriver(const String sensorType, ApiDriver *(*create)(), const bool autoInit)
{
    apiDriverList[numClouds] = {autoInit, sensorType, create};
    numClouds++;
    getInstanceOfProtocol()->apis += sensorType + ",";
}
ApiDriver *createApiDriver(const String sensorType, const int pin, const bool autoCreate)
{
    for (ApiDriverPair drvCloud : apiDriverList)
        if (drvCloud.sensorType == sensorType && drvCloud.autoCreate == autoCreate)
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
ApiDriver *apiDriverItems[32];

int ApiDrivers::count()
{
    return apiDriversCount;
}
ApiDriver *ApiDrivers::getItem(int index)
{
    return apiDriverItems[index];
}
void ApiDrivers::loop()
{
#ifdef DEBUG_API
    Serial.printf("BEGIN: apidrivers.loop(%i) \r\n", apiDriversCount);
#endif
    for (int i = 0; i < apiDriversCount; i++)
    {
        ApiDriver *drv = apiDriverItems[i];
        if (drv && drv->isLoop())
        {
#ifdef DEBUG_API
            Serial.printf("%s.loop()\r\n", drv->sensorType);
#endif
            drv->loop();
        }
        else
        {
#ifdef DEBUG_API
            Serial.println("ApiDrivers null");
            return;
#endif
        }
    }
#ifdef DEBUG_API
    Serial.printf("END: apidrivers.loop(%i) \r\n", apiDriversCount);
#endif
}

void ApiDrivers::add(ApiDriver *driver)
{
    apiDriverItems[apiDriversCount++] = driver;
#ifdef DEBUG_API
    Serial.println(apiDriverItems[0]->sensorType);
#endif
}

ApiDriver *ApiDrivers::initPinSensor(const int pin, const String sensorType)
{
    ApiDriver *drv = createApiDriver(sensorType, pin, false);
    if (drv)
    {
        drv->setup();
#ifdef DEBUG_API
        Serial.println(drv->toString());
#endif
        add(drv);
#ifdef DEBUG_API
        Serial.printf("Criou link cloud: %s em %i \r\n", sensorType, pin);
#endif
        return drv;
    }
    return nullptr;
}

ApiDriver *ApiDrivers::findByPin(const int pin)
{
#ifdef DEBUG_API
    Serial.printf("BEGIN: ApiDrivers::findByPin(%i)", pin);
#endif

    for (auto *drv : apiDriverItems)
        if (drv)
        {
            if (drv->pin == pin)
            {
#ifdef DEBUG_API
                Serial.printf("END: ApiDrivers::findByPin\r\n");
#endif

                return drv;
            }
        }
    return nullptr;
}

ApiDriver *ApiDrivers::findByType(const String sensorType)
{
#ifdef DEBUG_API
    Serial.printf("BEGIN: ApiDrivers::findByType(%s)", sensorType);
#endif
    for (auto *drv : apiDriverItems)
        if (drv)
        {
            if (drv->sensorType == sensorType)
            {
#ifdef DEBUG_API
                Serial.printf("END: ApiDrivers::findByType\r\n");
#endif
                return drv;
            }
        }

    return nullptr;
}

void ApiDrivers::changed(const int pin, const long value)
{
#ifdef DEBUG_API
    Serial.printf("BEGIN: ApiDrivers::changed(%i,%i)", pin, value);
#endif
    auto *drv = findByPin(pin);
    if (drv)
        drv->changed(pin, value);
#ifdef DEBUG_API
    Serial.printf("END: ApiDrivers::changed(%i,%i)\r\n", pin, value);
#endif
}

void ApiDrivers::afterSetup()
{
#ifdef DEBUG_API
    Serial.printf("BEGIN: afterSetup()");
#endif
    int id = 0;
    // criar instancia que requer autocreate
    for (ApiDriverPair drv : apiDriverList)
    {
        if (drv.autoCreate)
            createApiDriver(drv.sensorType, --id, true);
    }

    for (auto *drv : apiDriverItems)
        if (drv)
        {
            drv->afterSetup();
        }
#ifdef DEBUG_API
    Serial.printf("END: ApiDrivers::afterSetup()\r\n");
#endif
}
