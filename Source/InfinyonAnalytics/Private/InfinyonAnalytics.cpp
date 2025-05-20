// Copyright Infinyon 2025 All Rights Reserved.
#include "InfinyonAnalytics.h"
#include "InfinyonAnalyticsProvider.h"

// #include "Json.h"
// include json sub components to avoid pagma warning
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonSerializerMacros.h"
#include "Serialization/JsonWriter.h"

#include "WebSocketsModule.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"

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
FString EventToString(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes);

FAnalyticsProviderInfinyonAnalytics::FAnalyticsProviderInfinyonAnalytics() :
  nEventBuffer(0)
{
    // See DefaultEngine.ini for configuration, or Plugins/InfinyonAnalytics/sampleConfig.ini
    const auto SECTION_NAME = TEXT("InfinyonAnalytics");
    const auto DEFAULT_URL = TEXT("wss://infinyon.cloud/wsr/v1/simple/produce");

    FString cfgApiKey;
    FString cfgUrl;

    UE_LOG(LogInfinyonAnalytics, Warning, TEXT("checking ini %s"), *GEngineIni);
    if (GConfig) {
        if (!GConfig->DoesSectionExist(SECTION_NAME, GEngineIni)) {
            UE_LOG(LogInfinyonAnalytics, Warning, TEXT("Config section not found for %s"), SECTION_NAME);
        } else {
            GConfig->GetString(SECTION_NAME, TEXT("ApiKey"), cfgApiKey, GEngineIni);
            GConfig->GetString(SECTION_NAME, TEXT("URL"), cfgUrl, GEngineIni);
            UE_LOG(LogInfinyonAnalytics, Warning, TEXT("loaded url=%s access_key=%s"), *cfgUrl, *cfgApiKey);
        }
    }
    if (cfgUrl.IsEmpty()) {
        cfgUrl = DEFAULT_URL;
    }
    if (!cfgApiKey.IsEmpty()) {
        WebSocketUrl = FString::Printf(TEXT("%s?access_key=%s"), *cfgUrl, *cfgApiKey);
    }
    // UE_LOG(LogInfinyonAnalytics, VeryVerbose, TEXT("Url: %s"), *WebSocketUrl);
    UE_LOG(LogInfinyonAnalytics, Log, TEXT("provider created"));
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


    TArray<FAnalyticsEventAttribute> attrs;
    attrs.Add(FAnalyticsEventAttribute(TEXT("sessionID"), GetSessionID()));

    // additional FApp attributes
    if (true) {
        auto SessionName = FApp::GetSessionName();
        if (!SessionName.IsEmpty())
        {
            attrs.Add(FAnalyticsEventAttribute(TEXT("sessionName"), SessionName));
        }
        attrs.Add(FAnalyticsEventAttribute(TEXT("buildDate"), FApp::GetBuildDate()));
        attrs.Add(FAnalyticsEventAttribute(TEXT("buildVersion"), FApp::GetBuildVersion()));
        attrs.Add(FAnalyticsEventAttribute(TEXT("graphicsRhi"), FApp::GetGraphicsRHI()));
        attrs.Add(FAnalyticsEventAttribute(TEXT("instanceId"), FApp::GetInstanceId()));
        attrs.Add(FAnalyticsEventAttribute(TEXT("unrealEngineId"), FApp::GetEpicProductIdentifier()));
        if (FApp::HasProjectName())
        {
            attrs.Add(FAnalyticsEventAttribute(TEXT("projectName"), FApp::GetProjectName()));
        }
    }
    attrs.Append(Attributes);

    RecordEvent(TEXT("AnalyticsSessionStart"), attrs);

    return true;
}


void FAnalyticsProviderInfinyonAnalytics::EndSession()
{
    UE_LOG(LogInfinyonAnalytics, Log, TEXT("Provider Session End: %s"), *GetSessionID());

    TArray<FAnalyticsEventAttribute> Attributes;
    Attributes.Add(FAnalyticsEventAttribute(TEXT("sessionID"), GetSessionID()));
    RecordEvent(TEXT("AnalyticsSessionEnd"), Attributes);
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

// this is for analytics user id, not used for InfinyonAnalytics
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

    // if no default attributes have been set, add the sessionID
    // this will be wiped out if SetDefaultEventAttributes is called
    // but at that point, the caller should be aware of what they are doing
    // and are recommended to keep the sessionID in the default attributes
    // along with any other attributes they want to be sent with every event
    if (DefaultEventAttributes.Num() == 0)
    {
        DefaultEventAttributes.Add(FAnalyticsEventAttribute(TEXT("sessionID"), SessionID));
    }
	return true;
}


FString FAnalyticsProviderInfinyonAnalytics::GetSessionID() const
{
    return SessionID;
}


void FAnalyticsProviderInfinyonAnalytics::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
    FlushEvents();

    TArray<FAnalyticsEventAttribute> attrs;
    attrs.Reserve(Attributes.Num() + DefaultEventAttributes.Num());

    attrs.Append(DefaultEventAttributes);
    attrs.Append(Attributes);

    if (!WebSocket->IsConnected()) {
        // buffer the event
        if (!EnQueueEvent(EventName, attrs))
        {
            UE_LOG(LogInfinyonAnalytics, Warning, TEXT("%s, event dropped, buffer full"), *EventName);
            return;
        }
        UE_LOG(LogInfinyonAnalytics, Log, TEXT("%s, event queued"), *EventName);
        return;
    }
    SendEventOverWebSocket(EventName, attrs);
}


void FAnalyticsProviderInfinyonAnalytics::SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes)
{
    DefaultEventAttributes = MoveTemp(Attributes);
}


TArray<FAnalyticsEventAttribute> FAnalyticsProviderInfinyonAnalytics::GetDefaultEventAttributesSafe() const
{
    return TArray<FAnalyticsEventAttribute>();
}


int32 FAnalyticsProviderInfinyonAnalytics::GetDefaultEventAttributeCount() const
{
    return 0;
}


FAnalyticsEventAttribute FAnalyticsProviderInfinyonAnalytics::GetDefaultEventAttribute(int AttributeIndex) const
{
    return FAnalyticsEventAttribute();
}


void FAnalyticsProviderInfinyonAnalytics::WebSocketCheck()
{
    auto valid = WebSocket.IsValid() ? TEXT("valid") : TEXT("INVALID");
    auto connected = WebSocket->IsConnected() ? TEXT("connected") : TEXT("NOT connected");
    UE_LOG(LogInfinyonAnalytics, VeryVerbose, TEXT("WebSocket status: %s %s buf: %d"), valid, connected, nEventBuffer);
}


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

        // TODO: wrap WebSocket and state into a state engine to handle connection retries
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