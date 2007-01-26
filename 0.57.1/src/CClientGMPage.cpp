#include "graysvr.h"
#include "CClient.h"
#include "network/network.h"

/////////////////////////////////////////////

void CClient::Cmd_GM_Page( LPCTSTR pszReason ) // Help button (Calls GM Call Menus up)
{
	// Player pressed the help button.
	// m_Targ_Text = menu description
	// CLIMODE_PROMPT_GM_PAGE_TEXT

	if ( pszReason[0] == '\0' )
	{
		SysMessageDefault( DEFMSG_GMPAGE_CANCELED );
		return;
	}

	TEMPSTRING(pszMsg);
	sprintf(pszMsg, g_Cfg.GetDefaultMsg( DEFMSG_GMPAGE_REC ),
		m_pChar->GetName(), m_pChar->uid(), m_pChar->GetTopPoint().m_x,m_pChar->GetTopPoint().m_y,m_pChar->GetTopPoint().m_z,m_pChar->GetTopPoint().m_map, pszReason);
	g_Log.Event( LOGM_GM_PAGE, "%s\n", (LPCTSTR)pszMsg);

	bool fFound=false;
	ClientIterator it;
	CClient *client;
	while ( client = it.next() )
	{
		if ( client->GetChar() && client->IsPriv(PRIV_GM_PAGE) ) // found GM
		{
			fFound = true;
			client->SysMessage(pszMsg);
		}
	}

	SysMessageDefault( fFound ? DEFMSG_GMPAGE_NOTIFIED : DEFMSG_GMPAGE_QUED );

	sprintf(pszMsg, g_Cfg.GetDefaultMsg( DEFMSG_GMPAGE_QNUM ), g_World.m_GMPages.GetCount());
	SysMessage(pszMsg);

	// Already have a message in the queue ?
	// Find an existing GM page for this account.
	CGMPage * pPage = static_cast <CGMPage*>( g_World.m_GMPages.GetHead());
	for ( ; pPage!= NULL; pPage = pPage->GetNext())
	{
		if ( ! strcmpi( pPage->GetName(), GetAccount()->GetName()))
			break;
	}
	if ( pPage != NULL )
	{
		SysMessageDefault( DEFMSG_GMPAGE_UPDATE );
		pPage->SetReason( pszReason );
		pPage->m_timePage = CServTime::GetCurrentTime();
	}
	else
	{
		// Queue a GM page. (based on account)
		pPage = new CGMPage( GetAccount()->GetName());
		pPage->SetReason( pszReason );	// Description of reason for call.
	}
	pPage->m_ptOrigin = m_pChar->GetTopPoint();		// Origin Point of call.
}

void CClient::Cmd_GM_PageClear()
{
	if ( m_pGMPage )
	{
		m_pGMPage->ClearGMHandler();
		m_pGMPage = NULL;
	}
}

void CClient::Cmd_GM_PageMenu( int iEntryStart )
{
	// Just put up the GM page menu.
	SetPrivFlags( PRIV_GM_PAGE );
	Cmd_GM_PageClear();

	CMenuItem item[10];	// only display x at a time.

	item[0].m_sText = "GM Page Menu";

	int entry=0;
	int count=0;
	CGMPage * pPage = static_cast <CGMPage*>( g_World.m_GMPages.GetHead());
	for ( ; pPage!= NULL; pPage = pPage->GetNext(), entry++ )
	{
		if ( entry < iEntryStart )
			continue;

		CClient * pGM = pPage->FindGMHandler();	// being handled ?
		if ( pGM != NULL )
			continue;

		if ( ++count >= COUNTOF( item )-1 )
		{
			// Add the "MORE" option if there is more than 1 more.
			if ( pPage->GetNext() != NULL )
			{
				item[count].m_id = count-1;
				item[count].m_color = 0;
				item[count].m_sText.Format( "MORE" );
				m_tmMenu.m_Item[count] = 0xFFFF0000 | entry;
				break;
			}
		}

		CClient * pClient = pPage->FindAccount()->FindClient();	// logged in ?

		item[count].m_id = count-1;
		item[count].m_color = 0;
		item[count].m_sText.Format("%s %s %s", pPage->GetName(), (pClient==NULL) ? "OFF":"ON ", pPage->GetReason());
		m_tmMenu.m_Item[count] = entry;
	}

	if ( ! count )
	{
		SysMessage( "No GM pages queued. Use .page ?" );
		return;
	}
	addItemMenu( CLIMODE_MENU_GM_PAGES, item, count );
}

void CClient::Cmd_GM_PageInfo()
{
	// Show the current page.
	// This should be a dialog !!!??? book or scroll.
	SysMessagef("Current GM .PAGE Account=%s (%s) Reason='%s' Time=%ld",
		m_pGMPage->GetName(), m_pGMPage->GetAccountStatus(), m_pGMPage->GetReason(), m_pGMPage->GetAge());
}

enum GPV_TYPE
{
	GPV_BAN,
	GPV_CURRENT,
	GPV_D,
	GPV_DELETE,
	GPV_GO,
	GPV_JAIL,
	GPV_L,	// List
	GPV_LIST,
	GPV_OFF,
	GPV_ON,
	GPV_ORIGIN,
	GPV_U,
	GPV_UNDO,
	GPV_WIPE,
	GPV_QTY,
};

static LPCTSTR const sm_pszGMPageVerbs[GPV_QTY] =
{
	"BAN",
	"CURRENT",
	"D",
	"DELETE",
	"GO",
	"JAIL",
	"L",	// List
	"LIST",
	"OFF",
	"ON",
	"ORIGIN",
	"U",
	"UNDO",
	"WIPE",
};

void CClient::Cmd_GM_PageCmd( LPCTSTR pszCmd )
{
	static LPCTSTR const sm_pszGMPageVerbsHelp[] =
	{
		".PAGE on/off\n",
		".PAGE list = list of pages.\n",
		".PAGE delete = dispose of this page. Assume it has been handled.\n",
		".PAGE origin = go to the origin point of the page\n",
		".PAGE undo/queue = put back in the queue\n",
		".PAGE current = info on the current selected page.\n",
		".PAGE go/player = go to the player that made the page. (if they are logged in)\n",
		".PAGE jail\n",
		".PAGE ban/kick\n",
		".PAGE wipe (gm only)"
	};
	// A GM page command.
	// Put up a list of GM pages pending.

	if ( pszCmd == NULL || pszCmd[0] == '?' )
	{
		for ( int i=0; i<COUNTOF(sm_pszGMPageVerbsHelp); i++ )
		{
			SysMessage( sm_pszGMPageVerbsHelp[i] );
		}
		return;
	}
	if ( pszCmd[0] == '\0' )
	{
		if ( m_pGMPage )
			Cmd_GM_PageInfo();
		else
			Cmd_GM_PageMenu();
		return;
	}

	int index = FindTableHeadSorted( pszCmd, sm_pszGMPageVerbs, COUNTOF(sm_pszGMPageVerbs) );
	if ( index < 0 )
	{
		Cmd_GM_PageCmd(NULL);
		return;
	}

	switch ( index )
	{
		case GPV_OFF:
			if ( GetPrivLevel() < PLEVEL_Counsel )
				return;	// cant turn off.
			ClearPrivFlags( PRIV_GM_PAGE );
			Cmd_GM_PageClear();
			SysMessage( "GM pages off" );
			return;
		case GPV_ON:
			SetPrivFlags( PRIV_GM_PAGE );
			SysMessage( "GM pages on" );
			return;
		case GPV_WIPE:
			if ( ! IsPriv( PRIV_GM ))
				return;
			g_World.m_GMPages.DeleteAll();
			return;
	}

	if ( m_pGMPage == NULL )
	{
		// No gm page has been selected yet.
		Cmd_GM_PageMenu();
		return;
	}

	// We must have a select page for these commands.
	switch ( index )
	{
		case GPV_L:	// List
		case GPV_LIST:
			Cmd_GM_PageMenu();
			return;
		case GPV_D:
		case GPV_DELETE:
			// /PAGE delete = dispose of this page. We assume it has been handled.
			SysMessage( "GM Page deleted" );
			delete m_pGMPage;
			return;
		case GPV_ORIGIN:
			// /PAGE origin = go to the origin point of the page
			m_pChar->Spell_Teleport( m_pGMPage->m_ptOrigin, true, false );
			return;
		case GPV_U:
		case GPV_UNDO:
			// /PAGE queue = put back in the queue
			SysMessage( "GM Page re-queued." );
			Cmd_GM_PageClear();
			return;
		case GPV_BAN:
			// This should work even if they are not logged in.
			{
				CAccountRef pAccount = m_pGMPage->FindAccount();
				if ( pAccount )
				{
					if ( ! pAccount->Kick( this, true ))
						return;
				}
				else
				{
					SysMessage( "Invalid account for page !?" );
				}
			}
			break;
		case GPV_CURRENT:
			// What am i servicing ?
			Cmd_GM_PageInfo();
			return;
	}

	// Manipulate the character only if logged in.

	CClient * pClient = m_pGMPage->FindAccount()->FindClient();
	if ( pClient == NULL || pClient->GetChar() == NULL )
	{
		SysMessage( "The account is not logged in." );
		if ( index == GPV_GO )
		{
			m_pChar->Spell_Teleport( m_pGMPage->m_ptOrigin, true, false );
		}
		return;
	}

	switch ( index )
	{
		case GPV_GO: // /PAGE player = go to the player that made the page. (if they are logged in)
			m_pChar->Spell_Teleport( pClient->GetChar()->GetTopPoint(), true, false );
			return;
		case GPV_BAN:
			pClient->addKick( m_pChar );
			return;
		case GPV_JAIL:
			pClient->GetChar()->Jail( this, true, 0 );
			return;
		default:
			return;
	}
}

void CClient::Cmd_GM_PageSelect( int iSelect )
{
	// 0 = cancel.
	// 1 based.

	if ( m_pGMPage != NULL )
	{
		SysMessage( "Current message sent back to the queue" );
		Cmd_GM_PageClear();
	}

	if ( iSelect <= 0 || iSelect >= COUNTOF(m_tmMenu.m_Item))
	{
		return;
	}

	if ( m_tmMenu.m_Item[iSelect] & 0xFFFF0000 )
	{
		// "MORE" selected
		Cmd_GM_PageMenu( m_tmMenu.m_Item[iSelect] & 0xFFFF );
		return;
	}

	CGMPage * pPage = static_cast <CGMPage*>( g_World.m_GMPages.GetAt( m_tmMenu.m_Item[iSelect] ));
	if ( pPage != NULL )
	{
		if ( pPage->FindGMHandler())
		{
			SysMessage( "GM Page already being handled." );
			return;	// someone already has this.
		}

		m_pGMPage = pPage;
		m_pGMPage->SetGMHandler( this );
		Cmd_GM_PageInfo();
		Cmd_GM_PageCmd( "GO" );	// go there.
	}
}
