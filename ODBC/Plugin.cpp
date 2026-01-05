////////////////////////////////////////////////////
// Plugin.cpp
// Standard implementation file for all AmiBroker plug-ins
//
// Copyright (C)2001-2006 Tomasz Janeczko, amibroker.com
// All rights reserved.
//
// Last modified: 2006-07-21 TJ
// 
// You may use this code in your own projects provided that:
//
// 1. You are registered user of AmiBroker
// 2. The software you write using it is for personal, noncommercial use only
//
// For commercial use you have to obtain a separate license from Amibroker.com
//
////////////////////////////////////////////////////

#include "stdafx.h"
#include "ODBC.h"
#include "Plugin.h"
#include "resource.h"
#include "ConfigDlg.h"
#include "SRecordset.h"


// These are the only two lines you need to change
#define PLUGIN_NAME "ODBC/SQL Universal Data Plug-in"
#define PLUGIN_NAME_AFL "ODBC/SQL Universal AFL Plug-in"
#define VENDOR_NAME "Amibroker.com"
#define PLUGIN_VERSION 10700
#define PLUGIN_ID PIDCODE( 'O', 'D', 'B', 'S' )

// NOTE: we don't use
// "ODBC" id code because some 3rd party group used it in their plugin
// and we don't need conflicts if someone installed that before

#define THIS_PLUGIN_TYPE PLUGIN_TYPE_DATA

////////////////////////////////////////
// Data section
////////////////////////////////////////
static struct PluginInfo oPluginInfo =
{
		sizeof( struct PluginInfo ),
		PLUGIN_TYPE_DATA,		
		PLUGIN_VERSION,
		PLUGIN_ID,
		PLUGIN_NAME,
		VENDOR_NAME,
		0,
		387000
};

static struct PluginInfo oPluginInfoAfl =
{
		sizeof( struct PluginInfo ),
		PLUGIN_TYPE_AFL,		
		PLUGIN_VERSION,
		PLUGIN_ID,
		PLUGIN_NAME_AFL,
		VENDOR_NAME,
		0,
		387000
};


// the site interface for callbacks
struct SiteInterface gSite;

/* ideepcoder Feature patch : 20260105, include DB_NAME in registry subkey under TJP */
CString g_oDbName = "Data";					// DB Name for Registry key

void ConfigSetDbName( LPCTSTR pszPath );	// ideepcoder: added fwd declaration

///////////////////////////////////////////////////////////
// Basic plug-in interface functions exported by DLL
///////////////////////////////////////////////////////////


PLUGINAPI int GetPluginInfo( struct PluginInfo *pInfo ) 
{ 
	AFX_MANAGE_STATE( AfxGetStaticModuleState() );

	char szThisPluginName[ _MAX_PATH ];

	GetModuleFileName( AfxGetInstanceHandle(), szThisPluginName, sizeof( szThisPluginName ) );

	if( strstr( _strupr( szThisPluginName ), "ODBCA" ) != 0 )
	{
		*pInfo = oPluginInfoAfl;
	}
	else
	{
		*pInfo = oPluginInfo;
	}

	return TRUE;
}



PLUGINAPI int Init(void) 
{ 
	AFX_MANAGE_STATE( AfxGetStaticModuleState() );

	g_oData.Load();
	return 1;
}	 

PLUGINAPI int Release(void) 
{ 
	return 1; 	  // default implementation does nothing
} 

PLUGINAPI int SetSiteInterface( struct SiteInterface *pInterface )
{
	gSite = *pInterface;

	return TRUE;
}


PLUGINAPI int GetFunctionTable( FunctionTag **ppFunctionTable )
{
	*ppFunctionTable = gFunctionTable;

	// must return the number of functions in the table
	return gFunctionTableSize;
}

#define MY_TIMER_ID 5678

VOID CALLBACK TimerFunc(	HWND hwnd,     // handle of window for timer messages
							UINT uMsg,     // WM_TIMER message
							UINT_PTR idEvent,  // timer identifier
							DWORD dwTime   // current system time
)
{
	if( idEvent == MY_TIMER_ID )
	{
		PostMessage( g_oData.m_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE, 0, 0 );
	}

}

PLUGINAPI int Notify(struct PluginNotification *pn) 
{ 
	AFX_MANAGE_STATE( AfxGetStaticModuleState() );

	///////////////////
	// Check if Data manager is running
	//////////////////
	if( ( pn->nReason & REASON_DATABASE_LOADED ) )
	{
		g_oData.Connect();

		g_oData.m_hAmiBrokerWnd = pn->hMainWnd;
		if( g_oData.m_bAutoRefresh )
		{
			SetTimer( g_oData.m_hAmiBrokerWnd, MY_TIMER_ID, 5000, TimerFunc );
		}
		else
		{
			KillTimer( g_oData.m_hAmiBrokerWnd, MY_TIMER_ID );
		}
	}

	if( pn->nReason & REASON_DATABASE_UNLOADED ) 
	{
		g_oData.Disconnect();
	}
	return 1;
}	 

// GetQuotes wrapper for LEGACY format support
// convert back and forth between old and new format
//
// WARNING: it is highly inefficient and should be avoided
// So this is left just for maintaning compatibility,
// not for performance
// 
PLUGINAPI int GetQuotes( LPCTSTR pszTicker, int nPeriodicity, int nLastValid,   int nSize, struct QuotationFormat4 *pQuotes )
{
	AFX_MANAGE_STATE( AfxGetStaticModuleState() );

	Quotation *pQuote5 = (struct Quotation *) malloc( nSize * sizeof( Quotation   ) );
	QuotationFormat4 *src = pQuotes; 
	Quotation *dst = pQuote5;

	int i;
	for( i = 0; i <= nLastValid; i++,     src++, dst++ )
	{
		ConvertFormat4Quote( src, dst );
	}

	int nQty = GetQuotesEx( pszTicker, nPeriodicity, nLastValid, nSize, pQuote5,   NULL );
	dst = pQuote5;
	src = pQuotes;

	for( i = 0; i < nQty; i++, dst++,     src++ )
	{
		ConvertFormat5Quote( dst, src );
	}

	free( pQuote5 );

	return nQty;
}

PLUGINAPI int GetQuotesEx( LPCTSTR pszTicker, int nPeriodicity, int nLastValid, int nSize, struct Quotation *pQuotes, GQEContext *pContext  )
{
	// needed because we will use MFC inside this function
	AFX_MANAGE_STATE( AfxGetStaticModuleState() );

	try
	{
		CDatabase *poDB = g_oData.GetQuoteDatabase();

		if( poDB )
		{
			CSRecordset oSet( poDB );

			CString oQuery = g_oData.GetQuoteSQL( pszTicker, nSize ) ;

			int iNumRows = 0;

			TRACE("QUERY: %s\n", oQuery );

			if( oSet.Open( CRecordset::forwardOnly,
							oQuery ) )
			{
				int iBar = nSize - 1;


				CStringArray oArr;

				g_oData.GetFieldArray( oArr );

				short anFieldIndexes[ FIELD_QTY ];


				for( int iField = 0; iField < sizeof( anFieldIndexes )/sizeof( anFieldIndexes[ 0 ] ); iField++ )
				{
					anFieldIndexes[ iField ] = -1;
					
					if( ! oArr[ iField ].IsEmpty() )
					{
						try
						{
							anFieldIndexes[ iField ] = oSet.GetFieldIndexByName( oArr[ iField ] );
							TRACE( "Field %d %s = %d\n", iField, oArr[ iField ], anFieldIndexes[ iField ] );
						}
						catch( CDBException *e )
						{
							CString strFormatted;

							strFormatted.Format(_T("Field '%s' is not found in the recordset.\n\nSQL Query:\n%s\n\nODBC driver returned following exception:\n\n%d\n%s\n%s"), oArr[ iField ], oQuery, e->m_nRetCode, e->m_strError, e->m_strStateNativeOrigin);
							AfxMessageBox( strFormatted );

							e->Delete();

							return nLastValid + 1;
						}

					}
				}

				DATE_TIME_INT nLastDate = -1;

				while( iBar >= 0 && ! oSet.IsEOF() )
				{
					struct Quotation *qt = NULL;

					for( int iField = 0; iField < sizeof( anFieldIndexes )/sizeof( anFieldIndexes[ 0 ] ); iField++ )
					{
						if( anFieldIndexes[ iField ] == -1 ) continue; 

						CDBVariant oVar;
					
						oSet.GetFieldValue( anFieldIndexes[ iField ], oVar, iField == 0 /* date */ ? SQL_C_TIMESTAMP : SQL_C_FLOAT );

						qt = &pQuotes[ iBar ];

						switch( iField )
						{
							case 1:
								qt->Open = oVar.m_fltVal;
								break;
							case 2:
								qt->High = oVar.m_fltVal;
								break;
							case 3:
								qt->Low = oVar.m_fltVal;
								break;
							case 4:
								qt->Price = oVar.m_fltVal;
								break;
							case 5:
								qt->Volume = oVar.m_fltVal;
								break;
							case 6:
								qt->OpenInterest = oVar.m_fltVal;
								break;
							case 0:

								if( nPeriodicity == 24 * 60 * 60 &&
									oVar.m_pdate->hour == 0 &&
									oVar.m_pdate->minute == 0 &&
									oVar.m_pdate->second == 0 )
								{
									qt->DateTime.Date = DAILY_MASK;
								}
								else
								{
									qt->DateTime.Date = 0;
									qt->DateTime.PackDate.Hour = oVar.m_pdate->hour;
									qt->DateTime.PackDate.Minute = oVar.m_pdate->minute;
									qt->DateTime.PackDate.Second = oVar.m_pdate->second;
								}

								qt->DateTime.PackDate.Year = oVar.m_pdate->year;
								qt->DateTime.PackDate.Month = oVar.m_pdate->month;
								qt->DateTime.PackDate.Day = oVar.m_pdate->day;

								break;
							default:
								break;
						}


					}

					if( qt )
					{
						if( nLastDate != -1 && nLastDate < qt->DateTime.Date )
						{
							CString oMsg;
							oMsg.Format("Date order invalid. Records should be sorted in DESCENDING date order.\n\nSQL Query:\n\n%s", oQuery );
							AfxMessageBox( oMsg );
							break;
						}

						nLastDate = qt->DateTime.Date;
					}

					oSet.MoveNext();

					iNumRows++;
					iBar--;
				}

			}

			if( iNumRows > 0 && iNumRows < nSize )
			{
				memmove( pQuotes, &pQuotes[ nSize - iNumRows ], iNumRows * sizeof( struct Quotation ) );
			}

			return iNumRows;
		}
	}
	catch( CDBException *e )
	{	 
		CString strFormatted;

		strFormatted.Format(_T("ODBC driver returned following exception:\n\n%d\n%s\n%s"), e->m_nRetCode, e->m_strError, e->m_strStateNativeOrigin);
		AfxMessageBox( strFormatted );

		e->Delete();
	}

	return nLastValid + 1;

	/*
	// we assume that intraday data files are stored in ASCII subfolder
	// of AmiBroker directory and they have name of <SYMBOL>.AQI
	// and the format of Date(YYMMDD),Time(HHMM),Open,High,Low,Close,Volume
	// and quotes are sorted in ascending order - oldest quote is on the top

	char filename[ 256 ];
	FILE *fh;
	int  iLines = 0;
	
	// format path to the file (we are using relative path)
	sprintf( filename, "ASCII\\%s.AQI", pszTicker );

	// open file for reading
	fh = fopen( filename, "r" );

	// if file is successfully opened read it and fill quotation array
	if( fh )
	{
		char line[ 256 ];

		// read the line of text until the end of text
		// but not more than array size provided by AmiBroker
		while( fgets( line, sizeof( line ), fh ) && iLines < nSize )
		{
			// get array entry
			struct Quotation *qt = &pQuotes[ iLines ];
			
			// parse line contents: divide tokens separated by comma (strtok) and interpret values
			
			// date	and time first
			int datenum = atoi( strtok( line, "," ) );	// YYMMDD
			int timenum = atoi( strtok( NULL, "," ) );	// HHMM

			qt->DateTime.PackDate.Tick = 0;
			qt->DateTime.PackDate.Minute = timenum % 100;
			qt->DateTime.PackDate.Hour = timenum / 100;
			qt->DateTime.PackDate.Year = 100 + datenum / 10000;
			qt->DateTime.PackDate.Month = ( datenum / 100 ) % 100;
			qt->DateTime.PackDate.Day = datenum % 100;

			// now OHLC price fields
			qt->Open = (float) atof( strtok( NULL, "," ) );
			qt->High = (float) atof( strtok( NULL, "," ) );
			qt->Low  = (float) atof( strtok( NULL, "," ) );
			qt->Price = (float) atof( strtok( NULL, "," ) ); // close price

			// ... and Volume
			qt->Volume = atoi( strtok( NULL, ",\n" ) );

			iLines++;
		}

		// close the file once we are done
		fclose( fh );

	}

	// return number of lines read which is equal to
	// number of quotes
	return iLines;	 
	*/

}


PLUGINAPI int Configure( LPCTSTR pszPath, struct InfoSite *pSite )
{
	// AFX_MANAGE_STATE is needed because dialog boxes call MFC and use resources
	// similar line should be added to any other function exported by the plugin
	// if only the function calls MFC or uses DLL resources
	AFX_MANAGE_STATE( AfxGetStaticModuleState() );

	ConfigSetDbName( pszPath );						// ideepcoder feature patch

	CConfigDlg oDlg;
	oDlg.m_pSite = pSite;

	oDlg.DoModal();

	return 1;
}

PLUGINAPI AmiVar GetExtraData( LPCTSTR pszTicker, LPCTSTR pszName, int nArraySize, int nPeriodicity, void* (*pfAlloc)(unsigned int nSize) )
{
	// default implementation does nothing

	AmiVar var;

	var.type = VAR_NONE;
	var.val = 0;
	
	return var;
}

PLUGINAPI int SetTimeBase( int nTimeBase )
{
	// only intraday intervals are supported
	return 1;//return ( nTimeBase < ( 24 * 60 * 60 ) )  ? 1 : 0;
}

/// <summary>
/// ideepcoder feature patch
/// Configure() calls this helper func to set DbName global
/// </summary>
/// <param name="pszPath"></param>
void ConfigSetDbName( LPCTSTR pszPath ) {
	CString p( pszPath );
	int pos = p.ReverseFind( '\\' );					// Trust in AB path is good

	g_oDbName.SetString( p.Right( p.GetLength() - pos - 1 ) ); // New var to store DbName to build  RegKey
}
