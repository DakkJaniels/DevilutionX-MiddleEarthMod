#include <fmt/format.h>

#include "DiabloUI/diabloui.h"
#include "stores.h"
#include "storm/storm_net.hpp"
#include "utils/language.h"
#include "utils/utf8.hpp"

namespace devilution {

int provider;
const char *ConnectionNames[] {
	"ZeroTier",
	N_("Client-Server (TCP)"),
	N_("Loopback"),
};

namespace {

char selconn_MaxPlayers[64];
char selconn_Description[256];
char selconn_Gateway[129];
bool selconn_ReturnValue = false;
bool selconn_EndMenu = false;
GameData *selconn_GameData;

std::vector<std::unique_ptr<UiListItem>> vecConnItems;
std::vector<std::unique_ptr<UiItemBase>> vecSelConnDlg;

#define DESCRIPTION_WIDTH 205

void SelconnEsc();
void SelconnFocus(int value);
void SelconnSelect(int value);

void SelconnLoad()
{
	LoadBackgroundArt("ui_art\\selconn.pcx");

#ifndef NONET
#ifndef DISABLE_ZERO_TIER
	vecConnItems.push_back(std::make_unique<UiListItem>(ConnectionNames[SELCONN_ZT], SELCONN_ZT));
#endif
#ifndef DISABLE_TCP
	vecConnItems.push_back(std::make_unique<UiListItem>(_(ConnectionNames[SELCONN_TCP]), SELCONN_TCP));
#endif
#endif
	vecConnItems.push_back(std::make_unique<UiListItem>(_(ConnectionNames[SELCONN_LOOPBACK]), SELCONN_LOOPBACK));

	UiAddBackground(&vecSelConnDlg);
	UiAddLogo(&vecSelConnDlg);

	SDL_Rect rect1 = { (Sint16)(PANEL_LEFT + 24), (Sint16)(Sint16)(UI_OFFSET_Y + 161), 590, 35 };
	vecSelConnDlg.push_back(std::make_unique<UiArtText>(_("Multi Player Game").c_str(), rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	SDL_Rect rect2 = { (Sint16)(PANEL_LEFT + 35), (Sint16)(UI_OFFSET_Y + 218), DESCRIPTION_WIDTH, 21 };
	vecSelConnDlg.push_back(std::make_unique<UiArtText>(selconn_MaxPlayers, rect2, UiFlags::FontSize12 | UiFlags::ColorUiSilverDark));

	SDL_Rect rect3 = { (Sint16)(PANEL_LEFT + 35), (Sint16)(UI_OFFSET_Y + 256), DESCRIPTION_WIDTH, 21 };
	vecSelConnDlg.push_back(std::make_unique<UiArtText>(_("Requirements:").c_str(), rect3, UiFlags::FontSize12 | UiFlags::ColorUiSilverDark));

	SDL_Rect rect4 = { (Sint16)(PANEL_LEFT + 35), (Sint16)(UI_OFFSET_Y + 275), DESCRIPTION_WIDTH, 66 };
	vecSelConnDlg.push_back(std::make_unique<UiArtText>(selconn_Description, rect4, UiFlags::FontSize12 | UiFlags::ColorUiSilverDark, 1, 16));

	SDL_Rect rect5 = { (Sint16)(PANEL_LEFT + 30), (Sint16)(UI_OFFSET_Y + 356), 220, 31 };
	vecSelConnDlg.push_back(std::make_unique<UiArtText>(_("no gateway needed").c_str(), rect5, UiFlags::AlignCenter | UiFlags::FontSize24 | UiFlags::ColorUiSilver, 0));

	SDL_Rect rect6 = { (Sint16)(PANEL_LEFT + 35), (Sint16)(UI_OFFSET_Y + 393), DESCRIPTION_WIDTH, 21 };
	vecSelConnDlg.push_back(std::make_unique<UiArtText>(selconn_Gateway, rect6, UiFlags::AlignCenter | UiFlags::FontSize12 | UiFlags::ColorUiSilverDark));

	SDL_Rect rect7 = { (Sint16)(PANEL_LEFT + 300), (Sint16)(UI_OFFSET_Y + 211), 295, 33 };
	vecSelConnDlg.push_back(std::make_unique<UiArtText>(_("Select Connection").c_str(), rect7, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	SDL_Rect rect8 = { (Sint16)(PANEL_LEFT + 16), (Sint16)(UI_OFFSET_Y + 427), 250, 35 };
	vecSelConnDlg.push_back(std::make_unique<UiArtTextButton>(_("Change Gateway"), nullptr, rect8, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold | UiFlags::ElementHidden));

	vecSelConnDlg.push_back(std::make_unique<UiList>(vecConnItems, vecConnItems.size(), PANEL_LEFT + 305, (UI_OFFSET_Y + 256), 285, 26, UiFlags::AlignCenter | UiFlags::FontSize12 | UiFlags::VerticalCenter | UiFlags::ColorUiGoldDark));

	SDL_Rect rect9 = { (Sint16)(PANEL_LEFT + 299), (Sint16)(UI_OFFSET_Y + 427), 140, 35 };
	vecSelConnDlg.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect9, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	SDL_Rect rect10 = { (Sint16)(PANEL_LEFT + 454), (Sint16)(UI_OFFSET_Y + 427), 140, 35 };
	vecSelConnDlg.push_back(std::make_unique<UiArtTextButton>(_("Cancel"), &UiFocusNavigationEsc, rect10, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	UiInitList(SelconnFocus, SelconnSelect, SelconnEsc, vecSelConnDlg, true);
}

void SelconnFree()
{
	ArtBackground.Unload();

	vecConnItems.clear();

	vecSelConnDlg.clear();
}

void SelconnEsc()
{
	selconn_ReturnValue = false;
	selconn_EndMenu = true;
}

void SelconnFocus(int value)
{
	int players = MAX_PLRS;
	switch (vecConnItems[value]->m_value) {
	case SELCONN_TCP:
		CopyUtf8(selconn_Description, _("All computers must be connected to a TCP-compatible network."), sizeof(selconn_Description));
		players = MAX_PLRS;
		break;
	case SELCONN_ZT:
		CopyUtf8(selconn_Description, _("All computers must be connected to the internet."), sizeof(selconn_Description));
		players = MAX_PLRS;
		break;
	case SELCONN_LOOPBACK:
		CopyUtf8(selconn_Description, _("Play by yourself with no network exposure."), sizeof(selconn_Description));
		players = 1;
		break;
	}

	CopyUtf8(selconn_MaxPlayers, fmt::format(_("Players Supported: {:d}"), players), sizeof(selconn_MaxPlayers));
	CopyUtf8(selconn_Description, WordWrapString(selconn_Description, DESCRIPTION_WIDTH), sizeof(selconn_Description));
}

void SelconnSelect(int value)
{
	provider = vecConnItems[value]->m_value;

	SelconnFree();
	selconn_EndMenu = SNetInitializeProvider(provider, selconn_GameData);
	SelconnLoad();
}

} // namespace

bool UiSelectProvider(GameData *gameData)
{
	selconn_GameData = gameData;
	SelconnLoad();

	selconn_ReturnValue = true;
	selconn_EndMenu = false;
	while (!selconn_EndMenu) {
		UiClearScreen();
		UiPollAndRender();
	}
	SelconnFree();

	return selconn_ReturnValue;
}

} // namespace devilution
