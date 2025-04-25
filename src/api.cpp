#include "api.h"

ApiDriverPair apiDriverList[API_SIZE];
int numClouds = 0;

ApiDrivers apiDrivers;
ApiDrivers getApiDrivers()
{
    return apiDrivers;
}

void registerDriver(const String sensorType, ApiDriver *(*create)(), const bool autoInit)
{
    DEBUGP("registerDriver(%s)\r\n", sensorType.c_str());
    if (numClouds == API_SIZE)
    {
        Protocol::instance()->debugf("Erro: numero maximo de APIs atingido.");
        DEBUGP("Erro: numero maximo de APIs atingido (%i).", numClouds);
        return;
    }
    apiDriverList[numClouds] = {autoInit, sensorType, create};
    DEBUGP("Adicionado ApiDriverList[%i] = %s\r\n", numClouds, sensorType.c_str());
    numClouds++;
    Protocol::instance()->apis += sensorType + ",";
}
ApiDriver *createApiDriver(const String sensorType, const int pin, const bool autoCreate)
{
    DEBUGP("createApiDriver(%s,%i)\r\n", sensorType.c_str(), pin);
    for (ApiDriverPair drvCloud : apiDriverList)
        if (drvCloud.sensorType == sensorType && drvCloud.autoCreate == autoCreate)
        {
            ApiDriver *drv = drvCloud.create();
            if (drv)
            {
                drv->sensorType = sensorType;
                drv->pin = pin;
                drv->afterCreate();
                return drv;
            }
        }
    return nullptr;
}

int apiDriversCount = 0;
ApiDriver *apiDriverItems[API_SIZE];

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
    DEBUGP("ApiDrivers::loop(%i)\r\n", apiDriversCount);
#ifdef DEBUG_API
    Serial.printf("BEGIN: apidrivers.loop(%i) \r\n", apiDriversCount);
#endif
    for (int i = 0; i < apiDriversCount; i++)
    {
        ApiDriver *drv = apiDriverItems[i];
        if (drv)
        {
#ifdef DEBUG_API
            Serial.printf("%s.loop()\r\n", drv->sensorType);
#endif
            drv->loop();
        }
    }
#ifdef DEBUG_API
    Serial.printf("END: apidrivers.loop(%i) \r\n", apiDriversCount);
#endif
}

void ApiDrivers::add(ApiDriver *driver)
{
    DEBUGP("ApiDrivers::add(%s)\r\n", driver->sensorType.c_str());
    apiDriverItems[apiDriversCount++] = driver;
#ifdef DEBUG_API
    Serial.println(apiDriverItems[0]->sensorType);
#endif
}

ApiDriver *ApiDrivers::initPinSensor(const int pin, const String sensorType)
{
    DEBUGP("ApiDrivers::initPinSensor(%i,%s)\r\n", pin, sensorType.c_str());
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
    DEBUGP("ApiDrivers::findByPin(%i)\r\n", pin);
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
    DEBUGP("ApiDrivers::findByType(%s)\r\n", sensorType.c_str());
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

void ApiDrivers::reload()
{
    for (auto *drv : apiDriverItems)
        if (drv)
        {
            drv->reload();
        }
}

void ApiDrivers::afterSetup()
{
#ifdef DEBUG_API
    Serial.printf("BEGIN: afterSetup()");
#endif
    for (auto *drv : apiDriverItems)
        if (drv)
        {
            drv->afterSetup();
        }
#ifdef DEBUG_API
    Serial.printf("END: ApiDrivers::afterSetup()\r\n");
#endif
}

void ApiDrivers::afterCreate()
{
    int id = 0;
    // criar instancia que requer autocreate
    for (ApiDriverPair drvPair : apiDriverList)
    {
        if (drvPair.autoCreate)
        {
            ApiDriver *drv = createApiDriver(drvPair.sensorType, --id, true);
            if (drv)
            {
                drv->setup();
                add(drv);
            }
        }
    }
}
