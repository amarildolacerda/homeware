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
    Serial.printf("apidrivers.loop(%i) \r\n", apiDriversCount);
#endif
    for (int i = 0; i < apiDriversCount; i++)
    {
        ApiDriver *drv = apiDriverItems[i];
        if (drv && drv->isLoop())
        {
#ifdef DEBUG_API
            Serial.printf("%s.loop()", drv->sensorType);
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
    ApiDriver *drv = createApiDriver(sensorType, pin);
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
    for (auto *drv : apiDriverItems)
        if (drv)
        {
            if (drv->pin == pin)
            {
                return drv;
            }
        }
    return nullptr;
}

ApiDriver *ApiDrivers::findByType(const String sensorType)
{
    for (auto *drv : apiDriverItems)
        if (drv)
        {
            if (drv->sensorType == sensorType)
            {
                return drv;
            }
        }
    return nullptr;
}

void ApiDrivers::changed(const int pin, const long value)
{
    auto *drv = findByPin(pin);
    if (drv)
        drv->changed(pin, value);
}

void ApiDrivers::afterSetup()
{
    for (auto *drv : apiDriverItems)
        if (drv)
        {
            drv->afterSetup();
        }
}
