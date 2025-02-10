#pragma once

#include "Interfaces/IAnalyticsProvider.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"


class FAnalyticsProviderInfinyonAnalytics : public IAnalyticsProvider
{
public:
    // Constructor and Destructor
    FAnalyticsProviderInfinyonAnalytics();
    virtual ~FAnalyticsProviderInfinyonAnalytics();

    // IAnalyticsProvider Interface
    virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes) override;
    virtual void EndSession() override;
    virtual void FlushEvents() override;

    virtual void SetUserID(const FString& InUserID) override;
    virtual FString GetUserID() const override;

    virtual bool SetSessionID(const FString& InSessionID) override;
    virtual FString GetSessionID() const override;

    virtual void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) override;

    virtual void SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes) override;
    virtual TArray<FAnalyticsEventAttribute> GetDefaultEventAttributesSafe() const;
    virtual int32 GetDefaultEventAttributeCount() const override;
    virtual FAnalyticsEventAttribute GetDefaultEventAttribute(int AttributeIndex) const;

private:
    FString UserID;
    FString SessionID;

    FString ApiKey;
    FString WebSocketUrl;
    TSharedPtr<IWebSocket> WebSocket; // WebSocket instance

    void ConnectWebSocket();
    void SendEventOverWebSocket(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes);
};
