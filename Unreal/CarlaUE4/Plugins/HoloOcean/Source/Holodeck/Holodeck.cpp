// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"

// OpenHUTB 修改（2026-11-04）：
// 原为 IMPLEMENT_PRIMARY_GAME_MODULE。该宏专供工程的「主游戏模块」使用，
// 会定义 GInternalProjectName / GLiveCodingProject / operator new / delete 等全局符号。
// 本插件作为 Runtime 模块被链接进单体可执行文件时，会与 CarlaUE4 主模块重复定义，
// 导致 LNK2005 / LNK1169。插件应使用 IMPLEMENT_MODULE。
IMPLEMENT_MODULE( FDefaultGameModuleImpl, Holodeck );

DEFINE_LOG_CATEGORY(LogHolodeck);