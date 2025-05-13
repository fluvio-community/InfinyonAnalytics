// Copyright Infinyon.com  2025 All Rights Reserved.
#pragma once

#include "Interfaces/IAnalyticsProvider.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Containers/Queue.h"

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

    TArray<FAnalyticsEventAttribute> DefaultEventAttributes;

    FString WebSocketUrl;
    TSharedPtr<IWebSocket> WebSocket; // WebSocket instance

    TQueue<FString> EventBuffer;
    uint32 nEventBuffer;

    bool EnQueueEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes);
    FString DeQueueEvent();

    void WebSocketCheck();
    void WebSocketConnect();

    void SendEventOverWebSocket(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes);
};
