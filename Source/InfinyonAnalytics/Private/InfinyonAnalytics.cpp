// Copyright Infinyon.com  All Rights Reserved.
#include "InfinyonAnalytics.h"
#include "InfinyonAnalyticsProvider.h"
#include "Json.h"
#include "WebSocketsModule.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY_STATIC(LogInfinyonAnalytics, Display, All);

//****** MODULE *********/

IMPLEMENT_MODULE(FAnalyticsInfinyonAnalytics, InfinyonAnalytics)

const uint32 DEFAULT_BUFFER_ENTRIES = 1024;

void FAnalyticsInfinyonAnalytics::StartupModule()
{
    if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    {
        FModuleManager::Get().LoadModule("WebSockets");
    }
    Provider = MakeShareable(new FAnalyticsProviderInfinyonAnalytics());

    auto session_id = FApp::GetSessionId().ToString();
    Provider->SetSessionID(session_id);
    UE_LOG(LogInfinyonAnalytics, Log, TEXT("Infinyon Analytics Module Started: app sid \"%s\""), *session_id);
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
    return Provider;
}

//****** PROVIDER *********/

// defined below
FString EventToString(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes);

FAnalyticsProviderInfinyonAnalytics::FAnalyticsProviderInfinyonAnalytics() :
  nEventBuffer(0)
{
    // hardcode for now, but figure out how to get in from FAnalyticsProviderModule startup
    // and pass to the provider
    ApiKey = TEXT("TBD");
    // WebSocketUrl = TEXT("wss://infinyon.cloud/wsr/v1/simple/produce?access_key=Cad5tsjLM46Ig54m6DVPxusTLTkTwbzC");
    WebSocketUrl = TEXT("ws://127.0.0.1:3000/ws/v2/produce/analytics");
    UE_LOG(LogInfinyonAnalytics, Log, TEXT("Infinyon Analytics Provider created."));
    WebSocketConnect();
}

FAnalyticsProviderInfinyonAnalytics::~FAnalyticsProviderInfinyonAnalytics()
{
    if (WebSocket->IsConnected())
    {
        WebSocket->Close();
    }
}

bool FAnalyticsProviderInfinyonAnalytics::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
    UE_LOG(LogInfinyonAnalytics, Log, TEXT("Session Started: sid \"%s\""), *GetSessionID());

    // record a session start event
    // todo add other FApp attributes
    TArray<FAnalyticsEventAttribute> attrs;
    attrs.Add(FAnalyticsEventAttribute(TEXT("sessionID"), GetSessionID()));
    attrs.Append(Attributes);

    RecordEvent(TEXT("AnalyticsSessionStart"), attrs);

    return true;
}

void FAnalyticsProviderInfinyonAnalytics::EndSession()
{
    TArray<FAnalyticsEventAttribute> Attributes;
    Attributes.Add(FAnalyticsEventAttribute(TEXT("sessionID"), GetSessionID()));
    RecordEvent(TEXT("AnalyticsSessionEnd"), Attributes);
    UE_LOG(LogInfinyonAnalytics, Log, TEXT("Provider Session End: %s"), *GetSessionID());
}

void FAnalyticsProviderInfinyonAnalytics::FlushEvents()
{
    if (!WebSocket->IsConnected())
    {
        WebSocketCheck();
        if (!WebSocket->IsConnected()) {
            return;
        }
    }

    while (!EventBuffer.IsEmpty())
    {
        auto event = DeQueueEvent();
        if (event.IsEmpty())
        {
            break;
        }
        WebSocket->Send(event);
    }
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
    FlushEvents();
    if (!WebSocket->IsConnected()) {
        // buffer the event
        if (!EnQueueEvent(EventName, Attributes))
        {
            UE_LOG(LogInfinyonAnalytics, Warning, TEXT("%s, event not sent or queued, buffer full"), *EventName);
            return;
        }
        UE_LOG(LogInfinyonAnalytics, Log, TEXT("%s, event queued"), *EventName);
        return;
    }
    // Send directly over websocket
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
    // return emptyhttps://forums.unrealengine.com/t/how-to-properly-implement-iwebsocket-and-send-packets-with-high-frequency/1206501/2
    return FAnalyticsEventAttribute();
}

void FAnalyticsProviderInfinyonAnalytics::WebSocketCheck()
{
    auto valid = WebSocket.IsValid() ? TEXT("valid") : TEXT("INVALID");
    auto connected = WebSocket->IsConnected() ? TEXT("connected") : TEXT("NOT connected");
    UE_LOG(LogInfinyonAnalytics, Error, TEXT("WebSocket status: %s %s"), valid, connected);
}

// Can check WebSocket.IsValid() to see if the connection was successful
void FAnalyticsProviderInfinyonAnalytics::WebSocketConnect()
{
    if (WebSocket.IsValid() && WebSocket->IsConnected())
    {
        return;
    }

    UE_LOG(LogInfinyonAnalytics, Log, TEXT("ConnectWebSocket: Checking Websocket module load"));
    if (!FModuleManager::Get().IsModuleLoaded("WebSockets")) {
        FModuleManager::Get().LoadModule("WebSockets");
    }
    auto ws_module = &FWebSocketsModule::Get();
    if (ws_module == nullptr)
    {
        UE_LOG(LogInfinyonAnalytics, Error, TEXT("WebSockets module not loaded!"));
        return;
    }

    if (!WebSocket.IsValid()) {
        FString protocol;
        if (WebSocketUrl.StartsWith(TEXT("wss://")))
        {
            protocol = TEXT("wss");
        }
        else if (WebSocketUrl.StartsWith(TEXT("ws://")))
        {
            protocol = TEXT("ws");
        }
        else
        {
            UE_LOG(LogInfinyonAnalytics, Error, TEXT("WebSocket URL must start with ws:// or wss://"));
            return;
        }
        UE_LOG(LogInfinyonAnalytics, Warning, TEXT("WebSocket create w/ protocol: %s"), *protocol);
        WebSocket = ws_module->CreateWebSocket(WebSocketUrl, protocol);

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
            UE_LOG(LogInfinyonAnalytics, Error, TEXT("WebSocket closed: %s"), *Reason);
        });
        UE_LOG(LogInfinyonAnalytics, Warning, TEXT("WebSocket connecting..."));
        WebSocket->Connect();
    }
    if (!WebSocket->IsConnected()) {
        UE_LOG(LogInfinyonAnalytics, Warning, TEXT("WebSocket still connecting..."));
    }
}

void FAnalyticsProviderInfinyonAnalytics::SendEventOverWebSocket(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
    FlushEvents();
    if (!WebSocket->IsConnected())
    {
        WebSocketCheck();
        return;
    }
    FString out_string = EventToString(EventName, Attributes);
    WebSocket->Send(out_string);
}

bool FAnalyticsProviderInfinyonAnalytics::EnQueueEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
    if (nEventBuffer >= DEFAULT_BUFFER_ENTRIES)
    {
        return false;
    }
    FString out_string = EventToString(EventName, Attributes);
    EventBuffer.Enqueue(out_string);
    nEventBuffer++;
    return true;
}

FString FAnalyticsProviderInfinyonAnalytics::DeQueueEvent()
{
    FString out_string;
    if (EventBuffer.Dequeue(out_string))
    {
        nEventBuffer--;
    }
    return out_string;
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


FString EventToString(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
    auto json_event = EventToJson(EventName, Attributes);

    FString out_string;
    TSharedRef< TJsonWriter<> > writer_ref = TJsonWriterFactory<>::Create(&out_string);
    TJsonWriter<>& Writer = writer_ref.Get();
    FJsonSerializer::Serialize(json_event.ToSharedRef(), Writer);

    return out_string;
}