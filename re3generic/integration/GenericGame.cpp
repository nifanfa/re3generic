#include "common.h"
#include "main.h"
#include "platform.h"
#include "crossplatform.h"
#include "Game.h"
#include "Frontend.h"
#include "Text.h"
#include "Pad.h"
#include "ControllerConfig.h"
#include "re3generic_game.h"
#include "re3generic.h"

extern rw::EngineOpenParams openParams;
extern psGlobalType psGlobal;
extern "C" void rg_audio_pump(void);

static bool initialised;
static bool playing;

int rg_game_init(uint32_t width, uint32_t height)
{
    if (initialised || !rg_bound_port() || width == 0 || height == 0)
        return 0;

    RsGlobal.ps = &psGlobal;
    if (RsEventHandler(rsINITIALIZE, nil) == rsEVENTERROR)
        return 0;

    CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_AMERICAN;
    RsGlobal.width = RsGlobal.maximumWidth = width;
    RsGlobal.height = RsGlobal.maximumHeight = height;
    openParams.width = width;
    openParams.height = height;
    openParams.windowtitle = "re3generic";
    ControlsManager.MakeControllerActionsBlank();
    ControlsManager.InitDefaultControlConfiguration();
    if (RsEventHandler(rsRWINITIALIZE, &openParams) == rsEVENTERROR)
        return 0;

    if (!CGame::InitialiseOnceAfterRW())
        return 0;

    RwRect screen = { 0, 0, (RwInt32)width, (RwInt32)height };
    RsEventHandler(rsCAMERASIZE, &screen);
    FrontEndMenuManager.m_bGameNotLoaded = true;
    CMenuManager::m_bStartUpFrontEndRequested = true;
    initialised = true;
    return 1;
}

int rg_game_step(void)
{
    if (!initialised || RsGlobal.quit)
        return 0;

    const RG_Port *port = rg_bound_port();
    RG_InputEvent event;
    while (port->poll_input(port->userdata, &event)) {
        if (event.type == RG_INPUT_KEY_DOWN || event.type == RG_INPUT_KEY_UP) {
            RsKeyStatus status = {};
            status.keyCharCode = (RsKeyCodes)event.code;
            RsKeyboardEventHandler(event.type == RG_INPUT_KEY_DOWN ? rsKEYDOWN : rsKEYUP, &status);
        }
    }

    if (!playing) {
        RsEventHandler(rsFRONTENDIDLE, nil);
        if (!FrontEndMenuManager.m_bMenuActive || FrontEndMenuManager.m_bWantToLoad) {
            InitialiseGame();
            FrontEndMenuManager.m_bGameNotLoaded = false;
            playing = true;
        }
    } else {
        RsEventHandler(rsIDLE, (void *)TRUE);
    }
    rg_audio_pump();
    return !RsGlobal.quit;
}

void rg_game_shutdown(void)
{
    if (!initialised)
        return;
    if (playing)
        CGame::ShutDown();
    RsEventHandler(rsRWTERMINATE, nil);
    RsEventHandler(rsTERMINATE, nil);
    initialised = false;
    playing = false;
}
