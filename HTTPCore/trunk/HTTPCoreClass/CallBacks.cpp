#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CallBacks.h"
#include "Build.h"


/*******************************************************************************************************/
HTTPCALLBACK::HTTPCALLBACK()
{
	CBItems = 0;
	CBList = NULL;
	ParentHTTPApi = NULL;
}
/*******************************************************************************************************/
HTTPCALLBACK::~HTTPCALLBACK()
{
	for (unsigned int i=0;i<CBItems ;i++)
    {
		if (CBList[i].lpDescription) free(CBList[i].lpDescription);			
    }
	if (CBList) free(CBList);
	CBList = NULL;
	CBItems = 0;
}

/**********************************************************************************************************************/
//! This function Registers an HTTP Callback Handler and is called by external plugins
/*!
	\param cbType CallBack Type. Valid options are CBTYPE_CLIENT_REQUEST , CBTYPE_CLIENT_RESPONSE , CBTYPE_BROWSER_REQUEST , CBTYPE_SERVER_RESPONSE. Use CBTYPE_CALLBACK_ALL to match every possible callback (including undefined ones).
    \param cb CallBack Address. This is the Address of the CallBack Function that will receive HTTP parameters.
    \return If an error is detected, for example an already added callback, 0 is returned.
	\note Registered callback functions are also responsible for handling undefined CallBack types. If a registered callback function does not know how to handle an specific callback type must ingore the data.
    For more information read the plugin development documentation.
*/
/**********************************************************************************************************************/
int HTTPCALLBACK::RegisterHTTPCallBack(unsigned int cbType, HTTP_IO_REQUEST_CALLBACK cb,const char *Description)
{

	for (unsigned int i=0;i<CBItems;i++)
	{
		if (CBList[i].cb == cb) return( 0 );
	}
	CBList=(PCB_LIST)realloc(CBList,sizeof(CB_LIST)*++CBItems);
    CBList[CBItems-1].cbType=cbType;
    CBList[CBItems-1].cb=cb;
	if (Description)
	{
		CBList[CBItems-1].lpDescription = strdup(Description);
	} else {
		CBList[CBItems-1].lpDescription = NULL;
	}
	//printf("REGISTRADO tipo %i---- TOTAL: %i\n",cbType,CBItems);
	return(1);

}
/**********************************************************************************************************************/
//! This function unregisters a previously loaded Callback
/*!
	\param cbType CallBack Type. Valid options are CBTYPE_CLIENT_REQUEST , CBTYPE_CLIENT_RESPONSE , CBTYPE_BROWSER_REQUEST , CBTYPE_SERVER_RESPONSE or CBTYPE_CALLBACK_ALL to match every possible callback
    \param cb CallBack Address. This is the Address of the CallBack Function that was receiving HTTP parameters.
	\return Returns the number of removed Callbacks.
	\note Its possible to remove all Callback types against a function using CBTYPE_CALLBACK_ALL.
*/
/**********************************************************************************************************************/

int  HTTPCALLBACK::RemoveHTTPCallBack(unsigned int cbType, HTTP_IO_REQUEST_CALLBACK cb)
{
    unsigned int ret=0;
    for (unsigned int i=0;i<=CBItems -1;i++)
    {
        if ( (cb==NULL) || (CBList[i].cb == cb ) )
        {
            if (CBList[i].cbType & cbType)
            {
                CBList[i].cb=NULL;
				if (CBList[i].lpDescription)
				{
					free(CBList[i].lpDescription);
					CBList[i].lpDescription = NULL;
				}
                ret++;
            }
        }
    }
    if (ret==CBItems) 
    {
        free(CBList);
        CBList=NULL;
        CBItems=0;
    }
    return(ret);
}
/**********************************************************************************************************************/
//! CallBack Dispatcher. This function is called from the HTTPCore module ( SendRawHttpRequest() ) and from the HTTPProxy Module DispatchHTTPProxyRequest() and will send http information against registered callbacks
/*!
	\param cbType CallBack Source Type. The value specifies where the data comes from. The valid options are CBTYPE_CLIENT_REQUEST , CBTYPE_CLIENT_RESPONSE , CBTYPE_PROXY_REQUEST , CBTYPE_PROXY_RESPONSE
	\param HTTPHandle HTTP Connection Handle with information about remote target (like ip address, port, ssl, protocol version,...)
	\param request struct containing all information related to the HTTP Request.
	\param response struct containing information about http reponse. This parameter could be NULL if the callback type is CBTYPE_CLIENT_RESPONSE or CBTYPE_PROXY_RESPONSE because request was not send yet.
	\return the return value CBRET_STATUS_NEXT_CB_CONTINUE indicates that the request (modified or not) its ok. If a registered handler blocks the request then CBRET_STATUS_CANCEL_REQUEST is returned. This value indicates that the request or response is not authorized to be delivery to the destionation.\n
    \note a Blocked PHTTP_DATA request or response could be used for example when a plugin is implementing a popup filtering.
*/
/**********************************************************************************************************************/
int HTTPCALLBACK::DoCallBack(int cbType,HTTPHANDLE HTTPHandle,PHTTP_DATA request,PHTTP_DATA response)
{ 
    unsigned int i;
	int ret;
	//printf("REALIZANDO CALLBACKS: %i\n",CBItems);
    for (i=0; i<CBItems;i++)
    {
        if ( (CBList[i].cbType & cbType) && (CBList[i].cb) )
		{
		  //printf("LLAMANDO A CALLBACK %i: %s\n",i,CBList[i].lpDescription);
			ret=CBList[i].cb (
                cbType,
				ParentHTTPApi,
				HTTPHandle,
				request,
				response);
            if (ret & CBRET_STATUS_NEXT_CB_BLOCK)
                break;
            if (ret & CBRET_STATUS_CANCEL_REQUEST)
            {
                return(CBRET_STATUS_CANCEL_REQUEST);
            }
        }
    }     
    return( CBRET_STATUS_NEXT_CB_CONTINUE );
}

/*******************************************************************************************************/