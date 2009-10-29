/** \file HTTP.h
 * Fast HTTP Auth Scanner - HTTP Engine v1.2.
 * This include file contains all needed information to manage the HTTP interface from the user side.
 * \author Andres Tarasco Acuna - http://www.tarasco.org
 */
#ifndef __HTTPAPI__
#define __HTTPAPI__

#include "HTTPHANDLE.h"

#include "Threading.h"
#include "CallBacks.h"


#define OPT_HTTP_PROXY_HOST		0x00
#define OPT_HTTP_PROXY_PORT		0x01
#define OPT_HTTP_PROXY_USER		0x02
#define OPT_HTTP_PROXY_PASS		0x04
#define OPT_HTTP_HEADER			0x08
#define OPT_HTTP_COOKIE			0x10
#define OPT_HTTP_USERAGENT		0x20
#define OPT_HTTP_PROTOCOL		0x40
#define OPT_HTTP_MAXSPEED_DOWNLOAD	0x80
#define OPT_HTTP_HOST           0x100
#define OPT_HTTP_PORT			0x200
#define OPT_HTTP_PROXY_ASYNC    0x400
#define OPT_HTTP_SSL_SUPPORTED  0x800
#define OPT_HTTP_SSL_CONNECTION 0x1000
#define OPT_HTTP_MAX_DOWNLOAD_SIZE 0x2000 //1Mb
#define OPT_HTTP_HANDLE_COOKIES 0x4000
#define OPT_HTTP_AUTOREDIRECT   0x8000

/******************************************************************************/
/* CancelHttpRequest() options */
#define HTTP_REQUEST_CURRENT 1
#define HTTP_REQUEST_ALL	 0

/******************************************************************************/


#define		INVALID_HHTPHANDLE_VALUE ((HTTPHANDLE)-1)
#define		GLOBAL_HTTP_CONFIG		 ((HTTPHANDLE)-2)

#define MAXIMUM_OPENED_HANDLES					4096
#define TARGET_FREE   							0
#define MAX_OPEN_CONNECTIONS					1024 //Our Connection table is able to handle 512 concurrent connections
#define MAX_HEADER_SIZE     	8192
#define MAX_POST_LENGTH 		8192
#define MAX_DOWNLOAD_SIZE 		MAX_POST_LENGTH*20


#define MAX_CONNECTIONS					20
#define HTTP_READ_TIMEOUT				10
#define REQUEST_ACCEPTED				0
#define BAD_REQUEST_NO_REQUEST_FOUND 	1
#define BAD_REQUEST_CANT_PARSE_REQUEST 	2
#define BAD_REQUEST_NULL_URL			3

#define OPT_HTTPPROXY_ALLOWCONNECT   0x00
#define OPT_HTTPPROXY_ANONYMOUSPROXY 0x01
#define OPT_HTTPPROXY_ASYNCREQUEST   0x02
#define OPT_HTTPPROXY_DISABLECACHE   0x03
#define OPT_HTTPPROXY_ORIGINALUSERAGENT 0x04
#define OPT_HTTPPROXY_FORCE_DEFAULT_HTTP_PORTS 0x05


#ifndef __AFXISAPI_H_ // these symbols may come from WININET.H
//! OK to continue with request
#define HTTP_STATUS_CONTINUE            100
//! server has switched protocols in upgrade header
#define HTTP_STATUS_SWITCH_PROTOCOLS    101
//! request completed
#define HTTP_STATUS_OK                  200
//! object created, reason = new URI
#define HTTP_STATUS_CREATED             201
//! async completion (TBS)
#define HTTP_STATUS_ACCEPTED            202
//! partial completion
#define HTTP_STATUS_PARTIAL             203
//! no info to return
#define HTTP_STATUS_NO_CONTENT          204
//! request completed, but clear form
#define HTTP_STATUS_RESET_CONTENT       205
//! partial GET furfilled
#define HTTP_STATUS_PARTIAL_CONTENT     206
//! server couldn't decide what to return
#define HTTP_STATUS_AMBIGUOUS           300
//! object permanently moved
#define HTTP_STATUS_MOVED               301
//! object temporarily moved
#define HTTP_STATUS_REDIRECT            302
//! redirection w/ new access method
#define HTTP_STATUS_REDIRECT_METHOD     303
//! if-modified-since was not modified
#define HTTP_STATUS_NOT_MODIFIED        304
//! redirection to proxy, location header specifies proxy to use
#define HTTP_STATUS_USE_PROXY           305
//! HTTP/1.1: keep same verb
#define HTTP_STATUS_REDIRECT_KEEP_VERB  307
//! invalid syntax
#define HTTP_STATUS_BAD_REQUEST         400
//! access denied
#define HTTP_STATUS_DENIED              401
//! payment required
#define HTTP_STATUS_PAYMENT_REQ         402
//! request forbidden
#define HTTP_STATUS_FORBIDDEN           403
//! object not found
#define HTTP_STATUS_NOT_FOUND           404
//! method is not allowed
#define HTTP_STATUS_BAD_METHOD          405
//! no response acceptable to client found
#define HTTP_STATUS_NONE_ACCEPTABLE     406
//! proxy authentication required
#define HTTP_STATUS_PROXY_AUTH_REQ      407
//! server timed out waiting for request
#define HTTP_STATUS_REQUEST_TIMEOUT     408
//! user should resubmit with more info
#define HTTP_STATUS_CONFLICT            409
//! the resource is no longer available
#define HTTP_STATUS_GONE                410
//! the server refused to accept request w/o a length
#define HTTP_STATUS_LENGTH_REQUIRED     411
//! precondition given in request failed
#define HTTP_STATUS_PRECOND_FAILED      412
//! request entity was too large
#define HTTP_STATUS_REQUEST_TOO_LARGE   413
//! request URI too long
#define HTTP_STATUS_URI_TOO_LONG        414
//! unsupported media type
#define HTTP_STATUS_UNSUPPORTED_MEDIA   415
//! internal server error
#define HTTP_STATUS_SERVER_ERROR        500
//! required not supported
#define HTTP_STATUS_NOT_SUPPORTED       501
//! error response received from gateway
#define HTTP_STATUS_BAD_GATEWAY         502
//! temporarily overloaded
#define HTTP_STATUS_SERVICE_UNAVAIL     503
//! timed out waiting for gateway
#define HTTP_STATUS_GATEWAY_TIMEOUT     504
//! HTTP version not supported
#define HTTP_STATUS_VERSION_NOT_SUP     505
#endif


/*!\STRUCT PREQUEST
  \brief This struct handles information related to and http response and includes information about client request, server response, url, server version .returned by an HTTP Server
*/
typedef struct  prequest {
	HTTPCHAR hostname[256];
   /*!< hostname of the server. This is related to the vhost parameter. If no vhost is specified, hostname contains the ip address. */
   int ip;
   /*!< remote HTTP ip address. */
   int port;
   /*!< remote HTTP port. This value is obtained from the InitHTTPConnectionHandle() */
   int NeedSSL;
   /*!< Boolean value. If this parameter is 1 then the connection is handled by openssl otherwise is just a tcp connection */
   HTTPSTR url;
   /*!< path to the file or directory requested */
   HTTPSTR Parameters;
   /*!< Request Parameters */
   PHTTP_DATA request;
   /*!< Information related to the HTTP Request. This struct contains both client headers and postdata */
   PHTTP_DATA response;
   /*!< Information related to the HTTP response. This struct contains both server headers and data */
   HTTPSTR server;
   /*!< pointer to a string that contains the server banner from the remote http server */
   HTTPCHAR Method[20];
   /*!< HTTP Verb used */
   unsigned int 	 status;
   /*!< status code returned by the HTTP server. Example: "200", for an STATUS OK response. */
   unsigned int challenge; 
   /*!< This value is not Zero if the remote host require authentication by returning the http header "WWW-Authenticate:" Possible values are: NO_AUTH,  BASIC_AUTH , DIGEST_AUTH, NTLM_AUTH, UNKNOWN_AUTH.*/
   HTTPSTR ContentType;
   /*!< Response Content-Type */
public:
	prequest();
	~prequest();
	int IsValidHTTPResponse(void) { return ((response) && (response->Header) && (response->HeaderSize) && (status>100) && (status<520) ) ; }
	int HasResponseHeader(void) { return ( (response) && (response->HeaderSize) && (response->Header) ); }
	int HasResponseData(void) {   return ( (response) && (response->DataSize) && (response->Data)   ); }
} REQUEST, *PREQUEST;


/***********************************************************************************************/
class HTTPAPI 
{
	
	class HHANDLE GlobalHTTPCoreApiOptions;/*Global HANDLE Configuration Options */	
	class HHANDLE *HTTPHandleTable[MAXIMUM_OPENED_HANDLES];/*HTTP Handle table*/	
	class ConnectionHandling *Connection_Table[MAX_OPEN_CONNECTIONS];/*Connection table (conexiones concurrentes) */	
	class HTTPCALLBACK HTTPCallBack;	/* CallBacks */
	class Threading HandleLock;			/* internal Threads */
	class Threading lock;				/* internal Threads */
	class Threading ProxyEngine;		/* internal Threads */
	#ifdef __WIN32__RELEASE__
	WSADATA ws;							/* Win32 Sockets */
	#endif

	HTTPCHAR BindIpAdress[256];			/* Proxy Bind Address */
	unsigned short BindPort;			/* Proxy Bind Port */
	SOCKET ListenSocket;				/* Listen proxy socket */
	int ForceDefaultHTTPPorts;			/* Proxy configuration option */
	int AnonymousProxy;					/* Proxy configuration option */
	int AsyncHTTPRequest;				/* Proxy configuration option */
	int DisableBrowserCache ;			/* Proxy configuration option */
	int ConnectMethodAllowed;			/* Proxy configuration option */
	int UseOriginalUserAgent;			/* Proxy configuration option */
	#ifdef _OPENSSL_SUPPORT_
	void *ctx;							/* Proxy SSL vars */
	void *bio_err;						/* Proxy SSL vars */
	int InitProxyCTX(void);
	#endif
	char	*FHScanUserAgent;			/* HTTPAPI User Agent */
	class CookieStatus *COOKIE;         /* Automatic Cookie Handling struct */
	
	friend int ThreadFunc(void *foo);
	friend int ListenConnectionThreadFunc(void *foo);
	friend int DispatchHTTPProxyRequestThreadFunc(void *foo);
	
	class HHANDLE *GetHHANDLE(HTTPHANDLE HTTPHandle);	
	void CleanConnectionTable(LPVOID *unused);
	class ConnectionHandling *GetSocketConnection(class HHANDLE *HTTPHandle, PHTTP_DATA request, unsigned long *id);
	int GetFirstIdleConnectionAgainstTarget(class HHANDLE *HTTPHandle);
	int GetFirstUnUsedConnection() ;
	void BuildBasicAuthHeader(HTTPCSTR Header,HTTPCSTR lpUsername, HTTPCSTR lpPassword,HTTPSTR destination,int dstsize);
	PHTTP_DATA DispatchHTTPRequest(HTTPHANDLE HTTPHandle,PHTTP_DATA request);

	//PROXYHTTP
	int ParseRequest(HTTPSTR line, HTTPSTR method,  HTTPSTR host, HTTPSTR path, int *port);
	int SkipHeader(HTTPSTR header);
	void *ListenConnection(void *foo);
	int DispatchHTTPProxyRequest(void *ListeningConnection);
	void SendHTTPProxyErrorMessage( ConnectionHandling* connection,int connectionclose, int status,HTTPCSTR protocol, HTTPCSTR title, HTTPCSTR extra_header, HTTPCSTR text );
	void ExtractCookiesFromResponseData( PHTTP_DATA response, const char *path, const char *TargetDNS);
	char *BuildCookiesFromStoredData( const char *TargetDNS, const char *path, int secure);
	char *GetPathFromURL(const char *url);
	char* GetPathFromLocationHeader(PHTTP_DATA response, int ssl, const char* domain);

	
public:
	HTTPAPI();
	HTTPAPI(HTTPSTR lpProxyHost, HTTPSTR lpProxy, HTTPSTR pUserName , HTTPSTR lpPassword );
	~HTTPAPI();

	HTTPHANDLE	InitHTTPConnectionHandle(HTTPSTR lpHostName,int port ,int ssl);
	HTTPHANDLE	InitHTTPConnectionHandle(HTTPSTR lpHostName,int port) {  return InitHTTPConnectionHandle(lpHostName,port,0); }
	int			SetHTTPConfig(HTTPHANDLE HTTPHandle,int opt, HTTPCSTR parameter);
	int 		SetHTTPConfig(HTTPHANDLE HTTPHandle,int opt, int parameter);
	HTTPSTR		GetHTTPConfig(HTTPHANDLE HTTPHandle,int opt);
	int			EndHTTPConnectionHandle(HTTPHANDLE);

	PHTTP_DATA  BuildHTTPRequest(HTTPHANDLE HTTPHandle,HTTPCSTR VHost,HTTPCSTR HTTPMethod,HTTPCSTR url,HTTPCSTR Postdata,unsigned int PostDataSize);
	PREQUEST	SendRawHTTPRequest(HTTPHANDLE HTTPHandle,HTTPCSTR headers, unsigned int HeaderSize, HTTPCSTR postdata, unsigned int PostDataSize);
	PREQUEST	SendHttpRequest(HTTPHANDLE HTTPHandle,PHTTP_DATA request);
	
	PREQUEST	SendHttpRequest(HTTPHANDLE HTTPHandle,HTTPCSTR VHost,HTTPCSTR HTTPMethod,HTTPCSTR url,HTTPCSTR Postdata,unsigned int PostDataSize,HTTPCSTR lpUsername,HTTPCSTR lpPassword,int AuthMethod);
	PREQUEST	SendHttpRequest(HTTPHANDLE HTTPHandle,HTTPCSTR VHost,HTTPCSTR HTTPMethod,HTTPCSTR url,HTTPCSTR Postdata,unsigned int PostDataSize);
	PREQUEST	SendHttpRequest(HTTPHANDLE HTTPHandle,HTTPCSTR HTTPMethod,HTTPCSTR url,HTTPCSTR Postdata,unsigned int PostDataSize);
	PREQUEST	SendHttpRequest(HTTPHANDLE HTTPHandle,HTTPCSTR VHost,HTTPCSTR HTTPMethod,HTTPCSTR url) ;
	PREQUEST    SendHttpRequest(HTTPHANDLE HTTPHandle,PHTTP_DATA request,HTTPCSTR lpUsername,HTTPCSTR lpPassword,int AuthMethod);
	PREQUEST	SendHttpRequest(HTTPHANDLE HTTPHandle,HTTPCSTR HTTPMethod,HTTPCSTR url) ;
	PREQUEST	SendHttpRequest(HTTPHANDLE HTTPHandle,HTTPCSTR url) ;
	PREQUEST	SendHttpRequest(HTTPCSTR Fullurl);
	int 		CancelHttpRequest(HTTPHANDLE HTTPHandle, int what);

	int			InitHTTPProxy(HTTPCSTR hostname, unsigned short port);
	int			InitHTTPProxy(HTTPCSTR hostname, HTTPCSTR port);
	void		SetHTTPProxyConfig(int opt,HTTPSTR parameter);
	void		SetHTTPProxyConfig(int opt,int parameter);
	int			StopHTTPProxy();

	int			RegisterHTTPCallBack(unsigned int cbType, HTTP_IO_REQUEST_CALLBACK cb,HTTPCSTR Description);
	void 	    doSpider(HTTPSTR host,HTTPSTR FullPath, PHTTP_DATA  response);
};

#endif