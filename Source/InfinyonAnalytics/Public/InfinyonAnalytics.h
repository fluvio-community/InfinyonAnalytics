// Copyright Infinyon.com  2025 All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IAnalyticsProviderModule.h"
#include "Modules/ModuleManager.h"

class FAnalyticsInfinyonAnalytics : public IAnalyticsProviderModule
{
	/** Singleton */
	TSharedPtr<IAnalyticsProvider> Provider;

public:
    static inline FAnalyticsInfinyonAnalytics& Get()
    {
        return FModuleManager::LoadModuleChecked< FAnalyticsInfinyonAnalytics >( "InfinyonAnalytics" );
    }

    virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const override;

private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};