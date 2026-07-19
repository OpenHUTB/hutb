// Fill out your copyright notice in the Description page of Project Settings.

#include "CarlaUE4.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, CarlaUE4, "CarlaUE4" );
#ifdef __linux__
#include <stdexcept>
namespace std {
    void __throw_out_of_range_fmt(const char* fmt, ...) {
        // 直接调用底层终止函数，骗过链接器即可，绝不使用 throw
        abort();
    }
}
#endif
DEFINE_LOG_CATEGORY(LogDReyeVR);
