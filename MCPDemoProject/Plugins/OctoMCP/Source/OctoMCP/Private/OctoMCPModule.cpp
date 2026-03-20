// Copyright Epic Games, Inc. All Rights Reserved.

#include "OctoMCPModule.h"

DEFINE_LOG_CATEGORY(LogOctoMCP);

void FOctoMCPModule::StartupModule()
{
    CachePluginVersion();
    StartHttpBridge();
}

void FOctoMCPModule::ShutdownModule()
{
    StopHttpBridge();
}

IMPLEMENT_MODULE(FOctoMCPModule, OctoMCP)
