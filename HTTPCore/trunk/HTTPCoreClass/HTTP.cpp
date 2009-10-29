#include <stdio.h>
#include "Build.h"
#include "HTTP.h"
#include "ConnectionHandling.h"
#include "CookieHandling.h"
#include <stdlib.h>

#include "Authentication/digest.h"
#include "Authentication/ntlm.h"
#include "Authentication/base64.h"

#include "Modules/Encoding_Chunked.h"
#include "Modules/Encoding_Deflate.h"
#ifdef __WIN32__RELEASE__
#pragma comment(lib,"ws2_32.lib")
#endif


#define MAX_HEADER_SIZE     8192
#define MAX_POST_LENGTH  	8192
#define MAX_DOWNLOAD_SIZE 	MAX_POST_LENGTH*20
#define PURGETIME						20  //20 secconds
#define MAX_INACTIVE_CONNECTION 		10000000 *PURGETIME
#define FHSCANUSERAGENT 				"Mozilla/5.0 (compatible; MSIE 7.0; FHScan Core 1.3)"

#include <iostream>
#include <string>
using namespace std;

struct params {
	void *classptr;
	void *ListeningConnectionptr;
};

/*
- TODO:
- Allow local and remote DNS resolution when working against a Global HTTP proxy
- Replace char , "const char", "const HTTPSTR" and "HTTPSTR" with HTTPCHR / HTTPCSTR / HTTPSTR to migrate the core to wchar
*/
/*******************************************************************************************************/
/*******************************************************************************************************/
/*******************************************************************************************************/
prequest::prequest()
{
	*hostname=0;
	ip=0;
	port = 0;
	NeedSSL = 0;
	url = NULL;
	Parameters = NULL;
	request = NULL;
	response = NULL;
	server = NULL;
	*Method=0;
	status=0;
	challenge = 0;
	ContentType = NULL;
}
/*******************************************************************************************/
prequest::~prequest()
{	
	delete request;
	delete response;
	if (server)			free(server);
	if (ContentType)	free(ContentType);
	if (url)			free(url);
	if (Parameters)		free(Parameters);
}

/*******************************************************************************************************/
/*******************************************************************************************************/
/*******************************************************************************************************/
int ThreadFunc(void *foo)
{
	HTTPAPI *api = (HTTPAPI*)foo;
	api->CleanConnectionTable(NULL);
	return(0);
}
/*******************************************************************************************************/
#ifdef _OPENSSL_SUPPORT_
int InitializeSSLLibrary();
#endif
/*******************************************************************************************/
HTTPAPI::HTTPAPI()
{
#ifdef __WIN32__RELEASE__
	WSAStartup( MAKEWORD(2,2), &ws );
#endif

	for(int i=0;i<MAXIMUM_OPENED_HANDLES;i++) HTTPHandleTable[i]=NULL;
#ifdef _OPENSSL_SUPPORT_
	InitializeSSLLibrary();
#endif

	for (int i = 0;i< MAX_OPEN_CONNECTIONS; i++)
	{
		Connection_Table[i] = new ConnectionHandling;
	}

	FHScanUserAgent= strdup(FHSCANUSERAGENT);

	HandleLock.InitThread((void*)ThreadFunc,(void*)this); 

	HTTPCallBack.SetHTTPApiInstance((void*)this);
	HTTPCallBack.RegisterHTTPCallBack( CBTYPE_CLIENT_RESPONSE | CBTYPE_PROXY_RESPONSE, (HTTP_IO_REQUEST_CALLBACK)CBDecodeChunk,"HTTP Chunk encoding decoder");
#ifdef _ZLIB_SUPPORT_
	HTTPCallBack.RegisterHTTPCallBack( CBTYPE_CLIENT_REQUEST | CBTYPE_CLIENT_RESPONSE, (HTTP_IO_REQUEST_CALLBACK)CBDeflate,"HTTP Gzip / Deflate decoder");
#endif
	BindPort = 0;
	COOKIE =  new CookieStatus();
}
/*******************************************************************************************************/
HTTPAPI::~HTTPAPI()
{
	if (BindPort)
	{
		StopHTTPProxy();
	}
	HandleLock.EndThread();

	for (int i = 0;i< MAX_OPEN_CONNECTIONS; i++)
	{
		delete Connection_Table[i]; 	
	}

	for(int i=0;i<MAXIMUM_OPENED_HANDLES;i++)
	{
		if (HTTPHandleTable[i])
		{
			delete HTTPHandleTable[i];
			HTTPHandleTable[i] = NULL;
		}
	}
	if (FHScanUserAgent)
	{
		free(FHScanUserAgent);
		FHScanUserAgent=NULL;
	}
	delete COOKIE;

#ifdef __WIN32__RELEASE__
	WSACleanup();
#endif

}
/*******************************************************************************************************/
int HTTPAPI::SetHTTPConfig(HTTPHANDLE HTTPHandle,int opt, HTTPCSTR parameter)
{
	if (HTTPHandle == GLOBAL_HTTP_CONFIG)
	{
		return ( GlobalHTTPCoreApiOptions.SetHTTPConfig(opt,parameter) );
	} else
	{
		if ((HTTPHandle>=0) && (HTTPHandle<MAXIMUM_OPENED_HANDLES) )
		{
			return ( HTTPHandleTable[HTTPHandle]->SetHTTPConfig(opt,parameter) );
		}
	}
	return(0);
}
/*******************************************************************************************************/
int HTTPAPI::SetHTTPConfig(HTTPHANDLE HTTPHandle,int opt, int parameter)
{
	if (HTTPHandle == GLOBAL_HTTP_CONFIG)
	{
		return ( GlobalHTTPCoreApiOptions.SetHTTPConfig(opt,parameter) );
	} else
	{
		if ((HTTPHandle>=0) && (HTTPHandle<MAXIMUM_OPENED_HANDLES) )
		{
			return ( HTTPHandleTable[HTTPHandle]->SetHTTPConfig(opt,parameter) );
		}
	}
	return(0);
}
/*******************************************************************************************************/
HTTPSTR	HTTPAPI::GetHTTPConfig(HTTPHANDLE HTTPHandle,int opt)
{
	if (HTTPHandle == GLOBAL_HTTP_CONFIG)
	{
		return ( GlobalHTTPCoreApiOptions.GetHTTPConfig(opt) );
	} else
	{
		if ((HTTPHandle>=0) && (HTTPHandle<MAXIMUM_OPENED_HANDLES) )
		{
			return ( HTTPHandleTable[HTTPHandle]->GetHTTPConfig(opt) );
		} 
	}
	return(0);
}
/*******************************************************************************************************/
HTTPHANDLE HTTPAPI::InitHTTPConnectionHandle(HTTPSTR lpHostName,int port,int ssl)
{
	class HHANDLE *HTTPHandle= new HHANDLE;
	if (HTTPHandle->InitHandle(lpHostName,port,ssl))
	{
		HandleLock.LockMutex();
		for (int i=0;i<MAXIMUM_OPENED_HANDLES;i++)
		{
			if (HTTPHandleTable[i]==NULL)
			{
				HTTPHandleTable[i]=HTTPHandle;
				HandleLock.UnLockMutex();

				HTTPHandle->SetHTTPConfig(OPT_HTTP_HANDLE_COOKIES,GlobalHTTPCoreApiOptions.GetHTTPConfig(OPT_HTTP_HANDLE_COOKIES));
				HTTPHandle->SetHTTPConfig(OPT_HTTP_AUTOREDIRECT,GlobalHTTPCoreApiOptions.GetHTTPConfig(OPT_HTTP_AUTOREDIRECT));

				if (GlobalHTTPCoreApiOptions.ProxyEnabled() )
				{
					HTTPHandle->SetHTTPConfig(OPT_HTTP_PROXY_HOST,GlobalHTTPCoreApiOptions.GetHTTPConfig(OPT_HTTP_PROXY_HOST));
					HTTPHandle->SetHTTPConfig(OPT_HTTP_PROXY_PORT,GlobalHTTPCoreApiOptions.GetHTTPConfig(OPT_HTTP_PROXY_PORT));
					if (GlobalHTTPCoreApiOptions.GetHTTPConfig(OPT_HTTP_PROXY_USER))
					{
						HTTPHandle->SetHTTPConfig(OPT_HTTP_PROXY_USER,GlobalHTTPCoreApiOptions.GetHTTPConfig(OPT_HTTP_PROXY_USER));
						HTTPHandle->SetHTTPConfig(OPT_HTTP_PROXY_PASS,GlobalHTTPCoreApiOptions.GetHTTPConfig(OPT_HTTP_PROXY_PASS));
					}
					/*set additional Global HTTP options if needed ... */
				}
				return(i);
			}
		}
		HandleLock.UnLockMutex();
	}
	delete HTTPHandle;
	return(-1);
}

/*******************************************************************************************************/

int HTTPAPI::EndHTTPConnectionHandle(HTTPHANDLE UserHandle)
{
	if ((UserHandle>=0) && (UserHandle<MAXIMUM_OPENED_HANDLES))
	{
		HandleLock.LockMutex();
		if (HTTPHandleTable[UserHandle]!=NULL)
		{
			delete HTTPHandleTable[UserHandle];
			HTTPHandleTable[UserHandle] = NULL;
			HandleLock.UnLockMutex();
			return(1);
		}
		HandleLock.UnLockMutex();
	}
	return(0);
}
/*******************************************************************************************************/
class HHANDLE *HTTPAPI::GetHHANDLE(HTTPHANDLE HTTPHandle)
{
	if ((HTTPHandle>=0) && (HTTPHandle<MAXIMUM_OPENED_HANDLES) )
	{
		return(HTTPHandleTable[HTTPHandle]);
	}
	return (NULL);
}
/*******************************************************************************************************/
HTTPAPI::HTTPAPI(HTTPSTR lpProxyHost, HTTPSTR lpProxyPort, HTTPSTR lpUserName = NULL, HTTPSTR lpPassword = NULL)
{
	HTTPAPI();
	GlobalHTTPCoreApiOptions.SetHTTPConfig(OPT_HTTP_PROXY_HOST,lpProxyHost);
	GlobalHTTPCoreApiOptions.SetHTTPConfig(OPT_HTTP_PROXY_PORT,lpProxyPort);
	if (lpUserName)
	{
		GlobalHTTPCoreApiOptions.SetHTTPConfig(OPT_HTTP_PROXY_USER,lpUserName);
		GlobalHTTPCoreApiOptions.SetHTTPConfig(OPT_HTTP_PROXY_PASS,lpPassword);
	}
}
/*******************************************************************************************************/
void  HTTPAPI::CleanConnectionTable(LPVOID *unused)
{

#ifdef __WIN32__RELEASE__
	FILETIME fcurrenttime;
	LARGE_INTEGER CurrentTime;
	LARGE_INTEGER LastUsedTime;
#else
	time_t fcurrenttime;
#endif
	while(1)
	{
		lock.LockMutex();

#ifdef __WIN32__RELEASE__
		GetSystemTimeAsFileTime(&fcurrenttime);
		CurrentTime.LowPart=fcurrenttime.dwLowDateTime;
		CurrentTime.HighPart=fcurrenttime.dwHighDateTime;
#else
		time(&fcurrenttime);
#endif
		for(int i=0; i<MAX_OPEN_CONNECTIONS;i++)
		{
			if ( (Connection_Table[i]->GetTarget()!=TARGET_FREE) && (!Connection_Table[i]->Getio()) && (!Connection_Table[i]->GetPENDINGPIPELINEREQUESTS()) )
			{
				//TODO: El recurso .io deberia estar protegido por el mutex de conexion[i].lock ?
#ifdef __WIN32__RELEASE__
				LastUsedTime.HighPart= Connection_Table[i]->tlastused.dwHighDateTime;
				LastUsedTime.LowPart= Connection_Table[i]->tlastused.dwLowDateTime;
				if ( (CurrentTime.QuadPart - LastUsedTime.QuadPart) > MAX_INACTIVE_CONNECTION )
#else
				if ( (fcurrenttime - Connection_Table[i]->tlastused)> MAX_INACTIVE_CONNECTION )
#endif
				{

#ifdef _DBG_
					printf("DBG: Eliminando conexion %3.3i against %s:%i \n",i,Connection_Table[i].targetDNS,Connection_Table[i].port);
#endif
					Connection_Table[i]->FreeConnection();
				}
			}
		}
		lock.UnLockMutex();
		Sleep(5000);
	}

}

/*******************************************************************************************************/
class ConnectionHandling *HTTPAPI::GetSocketConnection(class HHANDLE *HTTPHandle, PHTTP_DATA request, unsigned long *id)
{
	lock.LockMutex();

	class ConnectionHandling *connection = (class ConnectionHandling *) HTTPHandle->GetConnection();

	/* Seach if the HANDLE is already binded to a CONNECTION struct */
	if ( (request) && (connection) && 	
		(connection->GetTarget()==HTTPHandle->GetTarget()) && 
		(connection->GetThreadID()==HTTPHandle->GetThreadID()) && 
		( (connection->GetPort()==HTTPHandle->GetPort()) || ((connection->GetConnectionAgainstProxy()) && (connection->GetPort()==HTTPHandle->GetPort()) ) ) )
	{ 
		if  (!connection->Getio())
		{
#ifdef _DBG_
			printf("[DBG]: Direct Reuse Connection %3.3i- (%3.3i requests against %s)\n",HTTPHandle->conexion->id,HTTPHandle->conexion->NumberOfRequests,HTTPHandle->targetDNS);
#endif
			*id=connection->AddPipeLineRequest(request);
		} else
		{
			while (!connection->Getio())
			{
#ifdef _DBG_
				printf("[DBG]: Thread %i Waiting for io\n",*id);
#endif
				Sleep(500);
			}
			*id=connection->AddPipeLineRequest(request);
		}

		lock.UnLockMutex();	
		return(connection);
	} 

	/*Search For stablished connections that are not currently used. */
	int FirstIdleSlot=GetFirstIdleConnectionAgainstTarget(HTTPHandle);
	if (FirstIdleSlot!=-1) //Idle Connection Found. Reuse connection
	{
#ifdef _DBG_
		printf("[DBG]: Reuse Connection %3.3i  - %3.3i requests against %s\n",FirstIdleSlot,Connection_Table[FirstIdleSlot].NumberOfRequests,HTTPHandle->targetDNS);
#endif
		HTTPHandle->SetConnection((void*)Connection_Table[FirstIdleSlot]);
		if (request) *id = Connection_Table[FirstIdleSlot]->AddPipeLineRequest(request);
		lock.UnLockMutex();
		return(Connection_Table[FirstIdleSlot]);
	}

	/* Search for a free slot and then connect to the remote target */
	int FirstEmptySlot=GetFirstUnUsedConnection();
	if (FirstEmptySlot==-1) //Connection table full. Try Again Later
	{
#ifdef _DBG_
		printf("[DBG]: Unable to get a free Socket connection against target. Maybe your application is too aggresive");
		printf("UNABLE TO GET FREE SOCKET!!!\n");
#endif		
		lock.UnLockMutex();
		*id = 0;
		return(NULL);
	}

	/* Stablish a new connection against the remote HTTP Host */
	lock.UnLockMutex();
	int ret = Connection_Table[FirstEmptySlot]->GetConnection(HTTPHandle);

	if (!ret)
	{
#ifdef _DBG_
		printf("GetSocketConnection:  Connection Failed\n");
#endif
		lock.LockMutex();
		Connection_Table[FirstEmptySlot]->FreeConnection();
		lock.UnLockMutex();
		*id = 0;
		return(NULL);
	}

	HTTPHandle->SetConnection((void*)Connection_Table[FirstEmptySlot]);
	lock.LockMutex();
	if (request) *id=Connection_Table[FirstEmptySlot]->AddPipeLineRequest(request);
	Connection_Table[FirstEmptySlot]->Setio(0);
	lock.UnLockMutex();

	return(Connection_Table[FirstEmptySlot]);
}

/*******************************************************************************************/
//! This function checks the connection table searching for a free and inactive connection against the remote host.
/*!
\param HTTPHandle Connection handle.
\return Returns an index to the CONEXION item in the connection table.
\note This function is only used internally by HTTPCore Module.
\note If no connection matches, -1 is returned.
*/
/*******************************************************************************************/
int HTTPAPI::GetFirstIdleConnectionAgainstTarget(class HHANDLE *HTTPHandle)
{
	for(unsigned int i=0;i<MAX_OPEN_CONNECTIONS;i++)
	{
		if ( (Connection_Table[i]->GetThreadID()==HTTPHandle->GetThreadID()) && 
			(Connection_Table[i]->GetTarget()==HTTPHandle->GetTarget()) && 
			(Connection_Table[i]->GetPort()==HTTPHandle->GetPort()) )
		{
			if (Connection_Table[i]->Getio()==0)
			{
				return(i);
			}
		}
	}
	return(-1); //Connection Not Found
}
/*******************************************************************************************************/
//! This function checks the connection table searching for the first unused connection.
/*!
\return Returns an index to the CONEXION item in the connection table.
\note This function is only used internally by HTTPCore Module.
\note If no free connection is found, -1 is returned.
*/
/*******************************************************************************************/
int HTTPAPI::GetFirstUnUsedConnection()
{
	for(unsigned int i=0;i<MAX_OPEN_CONNECTIONS;i++)
	{
		if ( (Connection_Table[i]->GetTarget()==TARGET_FREE) && (!Connection_Table[i]->Getio()) && (!Connection_Table[i]->GetPENDINGPIPELINEREQUESTS()) )
		{
			return(i);
		}
	}
	return(-1);
}
/*******************************************************************************************************/
PHTTP_DATA HTTPAPI::DispatchHTTPRequest(HTTPHANDLE HTTPHandle,PHTTP_DATA request)
{
	PHTTP_DATA response = NULL;
	class ConnectionHandling *conexion;
	unsigned long ret = CBRET_STATUS_NEXT_CB_CONTINUE;
	unsigned long RequestID;


	try
	{

		if (request)
		{

			ret = HTTPCallBack.DoCallBack(CBTYPE_CLIENT_REQUEST ,HTTPHandle,request,response);

			if (ret & CBRET_STATUS_CANCEL_REQUEST)
			{
				return(response);
			}
			conexion=GetSocketConnection(HTTPHandleTable[HTTPHandle],request,&RequestID);
			if (!conexion)
			{
				return(NULL);
			}
			conexion->SendHTTPRequest(request);
		} else
		{
			conexion=GetSocketConnection(HTTPHandleTable[HTTPHandle],request,&RequestID);
		}

		while (RequestID != conexion->GetPIPELINERequestID()[0])
		{
#ifdef _DBG_
			printf("Waiting for ReadHTTPResponseData() %d / %d\n",RequestID,conexion->PIPELINE_Request_ID[0]);
#endif
			Sleep(100);
		}
#ifdef _DBG_
		printf("LECTURA: LEYENDO PETICION %i en conexion %i\n",RequestID,conexion->id);
#endif

		response = conexion->ReadHTTPResponseData((ConnectionHandling*) HTTPHandleTable[HTTPHandle]->GetClientConnection(),request,&lock);

		ret = HTTPCallBack.DoCallBack(CBTYPE_CLIENT_RESPONSE ,HTTPHandle,request,response);

		if (ret & CBRET_STATUS_CANCEL_REQUEST)
		{
			delete response;
			return(NULL);
		}


	}
	catch(...)
	{

		try
		{
			printf("#FATAL# ::DispatchHTTPRequest exception catch()\n");
			return (NULL);
		}
		catch(...)
		{


		}

	}

	return(response);


}

/*******************************************************************************************************/
PHTTP_DATA HTTPAPI::BuildHTTPRequest(
								  HTTPHANDLE HTTPHandle,
								  HTTPCSTR VHost,
								  HTTPCSTR HTTPMethod,
								  HTTPCSTR url,
								  HTTPCSTR PostData,
								  unsigned int PostDataSize)
{
	if ( (!url) || (*url!='/') )
	{
		return ( NULL);
	}

	class HHANDLE *RealHTTPHandle=(class HHANDLE *)HTTPHandleTable[HTTPHandle];
	HTTPCHAR		tmp[MAX_HEADER_SIZE];
	tmp[MAX_HEADER_SIZE-1]=0;

	if ( (RealHTTPHandle->ProxyEnabled()) && (RealHTTPHandle->IsSSLNeeded()) &&
       ( (!RealHTTPHandle->GetConnection()) ||
		( (RealHTTPHandle->GetConnection())  &&
		(!((class ConnectionHandling*)RealHTTPHandle->GetConnection())->IsSSLInitialized())  ) ) )
	{
		/*
		* We have to deal with an HTTPS request thought a proxy server 
		* Under this scenario we first need to send an initial non SSL request to the proxy
		* We need to send a "CONNECT" verb to the HTTP Proxy Server
		*/
		snprintf(tmp,sizeof(tmp)-1,"CONNECT %s:%i HTTP/1.1\r\n\r\n",RealHTTPHandle->GettargetDNS(),RealHTTPHandle->GetPort());
		PHTTP_DATA request = new httpdata(tmp,(int)strlen(tmp));

		if ( (RealHTTPHandle->GetlpProxyUserName()) && (RealHTTPHandle->GetlpProxyPassword())  )
		{
			BuildBasicAuthHeader("Proxy-Authorization",RealHTTPHandle->GetlpProxyUserName(),RealHTTPHandle->GetlpProxyPassword(),tmp,sizeof(tmp));
			request->AddHeader(tmp);
		}
		return(request);
	}

	if ( (RealHTTPHandle->ProxyEnabled()) && (!RealHTTPHandle->IsSSLNeeded()) )
	{
		snprintf(tmp,MAX_HEADER_SIZE-1,"%s http://%s:%i%s HTTP/1.%i\r\n",HTTPMethod,RealHTTPHandle->GettargetDNS(),RealHTTPHandle->GetPort(),url,RealHTTPHandle->GetVersion());
	} else
	{
		if ( (strncmp(HTTPMethod,"GET",3)!=0) || (!PostDataSize) )
		{
			snprintf(tmp,MAX_HEADER_SIZE-1,"%s %s HTTP/1.%i\r\n",HTTPMethod,url,RealHTTPHandle->GetVersion());
		} else
		{
			snprintf(tmp,MAX_HEADER_SIZE-1,"GET %s?%s HTTP/1.%i\r\n",url,PostData,RealHTTPHandle->GetVersion());
		}
	}
	string rb = tmp;

	/* Append the Host header */
	if (VHost)
	{
		snprintf(tmp,MAX_HEADER_SIZE-1,"Host: %s\r\n",VHost);
	} else
	{
		snprintf(tmp,MAX_HEADER_SIZE-1,"Host: %s\r\n",RealHTTPHandle->GettargetDNS());
	}
	rb+=tmp;

	/* Append FHSCAN User Agent */
	if (RealHTTPHandle->GetUserAgent())
	{
		snprintf(tmp,MAX_HEADER_SIZE-1,"User-Agent: %s\r\n",RealHTTPHandle->GetUserAgent());		
	} else
	{
		snprintf(tmp,MAX_HEADER_SIZE-1,"User-Agent: %s\r\n",FHScanUserAgent);		
	}
	rb += tmp;

	/* Append Custom user headers */
	if (RealHTTPHandle->GetAdditionalHeader())
	{
		rb +=RealHTTPHandle->GetAdditionalHeader();
	}

	if (RealHTTPHandle->IsCookieSupported())
	{ /* Include Cookies stored into the internal COOKIE bTree */
		char *lpPath = GetPathFromURL(url);
		char *ServerCookie = BuildCookiesFromStoredData( RealHTTPHandle->GettargetDNS(),lpPath,RealHTTPHandle->IsSSLNeeded());
		free(lpPath);
		if (ServerCookie)
		{
            rb+="Cookie: ";
			rb+=ServerCookie;
			rb+="\r\n";
			free(ServerCookie);
		}
	}

	if (RealHTTPHandle->GetCookie())
	{ /* Append additional cookies provided by the user - This code should be updated - TODO*/
		snprintf(tmp,MAX_HEADER_SIZE-1,"%s\r\n",RealHTTPHandle->GetCookie());
		rb+=tmp;
	}

	if ( (RealHTTPHandle->ProxyEnabled()) && (!RealHTTPHandle->IsSSLNeeded()) )
	{   /* Add Keep Alive Headers */
		rb+="Proxy-Connection: Keep-Alive\r\n";
		if ( (RealHTTPHandle->GetlpProxyUserName()) && (RealHTTPHandle->GetlpProxyPassword()) )
		{   /* Add Proxy Autentication Headers */
			BuildBasicAuthHeader("Proxy-Authorization",RealHTTPHandle->GetlpProxyUserName(),RealHTTPHandle->GetlpProxyPassword(),tmp,sizeof(tmp));
			rb+=tmp;
		}
	} else
	{
		rb+="Connection: Keep-Alive\r\n";
	}

	if  (  (strncmp(HTTPMethod,"GET",3)!=0) && ((PostDataSize) ||  (strncmp(HTTPMethod,"POST",4)==0)))
	{   /* Set the Content-Type header and inspect if have already been added to avoid dups*/
		HTTPSTR contenttype = RealHTTPHandle->GetAdditionalHeaderValue("Content-Type:",0);
		if (contenttype)
		{	
			free(contenttype);
			snprintf(tmp,MAX_HEADER_SIZE-1,"Content-Length: %i\r\n",PostDataSize);

		} else
		{
			snprintf(tmp,MAX_HEADER_SIZE-1,"Content-Type: application/x-www-form-urlencoded\r\nContent-Length: %i\r\n",PostDataSize);
		}
		rb+=tmp;
	}
	rb+="\r\n";

	return (new httpdata(rb.c_str(),(unsigned int)rb.length(),PostData,PostDataSize) );
}


/**************************************************************************************************/
/**************************************************************************************************/
/**************************************************************************************************/

PREQUEST HTTPAPI::SendHttpRequest(HTTPHANDLE HTTPHandle,PHTTP_DATA request,HTTPCSTR lpUsername,HTTPCSTR lpPassword,int AuthMethod)
{

	PHTTP_DATA 		response=NULL;
	HTTPCHAR		tmp[MAX_HEADER_SIZE];
	tmp[MAX_HEADER_SIZE-1]=0;	
	char HTTPMethod[20];
	char url[4096];
	char *p,*q;


	class HHANDLE *RealHTTPHandle=(class HHANDLE *)HTTPHandleTable[HTTPHandle];
	if ( (AuthMethod) && (lpUsername) && (lpPassword) )/* Deal with authentication */
	{   
		switch (AuthMethod)
		{
		case BASIC_AUTH:
			BuildBasicAuthHeader("Authorization",lpUsername,lpPassword,tmp,MAX_HEADER_SIZE);
			request->AddHeader(tmp);
			break;
		case DIGEST_AUTH:						
			p=strchr(request->Header,' ');
			if (p)
			{
				*p=0;
				strncpy(HTTPMethod,request->Header,sizeof(HTTPMethod)-1);
				HTTPMethod[sizeof(HTTPMethod)-1]=0;
				*p=' '; p++;
				q=strchr(p,' ');
				if (q)
				{
					*q=0;
					strncpy(url,p,sizeof(url)-1);
					url[sizeof(url)-1]=0;
					*q=' ';
				}
			}
			if ( (!RealHTTPHandle->GetLastRequestedUri()) || (strcmp(RealHTTPHandle->GetLastRequestedUri(),url)!=0) && (RealHTTPHandle->GetLastAuthenticationString()==NULL) )
			{   /*Send another request to check if authentication is required to get the www-authenticate header */
				/* We cant reuse RealHTTPHandle->LastAuthenticationString now*/
				response=DispatchHTTPRequest(HTTPHandle,request);				
				if (!response)
				{
					return(NULL);
				} else 
				{
					RealHTTPHandle->SetLastRequestedUri(url);
					if (( response->HeaderSize>=12) && (memcmp(response->Header+9,"401",3)!=0) )
					{   
						break;
					} else 
					{
						RealHTTPHandle->SetLastAuthenticationString(response->GetHeaderValue("WWW-Authenticate: Digest ",0));
						delete response; response = NULL;
					}
				}
			}
			HTTPSTR AuthenticationHeader;
			if (AuthenticationHeader=CreateDigestAuth(RealHTTPHandle->GetLastAuthenticationString(),lpUsername,lpPassword,HTTPMethod,url,0))
			{
				request->AddHeader(AuthenticationHeader);
				free(AuthenticationHeader);
			} else
			{
				RealHTTPHandle->SetLastAuthenticationString(NULL);
			}
			break;

		case NTLM_AUTH:
		case NEGOTIATE_AUTH:
			unsigned char buf2[4096];
			unsigned char buf1[4096];
			memset(buf1,'\0',sizeof(buf1));
			memset(buf2,'\0',sizeof(buf2));

			BuildAuthRequest((tSmbNtlmAuthRequest*)buf2,0,NULL,NULL);
			to64frombits(buf1, buf2, (int)SmbLength((tSmbNtlmAuthResponse*)buf2));
			snprintf(tmp,MAX_HEADER_SIZE-1,"Authorization: NTLM %s\r\n",buf1);
			request->AddHeader(tmp);

			response=DispatchHTTPRequest(HTTPHandle,request);
			request->RemoveHeader("Authorization:");
			RealHTTPHandle->SetLastRequestedUri(url);
			if (!response)
			{ /* NTLM Negotiation failed */
				return(NULL);
			}
			if (( response->HeaderSize>=12) &&(memcmp(response->Header+9,"401",3)==0))
			{   /*Parse NTLM Message Type 2 */
				HTTPSTR NTLMresponse = response->GetHeaderValue("WWW-Authenticate: NTLM ",0);
				if (!NTLMresponse)  break;  /* WWW-Authenticate: NTLM Header not Found */
				from64tobits((HTTPSTR )&buf1[0], NTLMresponse); /* Build NTLM Message Type 3 */
				buildAuthResponse((tSmbNtlmAuthChallenge*)buf1,(tSmbNtlmAuthResponse*)buf2,0,lpUsername,lpPassword,NULL,NULL);
				to64frombits(buf1, buf2, (int)SmbLength((tSmbNtlmAuthResponse*)buf2));
				snprintf(tmp,MAX_HEADER_SIZE-1,"Authorization: NTLM %s\r\n",buf1);
                request->AddHeader(tmp);
				free(NTLMresponse);
				delete(response); response = NULL;

			} else
			{   /* The server does not requiere NTLM authentication or weird anonymous authentication supported? (only NTLM type 1 sent)*/
				break;
			}
			break;
		}
	}

	/* Authentication Headers - if needed - have been added */
	if (!response)
	{
		response=DispatchHTTPRequest(HTTPHandle,request);
    }
	if (!response)
	{
		RealHTTPHandle->SetLastRequestedUri(NULL);
		return(NULL);
	}

	if ( (RealHTTPHandle->ProxyEnabled()) && (RealHTTPHandle->IsSSLNeeded()) &&
	   ( (!RealHTTPHandle->GetConnection()) ||
	   ( (RealHTTPHandle->GetConnection())  &&
	     (!((class ConnectionHandling*)RealHTTPHandle->GetConnection())->IsSSLInitialized())  ) ) )
	{
		RealHTTPHandle->SetLastRequestedUri(NULL);
		if (( response->HeaderSize>=12) && (memcmp(response->Header+9,"200",3)==0))
		{
			/* Send the real http request thought stablished proxy connection */
			return SendHttpRequest(HTTPHandle,request,lpUsername,lpPassword,AuthMethod);
		} else 
		{
			/* Return a proxy error message */
			if ( (RealHTTPHandle->GetConnection()) && (((class ConnectionHandling*)RealHTTPHandle->GetConnection())->IsSSLInitialized()) )
			{
				((class ConnectionHandling*)RealHTTPHandle->GetConnection())->FreeConnection();
				RealHTTPHandle->SetConnection(NULL);
			}		
			return((PREQUEST)RealHTTPHandle->ParseReturnedBuffer(request, response));
		} 
	}

	RealHTTPHandle->SetLastRequestedUri(url);
	PREQUEST DATA=(PREQUEST)RealHTTPHandle->ParseReturnedBuffer(request, response);

	if ( (DATA) && (DATA->challenge) && (DATA->status==401) && (!AuthMethod) && (lpUsername) && (lpPassword)  )
	{   /* Send Authentication request and return the "authenticated" response */
		PREQUEST AUTHDATA=SendHttpRequest(HTTPHandle,request,lpUsername,lpPassword,DATA->challenge);
		if (AUTHDATA)
		{
			DATA->request = NULL; /* We are reutilizing the same request, so avoid deleting memory twice */
			delete DATA;
			return(AUTHDATA);
		}
	}
	if (RealHTTPHandle->IsCookieSupported())
	{
		char *lpPath =  GetPathFromURL(url);
		ExtractCookiesFromResponseData(response, lpPath,RealHTTPHandle->GettargetDNS());
		free(lpPath);
	}

	#define ISREDIRECT(a) ((a==HTTP_STATUS_MOVED) || (a ==HTTP_STATUS_REDIRECT) || (a==HTTP_STATUS_REDIRECT_METHOD) )
	if ( (RealHTTPHandle->IsAutoRedirectEnabled()) && ISREDIRECT(DATA->status) && (RealHTTPHandle->GetMaximumRedirects()) )
	{
		RealHTTPHandle->DecrementMaximumRedirectsCount();
		char *host = response->GetHeaderValue("Host:",0);
		char *Location = GetPathFromLocationHeader(DATA->response,RealHTTPHandle->IsSSLNeeded(),host);
		free(host);

		if (Location)
		{
			PREQUEST RedirectedData = SendHttpRequest(HTTPHandle,NULL,"GET",Location,NULL,0,lpUsername,lpPassword,DATA->challenge);
			if (RedirectedData)
			{
				free(Location);
				DATA->request=NULL; /* We are reutilizing the same request, so avoid deleting memory twice */
				delete DATA;
				return (RedirectedData);
			}
		}
	}
	RealHTTPHandle->ResetMaximumRedirects();
	return (DATA);


}
/**************************************************************************************************/
PREQUEST HTTPAPI::SendHttpRequest(
								  HTTPHANDLE HTTPHandle,
								  HTTPCSTR VHost,
								  HTTPCSTR HTTPMethod,
								  HTTPCSTR url,
								  HTTPCSTR PostData,
								  unsigned int PostDataSize,
								  HTTPCSTR lpUsername,
								  HTTPCSTR lpPassword,
								  int AuthMethod)
{

	PHTTP_DATA request=BuildHTTPRequest(HTTPHandle,VHost,HTTPMethod,url,PostData,PostDataSize);
	if (request)
	{
		PREQUEST DATA = SendHttpRequest(HTTPHandle,request,lpUsername,lpPassword,AuthMethod);
		if (DATA)
		{
			return(DATA);
		}
		delete request;
	}
	return(NULL);

}
/*******************************************************************************************/
char* HTTPAPI::GetPathFromLocationHeader(PHTTP_DATA response, int ssl, const char* domain)
{
	char *Location = response->GetHeaderValue("Location:",0);
	if (Location)
	{
		switch (*Location)
		{
			case '/': /* This does not Follows rfc however we should accept it */
				return(Location);
			case 'h':
			case 'H':
				if (strlen(Location)>=8 )
				{
					if (Location[4+ssl]==':')
					{
						if (strncmp(Location+7+ssl,domain,strlen(domain))==0)
						{
							char *p=strchr(Location+7+ssl,'/');
							if (p)
							{
								char *q = strdup(p);
								free(Location);
								return(q);
							}
						}
					}
				}

				if (Location[4+ssl]!=':') return(NULL); /* ssl status does not match */

				break;

		default:
			free(Location);
			return(NULL);
		}
     	free(Location);
	}
	return(NULL);

}
PREQUEST	HTTPAPI::SendHttpRequest(HTTPHANDLE HTTPHandle,HTTPCSTR VHost,HTTPCSTR HTTPMethod,HTTPCSTR url,HTTPCSTR Postdata,unsigned int PostDataSize)
{
	return	SendHttpRequest(HTTPHandle,VHost,HTTPMethod,url,Postdata,PostDataSize,NULL,NULL,0);
}
/*******************************************************************************************/
PREQUEST	HTTPAPI::SendHttpRequest(HTTPHANDLE HTTPHandle,HTTPCSTR HTTPMethod,HTTPCSTR url,HTTPCSTR Postdata,unsigned int PostDataSize)
{
	return	SendHttpRequest(HTTPHandle,NULL,HTTPMethod,url,Postdata,PostDataSize,NULL,NULL,0);
}
/*******************************************************************************************/
PREQUEST	HTTPAPI::SendHttpRequest(HTTPHANDLE HTTPHandle,HTTPCSTR VHost,HTTPCSTR HTTPMethod,HTTPCSTR url) 
{
	return	SendHttpRequest(HTTPHandle,VHost,HTTPMethod,url,NULL,0,NULL,NULL,0);
}				
/*******************************************************************************************/
PREQUEST	HTTPAPI::SendHttpRequest(HTTPHANDLE HTTPHandle,HTTPCSTR HTTPMethod,HTTPCSTR url)
{
	return	SendHttpRequest(HTTPHandle,NULL,HTTPMethod,url,NULL,0,NULL,NULL,0);
}
/*******************************************************************************************/
PREQUEST	HTTPAPI::SendHttpRequest(HTTPHANDLE HTTPHandle,HTTPCSTR url)
{
	return	SendHttpRequest(HTTPHandle,NULL,"GET",url,NULL,0,NULL,NULL,0);
}
/*******************************************************************************************/
PREQUEST	HTTPAPI::SendHttpRequest(HTTPCSTR Fullurl)
{
	int SSLREQUEST = ( (Fullurl[5]=='s') || ( Fullurl[5]=='S') );
	int port;
	HTTPSTR path = NULL;
	HTTPSTR host =  (HTTPSTR)Fullurl + 7 +  SSLREQUEST;
	HTTPSTR p = strchr(host,':');
	if (!p)
	{
		if (SSLREQUEST)
		{
			port = 443;
		} else
		{
			port = 80;
		}
		HTTPSTR newpath=strchr(host,'/');
		if (newpath)
		{
			path=strdup(newpath);
			*newpath=0;
		} else
		{
			path=strdup("/");
		}
	} else
	{
		*p=0;
		p++;
		HTTPSTR newpath=strchr(p,'/');
		if (newpath)
		{
			path=strdup(newpath);
			*newpath=0;
			port = atoi(p);
		} else
		{
			port = atoi(p);
			path=strdup("/");
		}
	}

	HTTPHANDLE HTTPHandle = InitHTTPConnectionHandle(host,port,SSLREQUEST);

	if (HTTPHandle !=INVALID_HHTPHANDLE_VALUE)
	{
		PREQUEST data= SendHttpRequest(HTTPHandle,path);
		EndHTTPConnectionHandle(HTTPHandle);
		free(path);
		return(data);
	}
	return(NULL);
}




/*******************************************************************************************/
//! This function is used to disconnect a currently stablished connection.
/*!
\param HTTPHandle Handle of the remote connection.
\param what Cancel only the current request HTTP_REQUEST_CURRENT or blocks all connections against the remote HTTP host with HTTP_REQUEST_ALL.
\note This function is needed to cancel requests like example a CONNECT call sent against a remote
HTTP proxy server by SendRawHttpRequest()
*/
/*******************************************************************************************/
int HTTPAPI::CancelHttpRequest(HTTPHANDLE HTTPHandle, int what)
{
	int ret=0;
	lock.LockMutex();
	class HHANDLE * phandle = GetHHANDLE(HTTPHandle);
	class ConnectionHandling *conexion = (class ConnectionHandling *)phandle->GetConnection();
	if (conexion)
	{
		conexion->Disconnect();
		ret=1;
	}
	lock.UnLockMutex();
	return (ret);
}
/*******************************************************************************************/
int HTTPAPI::RegisterHTTPCallBack(unsigned int cbType, HTTP_IO_REQUEST_CALLBACK cb,HTTPCSTR Description)
{
	return ( HTTPCallBack.RegisterHTTPCallBack(cbType,cb,Description));
}
/*******************************************************************************************/
void HTTPAPI::BuildBasicAuthHeader(HTTPCSTR Header,HTTPCSTR lpUsername, HTTPCSTR lpPassword,HTTPSTR destination,int dstsize)
{
	char RawUserPass[750];
	char EncodedUserPass[1000];

	RawUserPass[sizeof(RawUserPass)-1]='\0';
	snprintf(RawUserPass,sizeof(RawUserPass)-1,"%s:%s",lpUsername,lpPassword);
	int ret = Base64Encode((unsigned HTTPSTR )EncodedUserPass,(unsigned HTTPSTR)RawUserPass,(int)strlen(RawUserPass));	
	EncodedUserPass[ret]='\0';
	snprintf(destination,dstsize-1,"%s: Basic %s\r\n",Header,EncodedUserPass);

}


/*******************************************************************************************/
PREQUEST HTTPAPI::SendHttpRequest(HTTPHANDLE HTTPHandle,PHTTP_DATA request)
{
	PHTTP_DATA newrequest= new httpdata (request->Header,request->HeaderSize,request->Data, request->DataSize);
	return SendHttpRequest(HTTPHandle,newrequest,NULL,NULL,NO_AUTH);
}
/*******************************************************************************************/
PREQUEST HTTPAPI::SendRawHTTPRequest(HTTPHANDLE HTTPHandle,HTTPCSTR headers, unsigned int HeaderSize, HTTPCSTR postdata, unsigned int PostDataSize)
{
	PHTTP_DATA request= new httpdata ((HTTPSTR)headers,HeaderSize,(HTTPSTR)postdata, PostDataSize);

	PHTTP_DATA		response = DispatchHTTPRequest(HTTPHandle,request);
	if (!response)
	{
		delete request;
		return(NULL);
	}
	return ( (PREQUEST) HTTPHandleTable[HTTPHandle]->ParseReturnedBuffer( request,response) );

}
/*******************************************************************************************/

void HTTPAPI::SendHTTPProxyErrorMessage( ConnectionHandling* connection,int connectionclose, int status,HTTPCSTR protocol, HTTPCSTR title, HTTPCSTR extra_header, HTTPCSTR text )
{
	char tmp[10000];

	snprintf( tmp,sizeof(tmp)-1,"<HTML>\n<HEAD><TITLE>%d %s</TITLE></HEAD>\n"
		"<BODY BGCOLOR=\"#88a3f1\" TEXT=\"#000000\" LINK=\"#2020ff\" VLINK=\"#4040cc\">\n"
		"The HTTP Proxy server found an error while parsing client request</br>\n"
		"Error Status: <H4>%d %s</H4>\n"
		"<b>Detailed information: </b>%s\n"
		"<HR>\n"
		"<ADDRESS><A HREF=\"http://www.tarasco.org/security/\">FSCAN HTTP Proxy</A></ADDRESS>\n"
		"</BODY>\n"
		"</HTML>\n",
		status, title, status, title,text);

	PHTTP_DATA request = new httpdata;
#ifdef _OPENSSL_SUPPORT_
	request->BuildHTTPProxyResponseHeader( (connection->IsSSLInitialized()!=NULL),connectionclose, status,protocol, title, extra_header, "text/html", (int)strlen(tmp), -1 );
#else
	request->BuildHTTPProxyResponseHeader( 0,connectionclose, status,protocol, title, extra_header, "text/html", (int)strlen(tmp), -1 );
#endif

	free(request->Data);
	request->Data = strdup(tmp);
	request->DataSize=(unsigned int )strlen(tmp);
	connection->SendHTTPRequest(request);
	delete request;
}

/*******************************************************************************************************/
int DispatchHTTPProxyRequestThreadFunc(void *foo)
{
	struct params *param = (struct params *)foo;
	HTTPAPI *api = (HTTPAPI*)param->classptr;
	void *ListeningConnection = param->ListeningConnectionptr;
	free(foo);
	api->DispatchHTTPProxyRequest(ListeningConnection);
	return(0);
}
/*******************************************************************************************************/
void *HTTPAPI::ListenConnection(void *foo)
{
	struct sockaddr_in sin;   
	DWORD dwThread=0;

#ifdef __WIN32__RELEASE__
#else
	pthread_t e_th;
#endif
	int id=0;
	if ((ListenSocket=socket(PF_INET,SOCK_STREAM,IPPROTO_TCP))==INVALID_SOCKET)
	{
#ifdef __WIN32__RELEASE__
		printf("HTTPPROXY::ListenConnection(): socket() error: %d\n", WSAGetLastError());
#else
		printf("HTTPPROXY::ListenConnection(): socket() error: %d\n", 0);

#endif
		return (NULL);
	}

	sin.sin_family = AF_INET;
	sin.sin_port = (u_short)htons(BindPort);
	if (*BindIpAdress)
	{
		sin.sin_addr.s_addr  = inet_addr(BindIpAdress);
	} else
	{
		sin.sin_addr.s_addr = INADDR_ANY;
	}
	if (ListenSocket==SOCKET_ERROR) return(0);
	if ( bind(ListenSocket, (struct sockaddr *) &sin, sizeof(sin)) == SOCKET_ERROR )
	{
		printf("HTTPPROXY::WaitForRequests(): bind() error\n");
		return(NULL);
	}
	if ( listen(ListenSocket, HTTP_MAX_CONNECTIONS) == SOCKET_ERROR )
	{
		printf("HTTPPROXY::WaitForRequests(): listen() error\n");
		return(NULL);
	}
#ifdef _DBG_
	printf("WaitForRequests(): Waiting for new connections\n");
#endif

	/*This is our trick to call a class function and send them params */
	do
	{
		int clientLen= sizeof(struct sockaddr_in);
		ConnectionHandling *connection = new ConnectionHandling;
		connection->Acceptdatasock( ListenSocket );
#ifdef _DBG_
		printf("[%3.3i] WaitForRequests(): New Connection accepted from %s\n",connection->id,connection->targetDNS);
#endif
		/* Waiting for incoming connections */
		struct params *param = (struct params *)malloc (sizeof(struct params));
		param->classptr = (void*)this;
		param->ListeningConnectionptr = (void*)connection;
#ifdef __WIN32__RELEASE__
		CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) DispatchHTTPProxyRequestThreadFunc, (LPVOID) param, 0, &dwThread);
#else
		void* (*foo)(void*) = (void*(*)(void*))DispatchHTTPProxyRequestThreadFunc;
		pthread_create(&e_th, NULL,foo, (void *)param);
#endif

	} while(1);
}
/****************************************************************************************/
int ListenConnectionThreadFunc(void *foo)
{
	HTTPAPI *api = (HTTPAPI*)foo;
	api->ListenConnection(NULL);
	return(0);
}
/****************************************************************************************/
//! Initializes the HTTP Proxy engine.
/*!
\return This functions returns 1 if initialization succed, 0 if there is an error loading SSL certificates 
or binding to the current address. Value 2 means that the API is already initialized.
\note The HTTP engine should be initialized before interacting with the proxy .
*/
/*******************************************************************************/

int	HTTPAPI::InitHTTPProxy(HTTPCSTR hostname, unsigned short port)
{
	if (BindPort)
	{
		return(0);
	} else
	{
		BindPort=port;
		strncpy(BindIpAdress,hostname,sizeof(BindIpAdress)-1);
		BindIpAdress[sizeof(BindIpAdress)-1]='\0';
		ForceDefaultHTTPPorts = 1;
		AnonymousProxy        = 1;
		AsyncHTTPRequest      = 0;
		DisableBrowserCache   = 1;
		ConnectMethodAllowed  = 1;
		UseOriginalUserAgent  = 0;
#ifdef _OPENSSL_SUPPORT_
		InitProxyCTX();
#endif
		ProxyEngine.InitThread((void*)ListenConnectionThreadFunc,(void*)this);
		/* This is disabled by default in our Proxy server */
		this->SetHTTPConfig(GLOBAL_HTTP_CONFIG,OPT_HTTP_HANDLE_COOKIES,0);
		this->SetHTTPConfig(GLOBAL_HTTP_CONFIG,OPT_HTTP_AUTOREDIRECT,0);
		return(1);
	}
}
/*******************************************************************************/
int	HTTPAPI::InitHTTPProxy(HTTPCSTR hostname, HTTPCSTR port)
{
	return  ( InitHTTPProxy(hostname,atoi(port)) );
}
/*******************************************************************************/
int	HTTPAPI::StopHTTPProxy(void)
{
	if (!BindPort)
	{
		return(0);
	}
	BindPort = 0;
	memset( BindIpAdress,0,sizeof(BindIpAdress));
	ProxyEngine.EndThread();
	return  (1);

}

/*******************************************************************************/
//! This is the main HTTP Proxy function that reads for user requests and translates them to requests sent against remote HTTP hosts.
/*!
\param ListeningConnection pointer to an ConnectionHandling struct created when accepted an incoming connection.
\note This function is exported by the HTTPAPI class and must only be called Threaded from ListenConnection();
*/
/*******************************************************************************/
int HTTPAPI::DispatchHTTPProxyRequest(void *ListeningConnection)
{

	ConnectionHandling *ClientConnection = (ConnectionHandling *)ListeningConnection;
	PHTTP_DATA			ProxyRequest  = NULL;
	HTTPHANDLE			HTTPHandle = INVALID_HHTPHANDLE_VALUE;
	unsigned int		connect = 0;
	unsigned long		ret = 0;
	int					ConnectionClose = 0;

#ifdef _OPENSSL_SUPPORT_
	ClientConnection->SetBioErr(bio_err);
#endif
	/* Read an HTTP request from the connected client */
	while ( (!ConnectionClose) && (ProxyRequest=ClientConnection->ReadHTTPProxyRequestData()) )
	{
		if (connect)
		{   /* We are intercepting an HTTPS request. Just replay the request with some minor modifications */
			if (DisableBrowserCache)
			{   /* We can also force HTTP Requests to avoid cache */
				ProxyRequest->RemoveHeader("If-Modified-Since: ");
			}
			if (!UseOriginalUserAgent)
			{   /* We like FhScan, so we will use our User-Agent unless UseOriginalUserAgent is set to true */
				char tmp[256];
				ProxyRequest->RemoveHeader("User-Agent");
				snprintf(tmp,sizeof(tmp)-1,"User-Agent: %s\r\n",FHScanUserAgent);
				ProxyRequest->AddHeader(tmp);
			}
			/* Call the registered plugins */
			ret = HTTPCallBack.DoCallBack( CBTYPE_PROXY_REQUEST ,HTTPHandle,ProxyRequest,NULL );
			if (ret==CBRET_STATUS_CANCEL_REQUEST)
			{
				SendHTTPProxyErrorMessage( ClientConnection,0, 403,"HTTP/1.1", "Blocked Request", (HTTPSTR) 0, "The HTTP request was blocked before sending." );
			} else 
			{   /* Deliver the HTTP request to the HTTP server */
				PREQUEST data = SendRawHTTPRequest(HTTPHandle,ProxyRequest->Header,ProxyRequest->HeaderSize,ProxyRequest->Data,ProxyRequest->DataSize);

				ret = HTTPCallBack.DoCallBack( CBTYPE_PROXY_RESPONSE ,HTTPHandle,ProxyRequest,data->response);

				if (!AsyncHTTPRequest)
				{   /* Perform minor modifications to the HTTP Response and send it again to the client */
					if (ret==CBRET_STATUS_CANCEL_REQUEST)
					{
						SendHTTPProxyErrorMessage( ClientConnection,0, 403,"HTTP/1.1", "Blocked Request", (HTTPSTR) 0, "The HTTP request was blocked after sending." );
					} else
					{   /* Force The Client to Keep the connection open */

						data->response->RemoveHeader("Connection:");
						data->response->AddHeader("Connection: Keep-Alive");

						HTTPSTR header = data->response->GetHeaderValue("Content-Length:",0);
						if (header)
						{
							unsigned int DataSize = atoi(header);
							if (DataSize != data->response->DataSize)
							{
								/* Fix the Content-Length  header because the server returned bad or Incomplete Response */
								data->response->RemoveHeader("Content-Length:");
								char tmp[256];
								sprintf(tmp,"Content-Length: %i",data->response->DataSize);
								data->response->AddHeader(tmp);
								/* This error happens only if:
								- wrong/malformed HTTP response.
								- The connection have been closed.
								- The HTTP response data was parsed incorrectly.
								To prevent cascade errors, we should close the connection and force the client to reconnect*/
								ProxyRequest->RemoveHeader("Connection: ");
								data->response->AddHeader("Connection: close");
								ConnectionClose = 1;
							}
							free(header);
						} else
						{   /* Add a missing Content-Length header */
							char tmp[256];
							sprintf(tmp,"Content-Length: %i",data->response->DataSize);
							data->response->AddHeader(tmp);
						}									
						ClientConnection->SendHTTPRequest(data->response);				
					}
				}
			}
		} else
		{   /* There is still no SSL Tunnel Stablished so,  parse the request as a normal HTTP request */
			HTTPSTR line=NULL;
			char method[10000];
			char host[10000];
			char path[10000];
			int  port;
			char protocol[10000]="HTTP/1.1";		

			/* Get information from the incoming HTTP Request */
			line=ProxyRequest->GetHeaderValueByID(0);
			if (line)
			{
				if (strncmp(line,"CONNECT ",8)==0)
				{   /*TODO: Check for overflow*/
					if (sscanf( line, "%[^ ] %[^ ] %[^ ]", method, host, protocol )!=3)
					{
						ret = BAD_REQUEST_CANT_PARSE_REQUEST;
					} else
					{
						HTTPSTR lpPort=strchr(host,':');
						if (lpPort)
						{
							*lpPort=0;
							port=atoi(lpPort+1);
						} else
						{
							port=443;
						}
						ret = REQUEST_ACCEPTED;
					}
				} else
				{
					ret = ParseRequest(line, method,  host, path, &port);
				}
			} else
			{
				ret = BAD_REQUEST_CANT_PARSE_REQUEST;
			}

			/* Initialize the Handle to NULL */
			HTTPHandle = INVALID_HHTPHANDLE_VALUE;

			switch (ret)
			{
			case REQUEST_ACCEPTED:	
				/* Initialize the HTTPHandle against the remote host */
				connect = (strcmp(method,"CONNECT")==0);
				if ( ForceDefaultHTTPPorts)
				{
					if ( (port ==  80 ) || (port == 443)  )
					{					
						HTTPHandle = InitHTTPConnectionHandle(host,port, connect);
					} else
					{
						SendHTTPProxyErrorMessage( ClientConnection,0, 403,protocol, "Blocked Request", (HTTPSTR) 0, "The remote HTTP port is not allowed." );
					}
				} else
				{
					HTTPHandle = InitHTTPConnectionHandle(host,port, connect);
				}

				if (HTTPHandle == INVALID_HHTPHANDLE_VALUE)
				{
					memset(method,0,sizeof(method));
					snprintf(method,sizeof(method)-1," Unable to resolve the requested host <b>\"%s\"</b>.</br></br>"
						"This can be caused by a DNS timeout while resolving the remote address or just because the address you typed is wrong.</br>"
						"Click <a href=\"%s%s%s\">here</a> to try again",host,connect ? "https://" : "http://",host,path);
					if (connect) SendHTTPProxyErrorMessage( ClientConnection,1, 503,protocol, "Service Unavailable", (HTTPSTR) 0,method);
					else SendHTTPProxyErrorMessage( ClientConnection,0, 503,protocol, "Service Unavailable", (HTTPSTR) 0,method);
					connect=0;
				} else
				{
					SetHTTPConfig(HTTPHandle,OPT_HTTP_PROTOCOL,line+strlen(line)-1);
					if (AsyncHTTPRequest) 
					{
						GetHHANDLE(HTTPHandle)->SetClientConnection(ClientConnection);
					}

					if (strcmp(method,"CONNECT")==0) 
					{  /*Initialize the SSL Tunnel and replay the client with a "Connection stablished 200 OK" message*/
#ifdef _OPENSSL_SUPPORT_
						PHTTP_DATA  HTTPTunnel= new httpdata;
						HTTPTunnel->BuildHTTPProxyResponseHeader(ClientConnection->IsSSLInitialized()!=NULL,0,200,protocol,"Connection established","Proxy-connection: Keep-alive",NULL,-1,-1);
						ClientConnection->SendHTTPRequest( HTTPTunnel);
						delete HTTPTunnel;

						ClientConnection->SetCTX(ctx);
#else
						SendHTTPProxyErrorMessage( ClientConnection,0, 403,protocol, "Blocked Request", (HTTPSTR) 0, "SSL NOT SUPPORTED." );
#endif
					} else 
					{   /* Parse the HTTP request Headers before sending */
						unsigned long ret;

						ret = HTTPCallBack.DoCallBack( CBTYPE_PROXY_REQUEST ,HTTPHandle,ProxyRequest,NULL );

						if (ret & CBRET_STATUS_CANCEL_REQUEST)
						{
							SendHTTPProxyErrorMessage( ClientConnection,0, 403,protocol, "Blocked Request", (HTTPSTR) 0, "The requested resource was blocked by a registered module." );
						} else
						{
							char 		 *ClientRequest=NULL;
							unsigned int  ClientRequestLength  = 0;
							unsigned int HeaderLength ;
							char 		 tmp[256];
							unsigned int id =1;
							char 		 *p;
							HTTPSTR vhost = ProxyRequest->GetHeaderValue("Host:",0);


							if  (UseOriginalUserAgent) 
							{   /* By default SendHttpRequest() will append their own user-Agent header, so we must override it */
								SetHTTPConfig(HTTPHandle,OPT_HTTP_USERAGENT,NULL);
							}

							do	
							{  /*Append Browser HTTP Headers to our custom request and ignore some of them*/
								p=ProxyRequest->GetHeaderValueByID(id++);
								if (p)
								{
									if (!SkipHeader(p)) 
									{ 
										HeaderLength  = (unsigned int)strlen(p);
										if (!ClientRequestLength  )
										{
											ClientRequest = (HTTPSTR)malloc(HeaderLength  +2 +1);
										} else
										{
											ClientRequest = (HTTPSTR)realloc(ClientRequest,ClientRequestLength  + HeaderLength  +2 +1);
										}
										memcpy(ClientRequest+ClientRequestLength ,p,HeaderLength );
										ClientRequestLength  += HeaderLength ;
										memcpy(ClientRequest+ClientRequestLength ,"\r\n",2);
										ClientRequestLength  += 2;
									} else 
									{
										if ( (UseOriginalUserAgent) && (strncmp(p,"User-Agent:",11)==0))
										{
											SetHTTPConfig(HTTPHandle,OPT_HTTP_USERAGENT,p+12);
										} 
									}
									free(p);
								} 
							} while (p);


							if (!AnonymousProxy)
							{   /* Append Remote user identification Header */
								sprintf(tmp,"X-Forwarded-For: %s\r\n",ClientConnection->GettargetDNS());
								HeaderLength  = (unsigned int)strlen(tmp);
								if (!ClientRequest) 
								{
									ClientRequest = (HTTPSTR)malloc(HeaderLength  + 1);
								} else 
								{
									ClientRequest = (HTTPSTR)realloc(ClientRequest,ClientRequestLength  + HeaderLength +1);
								}
								memcpy(ClientRequest+ClientRequestLength ,tmp,HeaderLength );
								ClientRequestLength +=HeaderLength ;
							}

							if (ClientRequest)
							{
								ClientRequest[ClientRequestLength ]=0;
								SetHTTPConfig(HTTPHandle,OPT_HTTP_HEADER,ClientRequest);
								free(ClientRequest);
							}
							/* Send request to the remote HTTP Server */
							PREQUEST data = SendHttpRequest(HTTPHandle,vhost ? vhost : host,method,path,ProxyRequest->Data,ProxyRequest->DataSize,NULL,NULL,NO_AUTH);

							/* Clean the HTTPHandle options */
							SetHTTPConfig(HTTPHandle,OPT_HTTP_HEADER,NULL);
							if (vhost) free(vhost);

							if   (data) 
							{
								/* Parse returned HTTP response and add some extra headers to avoid client disconnection*/
								if ( (data->response) && (data->response->HeaderSize!=0) )
								{
#ifdef _DBG_
									printf("[%3.3i] HandleRequest() - Leida respuesta del servidor Web. Headers Len: %i bytes - Datos: %i bytes\n",ClientConnection->id,data->response->HeaderSize,data->response->DataSize);
									printf("Headers: !%s!\n",data->response->Header);
									if (data->response->DataSize) printf("!%s!\n",data->response->Data);
#endif
									data->response->RemoveHeader("Connection:");
									data->response->AddHeader("Proxy-Connection: keep-alive");
									data->response->RemoveHeader("Content-Length:");
									sprintf(path,"Content-Length: %i",data->response->DataSize);
									data->response->AddHeader(path);

									ret = HTTPCallBack.DoCallBack( CBTYPE_PROXY_RESPONSE ,HTTPHandle,ProxyRequest,data->response );

									if (!AsyncHTTPRequest)
									{
										if (ret & CBRET_STATUS_CANCEL_REQUEST)
										{
											SendHTTPProxyErrorMessage( ClientConnection,0, 403,protocol, "Blocked Request", (HTTPSTR) 0, "The requested resource was blocked by a register module." );
										} else
										{
											/* Finally, deliver HTTP response */
											ClientConnection->SendHTTPRequest( data->response);
										}
									}
								} else
								{
									SendHTTPProxyErrorMessage( ClientConnection,0, 503,protocol, "Service Unavailable", (HTTPSTR) 0, "The remote host returned no data." );
									//TODO: close the connection table (not sure if needed :? )

								}
								//FreeRequest(data);
								delete data;

							} else
							{
								SendHTTPProxyErrorMessage( ClientConnection,0, 504,protocol, "Gateway Timeout", (HTTPSTR) 0, "Unable to reach the remote HTTP Server." );
								//TODO: close the connection table (not sure if needed :? )
							}
						}
						EndHTTPConnectionHandle(HTTPHandle);
						HTTPHandle = INVALID_HHTPHANDLE_VALUE;

					} /*End of HTTP request (Method != "CONNECT") */
				}
				break;
				/* deal with some errors */
			case BAD_REQUEST_NO_REQUEST_FOUND:
				SendHTTPProxyErrorMessage( ClientConnection,1, 400, protocol,"Bad Request", (HTTPSTR) 0, "No request found." );
				ConnectionClose = 1;
				break;
			case BAD_REQUEST_CANT_PARSE_REQUEST:
				SendHTTPProxyErrorMessage(ClientConnection,1,  400, protocol,"Bad Request", (HTTPSTR) 0, "Can't parse request." );
				ConnectionClose = 1;
				break;
			case BAD_REQUEST_NULL_URL:
				SendHTTPProxyErrorMessage( ClientConnection,1, 400,protocol, "Bad Request", (HTTPSTR) 0, "Null URL." );
				ConnectionClose = 1;
				break;
			default:
				SendHTTPProxyErrorMessage( ClientConnection,1, 400, protocol,"Bad Request", (HTTPSTR) 0, "OMG! Unknown error =)" );
				ConnectionClose = 1;
				break;
			}			
			if (line) free(line);							
		}
		delete ProxyRequest;
		ProxyRequest=NULL;
	}	
	EndHTTPConnectionHandle(HTTPHandle);
	if (ProxyRequest)
	{
		delete ProxyRequest; 
		ProxyRequest = NULL;
	}
#ifdef _DBG_
	printf("[%3.3i] HandleRequest(): DESCONEXION del Cliente...\n",ClientConnection->id);
#endif
	delete ClientConnection;

	//TODO:
	/*
	- Liberar ClientConnection
	- liberar la informacion SSL de ClientConnection.
	*/
	return(1);
}
/****************************************************************************************/
int HTTPAPI::ParseRequest(HTTPSTR line, HTTPSTR method,  HTTPSTR host, HTTPSTR path, int *port)
{
	int iport;
	char protocol[10000];
	char url[10000];


	if ( (!line) || (*line=='\0')) return (BAD_REQUEST_NO_REQUEST_FOUND);

	if ( sscanf( line, "%[^ ] %[^ ] %[^ ]", method, url, protocol ) != 3 ) return(BAD_REQUEST_CANT_PARSE_REQUEST);

	if ( url == (HTTPSTR) 0 ) return(BAD_REQUEST_NULL_URL);

	if ( strnicmp( url, "http://", 7 ) == 0 )
	{
		strncpy( url, "http", 4 );	/* make sure it's lower case */
		if ( sscanf( url, "http://%[^:/]:%d%s", host, &iport, path ) == 3 )
			*port = (unsigned short) iport;
		else if ( sscanf( url, "http://%[^/]%s", host, path ) == 2 )
			*port = 80;
		else if ( sscanf( url, "http://%[^:/]:%d", host, &iport ) == 2 )
		{
			*port = (unsigned short) iport;
			strcpy(path,"/");
		}
		else if ( sscanf( url, "http://%[^/]", host ) == 1 )
		{
			*port = 80;
			strcpy(path,"/");
		} else
		{
			return (BAD_REQUEST_CANT_PARSE_REQUEST);
		}
	} else
	{
		if ( (ConnectMethodAllowed) && (strcmp(method,"CONNECT")==0) )
		{
			HTTPSTR p=strchr(url,':');
			if (p)
			{
				*port = iport=atoi(p+1);
				*p=0;
				strcpy(host,url);
			}
			else
			{
				return (BAD_REQUEST_CANT_PARSE_REQUEST);
			}
		} else
			return (BAD_REQUEST_CANT_PARSE_REQUEST);	//send_error( sock, 400, "Bad Request", (HTTPSTR) 0, "Unknown URL type." );
	}
	return (REQUEST_ACCEPTED);
}

/*******************************************************************************/
int HTTPAPI::SkipHeader(HTTPSTR header)
{
	struct skipheader
	{
		const char name[20];
		int len;
	} IgnoreHeaders[]= {
		{ "Accept-Encoding:", 16},
		{ "Age:",              4},
		{ "Cache-Control:",   14},
		{ "Connection:",      11},
		{ "Content-Length:",  15},
		{ "Host:",             5},
		{ "Keep-Alive:",      11},
		{ "Last-Modified:",   14},
		{ "Proxy-Connection:",17},
		{ "User-Agent:"      ,11}
	};
	int ret;
	unsigned int i=0;
	do {
		ret=strnicmp(header,IgnoreHeaders[i].name,IgnoreHeaders[i].len);
		if (ret==0) return(1);
		i++;
	} while ((i<sizeof(IgnoreHeaders)/sizeof(struct skipheader)) && (ret>0));

	if ( (DisableBrowserCache)  && (  (strnicmp(header,"If-Modified-Since: ",19)==0) || ( strnicmp(header,"If-None-Match: ",15)==0)))
	{
		return(1);
	}

	return(0);


}
/*******************************************************************************/
//! This function Allows users to change some HTTP Proxy configuration.
/*!
\param opt this value indicates the kind of data that is going to be modified. Valid options are:\n
- OPT_HTTPPROXY_ALLOWCONNECT (Enables Support of tunneling requests with the HTTP Connect method. Default allowed)
- OPT_HTTPPROXY_ANONYMOUSPROXY (Handle if the proxy is going to add extra headers like X-Forwarded-For. Default Anonymous )
- OPT_HTTPPROXY_ASYNCREQUEST (Does not wait for the full request to be readead and the readead data is sent as received to the client. Default disabled)
- OPT_HTTPPROXY_DISABLECACHE (Includes some extra headers to avoid browser cache, so the resource is retrieved again. Default enabled (cache is disabled)
- OPT_HTTPPROXY_ORIGINALUSERAGENT (Force the HTTP proxy to use the original user agent or use instead a custom fhscan user-agent. Default disabled (custom user-agent sent) )
- OPT_HTTPPROXY_FORCE_DEFAULT_HTTP_PORTS (Allows connecting to non default HTTP ports. Default is enabled. Only port 80 and 443 allowed)
\param parameter pointer to the Current value. These values can be "1" to enable the feature or "0" to disable it.
\return This function doest not return information, as its not needed.
\note if parameter is NULL, the operation is ignored.
/*******************************************************************************/
void HTTPAPI::SetHTTPProxyConfig(int opt,HTTPSTR parameter)
{
	if (!parameter) return;
	switch (opt)
	{
	case OPT_HTTPPROXY_ALLOWCONNECT:
		ConnectMethodAllowed = atoi(parameter);
		break;
	case OPT_HTTPPROXY_ANONYMOUSPROXY:
		AnonymousProxy = atoi(parameter);
		break;
	case OPT_HTTPPROXY_ASYNCREQUEST:
		AsyncHTTPRequest = atoi(parameter);
		break;
	case OPT_HTTPPROXY_DISABLECACHE:
		DisableBrowserCache = atoi(parameter);
		break;
	case OPT_HTTPPROXY_ORIGINALUSERAGENT:
		UseOriginalUserAgent = atoi(parameter);
		break;	
	case OPT_HTTPPROXY_FORCE_DEFAULT_HTTP_PORTS:
		ForceDefaultHTTPPorts = atoi(parameter);
		break;
	}
	return;
}

/*******************************************************************************/
/*******************************************************************************/
void HTTPAPI::SetHTTPProxyConfig(int opt,int parameter)
{
	switch (opt)
	{
	case OPT_HTTPPROXY_ALLOWCONNECT:
		ConnectMethodAllowed = parameter;
		break;
	case OPT_HTTPPROXY_ANONYMOUSPROXY:
		AnonymousProxy = parameter;
		break;
	case OPT_HTTPPROXY_ASYNCREQUEST:
		AsyncHTTPRequest = parameter;
		break;
	case OPT_HTTPPROXY_DISABLECACHE:
		DisableBrowserCache = parameter;
		break;
	case OPT_HTTPPROXY_ORIGINALUSERAGENT:
		UseOriginalUserAgent = parameter;
		break;	
	case OPT_HTTPPROXY_FORCE_DEFAULT_HTTP_PORTS:
		ForceDefaultHTTPPorts = parameter;
		break;
	}
	return;
}
/*******************************************************************************************/
char *HTTPAPI::GetPathFromURL(const char *url)
{ /* Its assumed that *url == '/' */

	char *FullURL=strdup(url);
	char *p=FullURL;
	char *end = NULL;

	while (*p)
	{
		switch (*p)
		{
		case '/':
			end=p;
			break;
		case '?':
		case '&':
		case ';':
			/* We can be sure that there is not parameters at the url so its recommended to check
              if they exist and avoid them to give us wrong data */
			if (end)
			{
				end[1] =0;
			}
			return(FullURL);
			break;
		}
		p++;
	}
	if (end)
	{
		end[1] =0;
	}
	return(FullURL);
}
/*******************************************************************************************/
char *HTTPAPI::BuildCookiesFromStoredData( const char *TargetDNS, const char *path,int secure)
{
	return (COOKIE ->ReturnCookieHeaderFor(TargetDNS,path,secure));

}
/*******************************************************************************************/
void HTTPAPI::ExtractCookiesFromResponseData(PHTTP_DATA response, const char *lpPath, const char *TargetDNS)
{
	if (response)
	{
		int n =0;
		char *lpCookie = NULL;
		do
		{
			if (lpCookie) free(lpCookie);
			lpCookie = response->GetHeaderValue("Set-Cookie:",n);
			if (lpCookie)
			{
				printf("Extraido: %s\n",lpCookie);
				COOKIE->ParseCookieData(lpCookie,lpPath,TargetDNS);
				n++;
			} 
		} while (lpCookie);
	}
}
/*******************************************************************************************/

