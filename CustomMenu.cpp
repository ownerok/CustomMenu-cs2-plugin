#include "CustomMenu.h"

CustomMenu CustomMenu;

IVEngineServer2* engine = nullptr;
CGlobalVars* gpGlobals = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

IUtilsApi* utils;
IMenusApi* menus_api;
IPlayersApi* players_api;

PLUGIN_EXPOSE(CustomMenu, CustomMenu);

static bool pluginLoaded = false;
vector<MenuData> allMenus;
std::map<std::string, std::string> g_Phrases;
const char* g_pszLanguage = "en";

string startMenu;

bool g_bShowDeathMessage = true;
bool g_bShowRoundStartMessage = true;

CGameEntitySystem* GameEntitySystem()
{
    return utils->GetCGameEntitySystem();
}

bool FileExists(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (file)
    {
        fclose(file);
        return true;
    }
    return false;
}

void CustomMenu_LoadTranslations()
{
    g_Phrases.clear();
    KeyValues* g_kvPhrases = new KeyValues("Phrases");
    const char *pszPath = "addons/translations/custommenu.phrases.txt";

    if (!g_kvPhrases->LoadFromFile(g_pFullFileSystem, pszPath))
    {
        utils->ErrorLog("%s Failed to load %s", g_PLAPI->GetLogTag(), pszPath);
        delete g_kvPhrases;
        return;
    }

    g_pszLanguage = utils->GetLanguage();

    for (KeyValues *pKey = g_kvPhrases->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey())
    {
        const char* translation = pKey->GetString(g_pszLanguage);
        if (!translation || !strlen(translation))
        {
            translation = pKey->GetString("en");
        }
        
        g_Phrases[std::string(pKey->GetName())] = std::string(translation);
    }
    
    delete g_kvPhrases;
}

const char* CustomMenu_GetPhrase(const char* key)
{
    auto it = g_Phrases.find(std::string(key));
    if (it != g_Phrases.end() && !it->second.empty())
    {
        return it->second.c_str();
    }
    
    static std::map<std::string, std::string> fallbackPhrases = {
        {"Prefix", "{DEFAULT}[CustomMenu]"},
        {"DeathMessage", "{DEFAULT}You died! Type {GREEN}!menuownerok{DEFAULT} to open menu"},
        {"RoundStartMessage", "{GREEN}New round!{DEFAULT} Type {YELLOW}!menuownerok{DEFAULT} to open menu"},
        {"MenuNotFound", "{RED}Menu {YELLOW}%s{RED} not found!"},
        {"ConfigReloaded", "{GREEN}CustomMenu configuration reloaded successfully!"},
        {"WelcomeMessage", "{LIGHT_GREEN}Welcome! Type {PINK}!menuownerok{LIGHT_GREEN} to open menu"}
    };
    
    auto fallback = fallbackPhrases.find(std::string(key));
    if (fallback != fallbackPhrases.end())
    {
        return fallback->second.c_str();
    }
    
    return key;
}

void CustomMenu_FormatString(char* buffer, size_t maxlen, const char* message)
{
    std::string result = message;
    
    size_t pos = 0;
    while ((pos = result.find("{DEFAULT}", pos)) != std::string::npos) {
        result.replace(pos, 9, "\x01");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find("{RED}", pos)) != std::string::npos) {
        result.replace(pos, 6, "\x02");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find("{GREEN}", pos)) != std::string::npos) {
        result.replace(pos, 8, "\x04");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find("{LIME}", pos)) != std::string::npos) {
        result.replace(pos, 7, "\x05");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find("{LIGHT_GREEN}", pos)) != std::string::npos) {
        result.replace(pos, 14, "\x06");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find("{YELLOW}", pos)) != std::string::npos) {
        result.replace(pos, 9, "\x07");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find("{PINK}", pos)) != std::string::npos) {
        result.replace(pos, 7, "\x08");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find("{PURPLE}", pos)) != std::string::npos) {
        result.replace(pos, 9, "\x09");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find("{GRAY}", pos)) != std::string::npos) {
        result.replace(pos, 7, "\x0A");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find("{LIGHT_RED}", pos)) != std::string::npos) {
        result.replace(pos, 12, "\x0B");
        pos += 1;
    }
    pos = 0;
    while ((pos = result.find("{OLIVE}", pos)) != std::string::npos) {
        result.replace(pos, 8, "\x0E");
        pos += 1;
    }
    
    strncpy(buffer, result.c_str(), maxlen - 1);
    buffer[maxlen - 1] = '\0';
}

void CustomMenu_PrintToChat(int iSlot, const char* phraseKey, ...)
{
    const char* format = CustomMenu_GetPhrase(phraseKey);
    
    va_list args;
    va_start(args, phraseKey);
    
    char buffer[512];
    V_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    const char* prefix = CustomMenu_GetPhrase("Prefix");
    
    char finalBuffer[1024];
    V_snprintf(finalBuffer, sizeof(finalBuffer), "%s %s", prefix, buffer);
    
    char coloredBuffer[1024];
    CustomMenu_FormatString(coloredBuffer, sizeof(coloredBuffer), finalBuffer);
    
    utils->PrintToChat(iSlot, "%s", coloredBuffer);
}

void CustomMenu_PrintToChatAll(const char* phraseKey, ...)
{
    const char* format = CustomMenu_GetPhrase(phraseKey);
    
    va_list args;
    va_start(args, phraseKey);
    
    char buffer[512];
    V_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    const char* prefix = CustomMenu_GetPhrase("Prefix");
    
    char finalBuffer[1024];
    V_snprintf(finalBuffer, sizeof(finalBuffer), "%s %s", prefix, buffer);
    
    char coloredBuffer[1024];
    CustomMenu_FormatString(coloredBuffer, sizeof(coloredBuffer), finalBuffer);
    
    utils->PrintToChatAll("%s", coloredBuffer);
}

void OpenMenu(int iSlot, string menu_name) {
    Menu hMenu;
    for (auto& m : allMenus) {
        if (m.name == menu_name) {
            menus_api->SetTitleMenu(hMenu, m.title.c_str());
            for (auto& i : m.items) {
                menus_api->AddItemMenu(hMenu, i.nextMenu.c_str(), i.text.c_str(), i.isDisabled ? ITEM_DISABLED : ITEM_DEFAULT);
            }
            menus_api->SetExitMenu(hMenu, true);
            if (m.name != startMenu) menus_api->SetBackMenu(hMenu, true);
            menus_api->SetCallback(hMenu, [m](const char* szBack, const char* szFront, int iItem, int iSlot) {
                CCSPlayerController* player = CCSPlayerController::FromSlot(iSlot);
                if (!player || !player->IsConnected()) return;
                if (!szBack || strcmp(szBack, "") == 0) return;
                if (strcmp(szBack, "back") == 0) {
                    OpenMenu(iSlot, m.backMenu.empty() ? startMenu : m.backMenu);
                }
                OpenMenu(iSlot, szBack);
            });
            menus_api->DisplayPlayerMenu(hMenu, iSlot, true, true);
            break;
        }
    }
}

bool StartMenuOpen(int iSlot, const char* content) {
    CCSPlayerController* player = CCSPlayerController::FromSlot(iSlot);
    if (!player || !player->IsConnected()) return false;

    bool bFound = false;
    for (auto& m : allMenus) {
        if (m.name == startMenu) {
            bFound = true;
            OpenMenu(iSlot, m.name);
            break;
        }
    }
    if (!bFound) {
        CustomMenu_PrintToChat(iSlot, "MenuNotFound", startMenu.c_str());
    }
    return true;
}

void OnPlayerDeath(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
    int iUserID = pEvent->GetInt("userid");
    if (iUserID < 0 || iUserID >= 64) return;
    
    CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(iUserID);
    if (!pPlayer || !pPlayer->IsConnected()) return;
    
    if (g_bShowDeathMessage && !startMenu.empty())
    {
        CustomMenu_PrintToChat(iUserID, "DeathMessage");
    }
}

void OnRoundStart(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
    if (g_bShowRoundStartMessage && !startMenu.empty())
    {
        CustomMenu_PrintToChatAll("RoundStartMessage");
    }
}

void OnRoundEnd(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
    // Можно добавить сообщение о конце раунда если нужно
}

void OnPlayerFullConnect(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
    int iSlot = pEvent->GetInt("userid");
    
    if (!startMenu.empty())
    {
        CustomMenu_PrintToChat(iSlot, "WelcomeMessage");
    }
    
    StartMenuOpen(iSlot, "");
}

void LoadMenuFromKV(KeyValues* kv, const std::string& path)
{
    std::string filename = path;
    size_t slash = filename.find_last_of("/\\");
    if (slash != std::string::npos)
        filename = filename.substr(slash + 1);

    size_t dot = filename.find_last_of(".");
    if (dot != std::string::npos)
        filename = filename.substr(0, dot);

    META_CONPRINTF("%s | Loaded: %s from menu list\n", g_PLAPI->GetLogTag(), filename.c_str());
    KeyValues* itemsKV = kv->FindKey("Items");
    if (itemsKV) {
        vector<MenuItem> items;
        FOR_EACH_TRUE_SUBKEY(itemsKV, id)
        {
            MenuItem item;
            const char* idName = id->GetName();
            if (strcmp(idName, "Item") != 0) continue;

            item.text = id->GetString("Text", "");
            item.nextMenu = id->GetString("NextMenu", "");
            item.isDisabled = id->GetBool("disabled", false);
            items.push_back(item);
        }
        allMenus.push_back({ filename, kv->GetString("Title", ""), kv->GetString("BackMenu", ""), items });
    }
}

void LoadConfig() {
    allMenus.clear();
    KeyValues* config = new KeyValues("Config");
    const char* path = "addons/configs/CustomMenu/config.ini";
    
    if (!config->LoadFromFile(g_pFullFileSystem, path)) {
        utils->ErrorLog("%s Failed to load: %s", g_PLAPI->GetLogTag(), path);
        delete config;
        return;
    }

    startMenu = config->GetString("start_menu", "");
    
    g_bShowDeathMessage = config->GetBool("show_death_message", true);
    g_bShowRoundStartMessage = config->GetBool("show_roundstart_message", true);

    KeyValues* menus = config->FindKey("Menus");
    if (!menus) {
        utils->ErrorLog("%s No `Menus` key in config", g_PLAPI->GetLogTag());
        delete config;
        return;
    }
    
    for (KeyValues* sub = menus->GetFirstSubKey(); sub; sub = sub->GetNextKey()) {
        const char* menuName = sub->GetName();
        char menuPath[256];
        g_SMAPI->Format(menuPath, sizeof(menuPath),
            "addons/configs/CustomMenu/menus/%s.cfg", menuName);
        
        KeyValues* kvMenu = new KeyValues("Menu");
        if (!kvMenu->LoadFromFile(g_pFullFileSystem, menuPath)) {
            utils->ErrorLog("%s | Failed to load menu from %s", g_PLAPI->GetLogTag(), menuPath);
            delete kvMenu;
            continue;
        }

        LoadMenuFromKV(kvMenu, menuPath);
        delete kvMenu;
    }

    delete config;
}

CON_COMMAND_F(mm_custommenu_reload, "Reload CustomMenu configuration and translations", FCVAR_SERVER_CAN_EXECUTE) {
    LoadConfig();
    CustomMenu_LoadTranslations();
    CustomMenu_PrintToChatAll("ConfigReloaded");
    META_CONPRINTF("CustomMenu reload successfully.\n");
}

void StartupServer() {
    g_pGameEntitySystem = GameEntitySystem();
    g_pEntitySystem = utils->GetCEntitySystem();
    gpGlobals = utils->GetCGlobalVars();
    META_CONPRINTF("%s Plugin started successfully.\n", g_PLAPI->GetLogTag());
}

bool CustomMenu::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late) {
    PLUGIN_SAVEVARS();
    GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
    GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
    GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);

    ConVar_Register(FCVAR_SERVER_CAN_EXECUTE | FCVAR_GAMEDLL);
    g_SMAPI->AddListener(this, this);

    return true;
}

void CustomMenu::AllPluginsLoaded() {
    int ret;
    utils = (IUtilsApi*)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("[CustomMenu] Missing UTILS plugin.\n");
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }
    menus_api = (IMenusApi*)g_SMAPI->MetaFactory(Menus_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("[CustomMenu] Missing Menus plugin.\n");
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }
    players_api = (IPlayersApi*)g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, nullptr);
    if (ret == META_IFACE_FAILED) {
        META_CONPRINTF("[CustomMenu] Missing Players plugin.\n");
        engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
        return;
    }

    CustomMenu_LoadTranslations();

    utils->HookEvent(g_PLID, "player_connect_full", OnPlayerFullConnect);
    utils->HookEvent(g_PLID, "player_death", OnPlayerDeath);
    utils->HookEvent(g_PLID, "round_start", OnRoundStart);
    utils->HookEvent(g_PLID, "round_end", OnRoundEnd);
    
    utils->RegCommand(g_PLID, {"mm_openmenu"}, {"!openmeun"}, StartMenuOpen);
    utils->StartupServer(g_PLID, StartupServer);
    LoadConfig();
    pluginLoaded = true;
}

bool CustomMenu::Unload(char* error, size_t maxlen) {
    if (utils) utils->ClearAllHooks(g_PLID);
    ConVar_Unregister();
    return true;
}

const char* CustomMenu::GetAuthor() { return "ownerok"; }
const char* CustomMenu::GetDate() { return __DATE__; }
const char* CustomMenu::GetDescription() { return "Custom Menu"; }
const char* CustomMenu::GetLicense() { return "Free"; }
const char* CustomMenu::GetLogTag() { return "CustomMenu"; }
const char* CustomMenu::GetName() { return "Custom Menu"; }
const char* CustomMenu::GetURL() { return ""; }
const char* CustomMenu::GetVersion() { return "1.0.0"; }
