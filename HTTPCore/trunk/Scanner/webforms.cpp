
//TODO: permitir que CheckWebformAuth() realice mas de una verificaci�n
//#include "sslscanner.h"
#include "FHScan.h"
#include "estructuras.h"

#include "../HTTPCoreClass/Authentication/md5.h"
#include "../HTTPCoreClass/Authentication/base64.h"

#include "webforms.h"
#include "Reporting/LogSettings.h"

//#undef _DBG_

//TODO: snprintf(data->server,sizeof(data->server)-1,"(%s)",WEBFORMS[i].model);
// incluir el soporte de modelo cuando la aplicacion no encuentra vulnerabilidades...
//TODO: Verificar porque algunas autetnticaciones web forms dejan logs como:
//81.34.217.9 	0 	0 		root 	/password 	(null)

extern int nUsers;
extern	USERLOGIN	    *logins;
extern int   nLogins;
extern   USERLIST *userpass;
struct _webform WEBFORMS[MAX_WEBFORMS];
int nWebforms=0;//=sizeof(WEBFORMS)/sizeof(struct _webform);
extern int bruteforce;




static int what(char *code) {
	//devuelve un codigo de error dependiendo del formato de los datos
	if (strncmp(code,RAWUSER,13)==0) return(0);
	if (strncmp(code,RAWPASS,13)==0) return(1);
	if (strncmp(code,B64USER,13)==0) return(2);
	if (strncmp(code,B64PASS,13)==0) return(3);
	if (strncmp(code,MD5USER,13)==0) return(4);
	if (strncmp(code,MD5PASS,13)==0) return(5);
	if (strncmp(code,RAWIPAD,13)==0) return(6);
	if (strncmp(code,RAWPORT,13)==0) return(7);
	if (strncmp(code,RAWTIME,13)==0) return(8);
	if (strncmp(code,HD5USER,13)==0) return(9);
	if (strncmp(code,HD5PASS,13)==0) return(10);
	if (strncmp(code,"!!!MD5S(",8)==0) return(11);

	//!!!Match(where,lpstart,lpend)!!!
	/*
	Where: headers, data, all
	*/
	//!!!Fill(pattern,what,len)
	/*
	pattern: "\x01"
	what: !!!RAWUSER!!! , !!!Match(data,"var seed=",";");

	*/


	return(-1);
}

/************************************/


unsigned int MatchString(PREQUEST new_data, char *validauthstring)
{

	if (validauthstring[0]=='\0') return(0);
	if ( (new_data) && (new_data->IsValidHTTPResponse()) )
	{
		if (new_data->response->Headerstrstr(validauthstring)!=NULL)
		{
			return(1);
		}
		if (new_data->HasResponseData())
		{
			if (new_data->response->Datastrstr(validauthstring)!=NULL)
			{
				return(1);
			}
		}
	}

	return(0);
}



/*************************************************************************************************/
static void GenerateAuth(char *scheme, char *output, char *username, char *password,long ip,int port) {
	//Generamos los datos del POST en base al esquema de autenticacion..

	//printf("Generando con usuario: %s!! y pass: %s!!\n",username,password);
	memset(output,'\0',MAX_POST_LENGTH);
	char *opt;
	char *p;
	char *where=scheme;
	char tmp[100];
	do {
		opt=strstr(where,"!!!");
		if (opt)
		{
			memset(tmp,'\0',sizeof(tmp));
			strncat(output,where,opt-where);
			switch (what(opt))
			{
				case 0:
					//			strncat(output,username,sizeof(output)-1);
					strncat(output,username,MAX_POST_LENGTH -strlen(output));
					//printf("output vale: %s!!!!!!!!!!!\n",output);
					break;
				case 1:
					//	strncat(output,password,sizeof(output)-1);
					strncat(output,password,MAX_POST_LENGTH-strlen(output));
					break;
				case 2:
					Base64Encode((unsigned char *)tmp,(unsigned char *)username,( unsigned int )strlen(username));
					strncat(output,tmp,MAX_POST_LENGTH-strlen(output));
					break;
				case 3:
					Base64Encode((unsigned char *)tmp,(unsigned char *)password,( unsigned int )strlen(password));
					strncat(output,tmp,MAX_POST_LENGTH-strlen(output));
					break;
				case 4:
					Getmd5Hash(username,( unsigned int )strlen(username),(unsigned char *)tmp);
					strncat(output,tmp,MAX_POST_LENGTH-strlen(output));
					break;
				case 5:
					Getmd5Hash(password,( unsigned int )strlen(password),(unsigned char *)tmp);
					strncat(output,tmp,MAX_POST_LENGTH-strlen(output));
					break;
				case 6:
					struct sockaddr_in server;
					server.sin_addr.s_addr=ip;
					strncat(output,inet_ntoa(server.sin_addr),MAX_POST_LENGTH-strlen(output));
					break;
				case 7:
					snprintf(tmp,sizeof(tmp)-1,"%i",port);
					strncat(output,tmp,MAX_POST_LENGTH-strlen(output));
					break;

				case 8:
					struct timeb  localTime;
					ftime(&localTime);
					//               printf("fecha: %ld\n",localTime.time);               
					//               printf("fecha: %ld\n",localTime.millitm);
					snprintf(tmp,sizeof(tmp)-1,"%ld",localTime.time);
					strncat(output,tmp+4,MAX_POST_LENGTH-strlen(output));
					snprintf(tmp,sizeof(tmp)-1,"%ld",localTime.millitm);
					strncat(output,tmp,MAX_POST_LENGTH-strlen(output));


					//printf("fecha: %ld\n",localTime.millitm);               
					//printf("sumamos: %I64d\n",sum);

					break;
				case 9:
					Getmd5Hash(username,( unsigned int )strlen(username),(unsigned char *)tmp);
					tmp[16]='\0';
					strncat(output,tmp,MAX_POST_LENGTH-( unsigned int )strlen(output));
					break;
				case 10:
					Getmd5Hash(password,( unsigned int )strlen(password),(unsigned char *)tmp);
					tmp[16]='\0';
					strncat(output,tmp,MAX_POST_LENGTH-( unsigned int )strlen(output));
					break;
				case 11:
					//printf("analizando: -%s-\n",where);
					p= strstr(opt+8,")!!!");
					if  (p) {
						char encodedpacket[512];
						char decodedpacket[MAX_POST_LENGTH];
						memset(encodedpacket,0,sizeof(encodedpacket));
						memcpy(encodedpacket,opt+8,p-opt+8);

						p=strstr(encodedpacket,")!!!");
						if (p) *p=0;
						//printf("El esquema a analizar es: %s\n",encodedpacket);
						GenerateAuth(encodedpacket, decodedpacket,username,password,ip,port);
						//printf("El paquete decoded es: %s\n",decodedpacket);

						Getmd5Hash(decodedpacket,( unsigned int )strlen(decodedpacket),(unsigned char *)tmp);
						//printf("El hash es: %s\n",tmp);
						strncat(output,tmp,MAX_POST_LENGTH-( unsigned int )strlen(output));
						opt += 8 + strlen(encodedpacket) + 4 -13;

					}
					break;
				default: //invalid scheme
					strncat(output,opt,3);
					//opt+=3;
					opt-=10;
			}
			where=opt+13;
		} else
		{ //copiamos el final
			strncat(output,where,MAX_POST_LENGTH-( unsigned int )strlen(output));
		}
	} while(opt!=NULL);
	//printf("Hemos decodificado esto a: %s\n",output);
}

/*************************************************************************************************/


static int TryHTTPWebformAuth(HTTPAPI *api,HTTPHANDLE HTTPHandle, PREQUEST request, int webform)
{


	char post[MAX_POST_LENGTH];
	char InvalidAuthString[MAX_POST_LENGTH];
	char InvalidAuthStringalt[MAX_POST_LENGTH];
	char AdditionalHeader[MAX_POST_LENGTH];

	char *UserName,  *Password;
	int retry=RETRY_COUNT;
	int RequestRetry=2;
	PREQUEST new_data;

	int iteractions;
	int login;
	char tmp[512];
//	char *cookie = NULL;


	iteractions = (WEBFORMS[webform].requireloginandpass) ? nUsers : nLogins;

	for(login=0;login<iteractions ;login++) 
	{
		if (WEBFORMS[webform].requireloginandpass)
		{
			UserName=userpass[login].UserName;
			Password=userpass[login].Password;
		} else {
			UserName=Password=logins[login].user;
		}


		if ( (WEBFORMS[webform].InitialCookieURL[0]!='\0')  )
		{
			/* Request an additiona URL to get a cookie */
			PREQUEST InitialCookie=api->SendHttpRequest(HTTPHandle,NULL,"GET",WEBFORMS[webform].InitialCookieURL,NULL,0,NULL,NULL,0);
			if (InitialCookie)
			{
            	delete InitialCookie;
			}
		}
		GenerateAuth(WEBFORMS[webform].authform,post,UserName,Password,request->ip,request->port);
		*AdditionalHeader='\0';
		GenerateAuth(WEBFORMS[webform].AdditionalHeader,AdditionalHeader,UserName,Password,request->ip,request->port);
		//printf("Usando cabecera adicional: %s\n",AdditionalHeader);



#ifdef _DBG_
		printf("TryHTTPWebformAuth::User: !!%s!! pass: !!%s!!\n",UserName,Password);
		printf("\nTryHTTPWebformAuth::POST: %s\n",post);
		printf("\nTryHTTPWebformAuth::AdditionalHeader: %s\n\n",WEBFORMS[webform].AdditionalHeader);

#endif

		RequestRetry=2;
		do {
			if (AdditionalHeader[0]!='\0') api->SetHTTPConfig(HTTPHandle,ConfigCookie,AdditionalHeader);
			new_data=api->SendHttpRequest(HTTPHandle,NULL,WEBFORMS[webform].authmethod,WEBFORMS[webform].authurl,post,(unsigned int) strlen(post),NULL,NULL,NO_AUTH);
			if (AdditionalHeader[0]!='\0') api->SetHTTPConfig(HTTPHandle,ConfigCookie,NULL);

			if ( (!new_data) || (!new_data->IsValidHTTPResponse()) )
			{
				if (new_data)
				{
                    delete new_data;
					new_data = NULL;
				}
				if (retry==0)
				{
					return(0);
			 	}
				retry--;
				RequestRetry--;
			}
		} while ( (RequestRetry!=0) && (!new_data)  );


		if (new_data)
		{
			if  ( (new_data->status==404) ||
				  (!new_data->HasResponseData()) ||
				  (new_data->response->Datastrstr("CGI process file does not exist")) )
			{
				delete new_data;
				return(-2); //Invalid auth schema
			}

			if (new_data->status!=204)
			{ /* Avoid Null chars in the request */
				for(unsigned int i=0;i<10;i++)
				{
					if (i==new_data->response->DataSize) break;
					else if (new_data->response->Data[i]==0x00) new_data->response->Data[i]=' ';
				}
#ifdef _DBG_
				printf("TryHTTPWebformAuth::return: \n %s",new_data->response->Data);
#endif

				//printf("TryHTTPWebformAuth::return: \n %s",new_data->lpBuffer);
				if (strlen(WEBFORMS[webform].validauthstring)>0) {
					if ( (MatchString(new_data,WEBFORMS[webform].validauthstring)) || (MatchString(new_data,WEBFORMS[webform].validauthstringalt)) ) {
						memset(tmp,'\0',sizeof(tmp));
						//snprintf(tmp,sizeof(tmp)-1,"%s %s", WEBFORMS[webform].model,"(Password Found)");
						if (WEBFORMS[webform].requireloginandpass){
							UpdateHTMLReport(new_data,MESSAGE_WEBFORM_PASSFOUND,UserName,Password, WEBFORMS[webform].authurl,WEBFORMS[webform].model);
						} else {
							UpdateHTMLReport(new_data,MESSAGE_WEBFORM_PASSFOUND,UserName,"", WEBFORMS[webform].authurl,WEBFORMS[webform].model);
						}
						//new_data=(PREQUEST)FreeRequest(new_data);
						delete new_data;
						new_data = NULL;
						return(1);
					}
				} else
				{
					GenerateAuth(WEBFORMS[webform].invalidauthstring,InvalidAuthString,UserName,Password,request->ip,request->port);
					if (MatchString(new_data,InvalidAuthString)==0) {
						GenerateAuth(WEBFORMS[webform].invalidauthstringalt,InvalidAuthStringalt,UserName,Password,request->ip,request->port);
#ifdef _DBG_
						printf("TryHTTPWebformAuth::Invalid String not found (trying InvalidAuthStringAlt..)\n");
						printf("TryHTTPWebformAuth::InvalidAuthStringalt: %s\n",InvalidAuthStringalt);
#endif

						if ( ( strlen(WEBFORMS[webform].invalidauthstringalt)==0 ) || (MatchString(new_data,InvalidAuthStringalt)==0) )
						{
#ifdef _DBG_
							printf("TryHTTPWebformAuth::invalidauthstringalt not found ;)\n");
#endif
							memset(tmp,'\0',sizeof(tmp));
							//                snprintf(tmp,sizeof(tmp)-1,"%s %s",WEBFORMS[webform].model,"(Password Found)");
							if (WEBFORMS[webform].requireloginandpass) {
								UpdateHTMLReport(new_data,MESSAGE_WEBFORM_PASSFOUND,UserName,Password, WEBFORMS[webform].authurl,WEBFORMS[webform].model);
							} else {
								UpdateHTMLReport(new_data,MESSAGE_WEBFORM_PASSFOUND,UserName,"", WEBFORMS[webform].authurl,WEBFORMS[webform].model);
							}
							//new_data=(PREQUEST)FreeRequest(new_data);
							delete new_data;
							new_data = NULL;
							return(1);
						}
					}

				}
			}

			if (WEBFORMS[webform].ReconnectOnMatch[0]!='\0')
			{
				if (MatchString(new_data,WEBFORMS[webform].ReconnectOnMatch))
				{
					api->CancelHttpRequest(HTTPHandle,1);

				}
			}

			//new_data=(PREQUEST)FreeRequest(new_data);
			delete new_data;
			new_data = NULL;
			if (WEBFORMS[webform].LoadAdditionalUrl[0]!='\0')
			{
				new_data=api->SendHttpRequest(HTTPHandle,NULL,"GET",WEBFORMS[webform].LoadAdditionalUrl,NULL,0,NULL,NULL,0);
				if (new_data)
				{
					delete new_data;
					new_data = NULL;
				}

			}
		}
	}
	memset(tmp,'\0',sizeof(tmp));
	//snprintf(tmp,sizeof(tmp)-1,"%s %s",WEBFORMS[webform].model,"(No password Matches)");
	//if ( !IsKnownWebServer(data->server,nKnownWebservers,KnownWebservers) 
	UpdateHTMLReport(request,MESSAGE_WEBFORMS_PASSNOTFOUND,"NOTFOUND","NOTFOUND", WEBFORMS[webform].authurl,WEBFORMS[webform].model);
	return(-1);
}


/*************************************************************************************************/

int CheckWebformAuth(HTTPAPI *api,HTTPHANDLE HTTPHandle,PREQUEST data, int pos)
{
	//verifica en base a firmas si debemos realizar una autenticacion por webforms

	int ret;

	if (!bruteforce) return(-1);
#ifdef _DBG_
	printf("nWebforms vale: %i\n",nWebforms);
	printf("Verificando datos: %s\n",data->response->Data);
#endif
	for(int i=pos;i<nWebforms;i++)
	{
		if (data->status==WEBFORMS[i].status)
		{

			#ifdef _DBG_
			printf("CheckWebformAuth::data->status: %i == WEBFORMS[%i].status: %i\n",data->status,i,WEBFORMS[i].status);
			printf("CheckWebformAuth::strlen: %i\n", strlen(data->server));
			#endif
			if ( (strlen(WEBFORMS[i].server)==0) ||
				(strcmp(data->server,WEBFORMS[i].server)==0)  ||
				( (strlen(data->server)==0) && (strcmp(WEBFORMS[i].server,"HTTP/1.0")==0) ) )
			{
				#ifdef _DBG_
				printf("CheckWebformAuth::vamos a verificar el string %i\n",i);
				#endif

				if ( (data->response->Headerstrstr(WEBFORMS[i].matchstring)!=NULL) || ( (data->HasResponseData()) && (data->response->Datastrstr(WEBFORMS[i].matchstring)!=NULL)  ) )
				{

				#ifdef _DBG_
					printf("CheckWebformAuth::match: %s\n\n",WEBFORMS[i].matchstring);
				#endif
					if (strlen(WEBFORMS[i].ValidateImage)==0)
					{
#ifdef _DBG_
						printf("CheckWebformAuth::ValidateImage len=0 %s\n\n",WEBFORMS[i].ValidateImage);
#endif
						ret = TryHTTPWebformAuth(api,HTTPHandle,data,i);
						if (ret==-2) return CheckWebformAuth(api,HTTPHandle,data,i+1);
						return(ret);

					} else
					{
						/*                  char image[200];
						sprintf(image,"GET %s HTTP/1.0\r\nHost: %s\r\n\r\n",WEBFORMS[i].ValidateImage,data->ipaddress);
						PREQUESTnew_data=(PREQUEST)conecta(data->ip,data->port, data->ssl,image);
						*/
						PREQUEST new_data=api->SendHttpRequest(HTTPHandle,NULL,"GET",WEBFORMS[i].ValidateImage,NULL,0,NULL,NULL,NO_AUTH);
						if (new_data)
						{
							///if ( (new_data->status==200) && (strstr(new_data->resultado,"Content-Type: image/")!=NULL) ) {
							if ( (new_data->IsValidHTTPResponse()) && (new_data->status==200) )
							{
#ifdef _DBG_
								printf(" CheckWebformAuth::Validateimage: MATCH %s\n",WEBFORMS[i].ValidateImage);
#endif
								delete new_data;
								new_data = NULL;
								ret = TryHTTPWebformAuth(api,HTTPHandle,data,i);
								if (ret==-2) return CheckWebformAuth(api,HTTPHandle,data,i+1);
								return(ret);
							} else
							{

#ifdef _DBG_
								printf(" CheckWebformAuth::Validateimage: Not found %s\n",WEBFORMS[i].ValidateImage);
								printf("%s",new_data->response->Data);
#endif
								delete new_data;
								new_data = NULL;
							}

						}

					}
				} else {
#ifdef _DBG_
					printf("CheckWebformAuth::no encontrado: %s\n",WEBFORMS[i].matchstring);
#endif
					//printf("datos: %s\n",data->resultado);
				}
			} else {
#ifdef _DBG_
				printf ("CheckWebformAuth No match %s - %s\n", data->server,WEBFORMS[i].server);
#endif
			}
		}
	}
#ifdef _DBG_
	printf("salimos...\n");
#endif   
	return(0);
}



