// Copyright Infinyon.com  All Rights Reserved.
#include "InfinyonAnalytics.h"
#include "InfinyonAnalyticsProvider.h"
#include "Json.h"
#include "WebSocketsModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogInfinyonAnalytics, Display, All);

//****** MODULE *********/

IMPLEMENT_MODULE(FAnalyticsInfinyonAnalytics, InfinyonAnalytics)

void FAnalyticsInfinyonAnalytics::StartupModule()
{
    Provider = MakeShareable(new FAnalyticsProviderInfinyonAnalytics());
    UE_LOG(LogInfinyonAnalytics, Log, TEXT("Infinyon Analytics Module Started"));
}

void FAnalyticsInfinyonAnalytics::ShutdownModule()
{
	if (Provider.IsValid())
	{
		Provider->EndSession();
	}
    UE_LOG(LogInfinyonAnalytics, Log, TEXT("Infinyon Analytics Module Shutdown"));
}

TSharedPtr<IAnalyticsProvider> FAnalyticsInfinyonAnalytics::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
    // todo: set key
    // if (GetConfigValue.IsBound())
	// {
    //     // Fetch the API key from the .ini file
    //     FString ApiKey = GetConfigValue.Execute(TEXT("ApiKey"), true);
    //     if (ApiKey.IsEmpty())
    //     {
    //         UE_LOG(LogInfinyonAnalytics, Error, TEXT("API Key is missing in the configuration."));
    //     }
    //     return MakeShared<FAnalyticsProviderInfinyonAnalytics>();
    // } else {
    //     UE_LOG(LogInfinyonAnalytics, Error, TEXT("InfinyonAnalytics: unbound config delegate"));
    // }
    return Provider;
}

//****** PROVIDER *********/

// defined below
TSharedPtr<FJsonObject> EventToJson(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes);


FAnalyticsProviderInfinyonAnalytics::FAnalyticsProviderInfinyonAnalytics()
{
    // hardcode for now, but figure out how to get in from FAnalyticsProviderModule startup
    // and pass to the provider
    ApiKey = TEXT("TBD");
    WebSocketUrl = TEXT("wss://infinyon.cloud/wsr/v1/simple/produce?access_key=Cad5tsjLM46Ig54m6DVPxusTLTkTwbzC");

    UE_LOG(LogInfinyonAnalytics, Log, TEXT("Fluvio Analytics Provider created."));
}

FAnalyticsProviderInfinyonAnalytics::~FAnalyticsProviderInfinyonAnalytics()
{
    if (WebSocket.IsValid())
    {
        WebSocket->Close();
    }
}

bool FAnalyticsProviderInfinyonAnalytics::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
    // record a session start event
    RecordEvent(TEXT("AnalyticsSessionStart"), Attributes);
    UE_LOG(LogInfinyonAnalytics, Log, TEXT("Fluvio Analytics Provider Session Started"));
    return true;
}

void FAnalyticsProviderInfinyonAnalytics::EndSession()
{
    RecordEvent(TEXT("AnalyticsSessionEnd"), TArray<FAnalyticsEventAttribute>());
}

void FAnalyticsProviderInfinyonAnalytics::FlushEvents()
{
    // Flush any pending events
}

void FAnalyticsProviderInfinyonAnalytics::SetUserID(const FString& InUserID)
{
    UserID = InUserID;
}

FString FAnalyticsProviderInfinyonAnalytics::GetUserID() const
{
    return UserID;
}

bool FAnalyticsProviderInfinyonAnalytics::SetSessionID(const FString& InSessionID)
{
    SessionID = InSessionID;
	return true;
}

FString FAnalyticsProviderInfinyonAnalytics::GetSessionID() const
{
    return SessionID;
}

void FAnalyticsProviderInfinyonAnalytics::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
    // Record the event with the provided attributes
    UE_LOG(LogInfinyonAnalytics, Log, TEXT("Event Recorded: %s"), *EventName);

    // DBG: Send directly over websocket
    // todo: figure out if UE framework does buffereing or if it's the provider's responsibility
    SendEventOverWebSocket(EventName, Attributes);
}

void FAnalyticsProviderInfinyonAnalytics::SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes)
{
    // todo add version?
}

TArray<FAnalyticsEventAttribute> FAnalyticsProviderInfinyonAnalytics::GetDefaultEventAttributesSafe() const
{
    // return empty
    return TArray<FAnalyticsEventAttribute>();
}

int32 FAnalyticsProviderInfinyonAnalytics::GetDefaultEventAttributeCount() const
{
    return 0;
}

FAnalyticsEventAttribute FAnalyticsProviderInfinyonAnalytics::GetDefaultEventAttribute(int AttributeIndex) const
{
    // return empty
    return FAnalyticsEventAttribute();
}

// Can check WebSocket.IsValid() to see if the connection was successful
void FAnalyticsProviderInfinyonAnalytics::ConnectWebSocket()
{
    UE_LOG(LogInfinyonAnalytics, Log, TEXT("ConnectWebSocket: Checking Websocket module load"));
    if (!FModuleManager::Get().IsModuleLoaded("WebSockets")) {
        UE_LOG(LogInfinyonAnalytics, Log, TEXT("WebSockets module not loaded!"));
        return;
    }
    UE_LOG(LogInfinyonAnalytics, Log, TEXT("WebSocket trying to connect"));
    const FString protocol = TEXT("wss");
    WebSocket = FWebSocketsModule::Get().CreateWebSocket(WebSocketUrl, protocol);
    WebSocket->OnConnected().AddLambda([]()
    {
        UE_LOG(LogInfinyonAnalytics, Log, TEXT("WebSocket connected!"));
    });

    WebSocket->OnConnectionError().AddLambda([](const FString& Error)
    {
        UE_LOG(LogInfinyonAnalytics, Error, TEXT("WebSocket connection error: %s"), *Error);
    });

    WebSocket->OnClosed().AddLambda([](int32 StatusCode, const FString& Reason, bool bWasClean)
    {
        UE_LOG(LogInfinyonAnalytics, Log, TEXT("WebSocket closed: %s"), *Reason);
    });
    WebSocket->Connect();
}

void FAnalyticsProviderInfinyonAnalytics::SendEventOverWebSocket(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
    if (!WebSocket.IsValid() || !WebSocket->IsConnected())
    {
        ConnectWebSocket();
        if (!WebSocket.IsValid() || !WebSocket->IsConnected())
        {
            UE_LOG(LogInfinyonAnalytics, Error, TEXT("WebSocket is not connected."));
            return;
        }
    }

     auto json_event = EventToJson(EventName, Attributes);

     FString out_string;
     TSharedRef< TJsonWriter<> > writer_ref = TJsonWriterFactory<>::Create(&out_string);
     TJsonWriter<>& Writer = writer_ref.Get();
     FJsonSerializer::Serialize(json_event.ToSharedRef(), Writer);

     WebSocket->Send(out_string);
}


TSharedPtr<FJsonObject> EventToJson(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) {
   // Create a JSON object to hold our data
   TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

   // Add event name
   JsonObject->SetStringField(TEXT("eventName"), EventName);

   // Create a JSON object for the attributes
   TSharedPtr<FJsonObject> AttributesObject = MakeShared<FJsonObject>();


   // Add each attribute to the JSON object
   for (const FAnalyticsEventAttribute& Attribute : Attributes)
   {
        AttributesObject->SetStringField(Attribute.GetName(), Attribute.GetValue());
   }
   JsonObject->SetObjectField(TEXT("attributes"), AttributesObject);
   return JsonObject;
}
