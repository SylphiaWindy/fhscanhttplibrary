#ifndef __AUTHSCANNER_
#define __AUTHSCANNER_

 #include "../HTTPCore/HTTP.h"
 #include "../HTTPCore/Threading.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __WIN32__RELEASE__
 #include <sys/timeb.h>
 #include <process.h>
 #include <windows.h>
 #include <time.h>
// #pragma comment(lib, "../HTTPCoreClass/Release/HTTPCoreClass.lib")

 # if defined(_MSC_VER)
 # ifndef _CRT_SECURE_NO_DEPRECATE
 # define _CRT_SECURE_NO_DEPRECATE (1)
 # endif
 # pragma warning(disable : 4996)
 # endif
/*
#define snprintf _snprintf
#define stricmp  _stricmp
#define strnicmp _strnicmp
*/
#else

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>  //pthread
#include <ctype.h> //toupper
#include <time.h>
#include <sys/timeb.h>

#define MAX_PATH 256
#define MAXIMUM_WAIT_OBJECTS 64
#define _strdup strdup
#define BOOL int
#define closesocket close
#define strnicmp strncasecmp
#define  stricmp strcasecmp
#define ioctlsocket ioctl
#define Sleep usleep
#define CRITICAL_SECTION pthread_mutex_t
#endif





//configuration
#define MAX_USER_LIST   200
#define MAX_AUTH_LIST   200
#define MAX_WEBFORMS    200
#define MAX_PORTS       100
#define RETRY_COUNT     6


#define MAX_POST_LENGTH	8192
#endif

