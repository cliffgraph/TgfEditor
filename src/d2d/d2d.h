#pragma once

#include "d2d1.h"
#include "d2dbasetypes.h"
#include "d2derr.h"

template<class Interface>
inline void D2DSafeRelease( Interface **ppInterfaceToRelease )
{
    if (*ppInterfaceToRelease != nullptr)
    {
        (*ppInterfaceToRelease)->Release();
        (*ppInterfaceToRelease) = nullptr;
    }
}
