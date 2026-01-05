// Global.cpp: implementation of the CGlobal class.
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
#include "Global.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

// ideepcoder: feature patch, DB_NAME in Registry subkey
// not modifying CGlobal, hence using extern global
extern CString  g_oDbName;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

const CString oRegPrefix = _T("ODBC_");		// a prefix following naming convention

CGlobal g_oData;

CGlobal::CGlobal()
{
	m_oClose = _T("CLOSE");
	m_oDate = _T("DATE");
	m_oDSN = _T("");
	m_oHigh = _T("HIGH");
	m_oLow = _T("LOW");
	m_oOpen = _T("OPEN");
	m_oOpenInt = _T("");
	m_bSQLCustomQuery = FALSE;
	m_oSQLQuotations = _T("SELECT Symbol, Date, Open, High, Low, Close, Volume FROM DataTable WHERE Symbol='{SYMBOL}' ORDER BY Date DESC");
	m_oSQLSymbolList = _T("SELECT Symbol FROM DataTable WHERE 1=1 GROUP BY Symbol");
	m_oSymbol = _T("SYMBOL");
	m_oTableName = _T("");
	m_oVolume = _T("VOLUME");

	m_poAflDB = NULL;
	m_poQuoteDB = NULL;

	m_oAflSymbolField = _T("SYMBOL");
	m_oAflDateField = _T("DATE");

	m_bAutoRefresh = FALSE;
	m_hAmiBrokerWnd = NULL;

	m_iServerType = 0;


	m_bDisplayErrors = TRUE;
}

CGlobal::~CGlobal()
{
	delete m_poAflDB;
	delete m_poQuoteDB;
}

void CGlobal::Load()
{
	CString oAbDbName;
	oAbDbName.Format( "%s%s", oRegPrefix.GetString(), g_oDbName.GetString() );

	m_oClose =	 AfxGetApp()->GetProfileString( oAbDbName, "FieldClose", m_oClose );
	m_oOpen  =	 AfxGetApp()->GetProfileString( oAbDbName, "FieldOpen", m_oOpen );
	m_oHigh  =   AfxGetApp()->GetProfileString( oAbDbName, "FieldHigh", m_oHigh );
	m_oLow   =   AfxGetApp()->GetProfileString( oAbDbName, "FieldLow", m_oLow );
	m_oVolume =  AfxGetApp()->GetProfileString( oAbDbName, "FieldVolume", m_oVolume );
	m_oOpenInt = AfxGetApp()->GetProfileString( oAbDbName, "FieldOpenInt", m_oOpenInt );
	m_oSymbol =  AfxGetApp()->GetProfileString( oAbDbName, "FieldSymbol", m_oSymbol );
	m_oDate  =   AfxGetApp()->GetProfileString( oAbDbName, "FieldDate", m_oDate );

	m_oDSN = AfxGetApp()->GetProfileString( oAbDbName, "DataSource", m_oDSN );
	m_oTableName = AfxGetApp()->GetProfileString( oAbDbName, "TableName", m_oTableName );
	m_bSQLCustomQuery = AfxGetApp()->GetProfileInt( oAbDbName, "SQLCustomQuery", m_bSQLCustomQuery );
	m_oSQLQuotations = AfxGetApp()->GetProfileString( oAbDbName, "SQLQuotations", m_oSQLQuotations );
	m_oSQLSymbolList = AfxGetApp()->GetProfileString( oAbDbName, "SQLSymbolList", m_oSQLSymbolList );

	m_bAutoRefresh = AfxGetApp()->GetProfileInt( oAbDbName, "AutoRefresh", 0 );
	m_iServerType = AfxGetApp()->GetProfileInt( oAbDbName, "ServerType", 0 );

	UpdateServerType();
}

void CGlobal::Save()
{
	CString oAbDbName;
	oAbDbName.Format( "%s%s", oRegPrefix.GetString(), g_oDbName.GetString() );

	AfxGetApp()->WriteProfileString( oAbDbName, "FieldClose", m_oClose );
	AfxGetApp()->WriteProfileString( oAbDbName, "FieldOpen", m_oOpen );
	AfxGetApp()->WriteProfileString( oAbDbName, "FieldHigh", m_oHigh );
	AfxGetApp()->WriteProfileString( oAbDbName, "FieldLow", m_oLow );
	AfxGetApp()->WriteProfileString( oAbDbName, "FieldVolume", m_oVolume );
	AfxGetApp()->WriteProfileString( oAbDbName, "FieldOpenInt", m_oOpenInt );
	AfxGetApp()->WriteProfileString( oAbDbName, "FieldSymbol", m_oSymbol );
	AfxGetApp()->WriteProfileString( oAbDbName, "FieldDate", m_oDate );

	AfxGetApp()->WriteProfileString( oAbDbName, "DataSource", m_oDSN );
	AfxGetApp()->WriteProfileString( oAbDbName, "TableName", m_oTableName );
	AfxGetApp()->WriteProfileInt( oAbDbName, "SQLCustomQuery", m_bSQLCustomQuery );
	AfxGetApp()->WriteProfileString( oAbDbName, "SQLQuotations", m_oSQLQuotations );
	AfxGetApp()->WriteProfileString( oAbDbName, "SQLSymbolList", m_oSQLSymbolList );

	AfxGetApp()->WriteProfileInt( oAbDbName, "AutoRefresh", m_bAutoRefresh );
	AfxGetApp()->WriteProfileInt( oAbDbName, "ServerType", m_iServerType );

	UpdateServerType();
}

void CGlobal::Connect()
{
	if( !m_oDSN.IsEmpty() )
	{
		try
		{
			if( m_poQuoteDB == NULL )
				m_poQuoteDB = new CDatabase;

			if( m_poQuoteDB->IsOpen() )
				m_poQuoteDB->Close();

			m_poQuoteDB->OpenEx( m_oDSN );
		}
		catch( CDBException *e )
		{
			CString strFormatted;

			strFormatted.Format(_T("ODBC driver returned following exception:\n\n%d\n%s\n%s"), e->m_nRetCode, e->m_strError, e->m_strStateNativeOrigin);
			AfxMessageBox( strFormatted );

			e->Delete();

			delete m_poQuoteDB;
			m_poQuoteDB = NULL;
		}
	}
}

void CGlobal::Disconnect()
{
	if( m_poQuoteDB ) 
	{
		delete m_poQuoteDB;
		m_poQuoteDB = NULL;
	}
}

CDatabase * CGlobal::GetQuoteDatabase()
{
	return m_poQuoteDB;
}

CString CGlobal::GetQuoteSQL(LPCTSTR pszSymbol, int nSize)
{
	CString oSQL;

	// note LIMIT clause is NOT supported by all sources !

	CString oColumns;

	CStringArray oArray;
	GetFieldArray( oArray );

	for( int iField = 0; iField < oArray.GetSize(); iField++ )
	{
		if( ! oArray[ iField ].IsEmpty() )
		{
			if( ! oColumns.IsEmpty() ) oColumns += ",";
			oColumns += Wrap( oArray[ iField ] );
		}
	}

	if( m_bSQLCustomQuery && !m_oSQLQuotations.IsEmpty() )
	{
	   oSQL = m_oSQLQuotations;

	   oSQL.Replace( "{SYMBOL}", pszSymbol );
	}
	else
	{
		if( m_oTableName.Find( "{SYMBOL}" ) != -1 )
		{
			m_oTableName.Replace( "{SYMBOL}", pszSymbol );

			oSQL.Format("SELECT %s FROM %s ORDER BY %s DESC",
				oColumns,
				Wrap( m_oTableName ),	 
				Wrap( m_oDate ) );
		}
		else
		{
			oSQL.Format("SELECT %s, %s FROM %s WHERE %s='%s' ORDER BY %s DESC",
				Wrap( m_oSymbol ),
				oColumns,
				Wrap( m_oTableName ),
				Wrap( m_oSymbol ),
				pszSymbol,
				Wrap( m_oDate ) );
		}
	}

	if( 0 /*GLOBAL_bUseLimitClause*/ )
	{
		CString oLimit;
		oLimit.Format(" LIMIT %d", nSize );

		oSQL += oLimit;
	}

	return oSQL;
}

void CGlobal::GetFieldArray(CStringArray &oArr)
{
	oArr.SetSize( FIELD_QTY );
	oArr[ 0 ] = m_oDate;
	oArr[ 1 ] = m_oOpen;
	oArr[ 2 ] = m_oHigh;
	oArr[ 3 ] = m_oLow;
	oArr[ 4 ] = m_oClose;
	oArr[ 5 ] = m_oVolume;
	oArr[ 6 ] = m_oOpenInt;

}

BOOL CGlobal::ConnectAFL(LPCTSTR pszConnectString)
{
	if( m_poAflDB && m_poAflDB->IsOpen() &&
		strcmp( m_oAflDSN, pszConnectString ) == 0 ) return TRUE;

	m_oAflDSN = pszConnectString;

	if( !m_oAflDSN.IsEmpty() )
	{
		try
		{
			if( m_poAflDB == NULL )
				m_poAflDB = new CDatabase;

			if( m_poAflDB->IsOpen() )
				m_poAflDB->Close();

			m_poAflDB->OpenEx( m_oAflDSN );
		}
		catch( CDBException *e )
		{
			m_oLastErrorMsg = e->m_strError;


			//e->ReportError();
			e->Delete();

			delete m_poAflDB;
			m_poAflDB = NULL;

			return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

CDatabase * CGlobal::GetAflDatabase()
{
	return m_poAflDB;
}

void CGlobal::StoreErrorMessage(CDBException *e)
{
	m_oLastErrorMsg	= e->m_strError;

	if( m_bDisplayErrors )
	{
		CString strFormatted;

		strFormatted.Format(_T("ODBC driver returned following exception:\n\n%d\n%s\n%s"), e->m_nRetCode, e->m_strError, e->m_strStateNativeOrigin);
		AfxMessageBox( strFormatted );
	}
}





CString CGlobal::Wrap(const CString &src)
{
	return m_oWrapMarks[ 0 ] + src + m_oWrapMarks[ 1 ];
}

void CGlobal::UpdateServerType()
{
	switch( m_iServerType )
	{
	case 0:
		m_oWrapMarks[ 0 ] = "";
		m_oWrapMarks[ 1 ] = "";
		break;
	case 1:
		m_oWrapMarks[ 0 ] = "`";
		m_oWrapMarks[ 1 ] = "`";
		break;
	case 2:
	case 3:
		m_oWrapMarks[ 0 ] = "[";
		m_oWrapMarks[ 1 ] = "]";
		break;
	default:
		ASSERT(0);
		break;
	}
}
