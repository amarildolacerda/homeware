
#pragma once

#include <cloud.h>

#ifdef SINRICPRO
#include <cloud/sinric.h>
#endif

void register_cloud_setup()
{
#ifdef SINRICPRO
    registerSinricApi();
#endif
}