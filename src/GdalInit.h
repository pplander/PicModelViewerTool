#pragma once
#include <gdal_priv.h>

inline void ensureGdalRegistered() {
    static bool registered = false;
    if (!registered) {
        GDALAllRegister();
        registered = true;
    }
}
