
#include "sip.h"
#include "log.h"
#include <assert.h>
#include <stdlib.h>        // free()函数
#include <osipparser2/osip_md5.h>
#include <sstream>		// stringstream
#include "md5.h"
#include "parse_xml.h"

#ifdef _WIN32
    #ifdef _DEBUG
        #define USE_DEBUG_OUTPUT
    #endif
    #define STDCALL __stdcall
#elif __linux__
    #define USE_DEBUG_OUTPUT
    #define STDCALL __attribute__((__stdcall__))
#else
#endif

#define USE_DEBUG_OUTPUT

#define max(a,b) ((a) > (b)) ? (a) : (b)
#define min(a,b) ((a) < (b)) ? (a) : (b)
// item的个数
#define ITEM_NUM 4


std::string CSip::random_string(int length)
{
	std::stringstream str_random;		// #include <sstream>
#if 0
	static char str[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	static int len = strlen(str);
	//srand(time(NULL));				// 初始化随机数种子
	for (int i = 0; i < length; i++) {
		str_random << str[rand() % len];
	}
	//static int min = 1000000;
	//static int max = 9999999999;
	//srand(time(NULL));
	//unsigned int nRand = rand() % (max - min + 1) + min;
#else
	if (36 == length) {
		static char str[] = "0123456789abcdef";
		static int len = strlen(str);
		for (int i = 0; i < length; i++) {
			if ((8 == i) || (13 == i) || (18 == i) || (23 == i))
				str_random << '-';
			else str_random << str[rand() % len];
		}
	}
	else {
		static char str[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
		static int len = strlen(str);
		//srand(time(NULL));				// 初始化随机数种子
		for (int i = 0; i < length; i++) {
			str_random << str[rand() % len];
		}
	}
#endif
	return str_random.str();

}

CSip::CSip()
{
    init();
}

CSip::~CSip()
{
    release();
}

int CSip::init()
{
    int nRet = -1;
	if (NULL != m_pContext)
	{
		LOG("WARN", "m_pContext=%p\n", m_pContext);
		osip_free(m_pContext);
		m_pContext = NULL;
	}
	m_pContext = eXosip_malloc();
	if(NULL != m_pContext)
	{
		nRet = eXosip_init(m_pContext);	// 初始化osip和eXosip协议栈
		if (0 != nRet)
		{
			osip_free(m_pContext);
			m_pContext = NULL;
			#ifdef USE_DEBUG_OUTPUT
			LOG("ERROR", "Couldn't initialize eXosip [%d]\n", nRet);
			#endif
		}
	}
	#ifdef USE_DEBUG_OUTPUT
	else LOG("ERROR", "eXosip_malloc() failed [%d]\n", nRet);
	#endif
	return nRet;
}

void CSip::release()
{
    if (NULL != m_pContext)
    {
		osip_free(m_pContext);
		m_pContext = NULL;
	}
}

#ifdef USE_GB28181_SERVER
int CSip::open(const char *ip, int port)
{
    assert(NULL != m_pContext);
    int nRet = eXosip_listen_addr(m_pContext, IPPROTO_UDP, ip, port, AF_INET, 0);
    if (0 == nRet)
    {
        //LOG("INFO","Listening %s:%d ...\n\n", ip, port);
        m_evt_thd_flag = true;
        m_evt_thd = osip_thread_create(20000, event_proc, this);
    }
    #ifdef USE_DEBUG_OUTPUT
    else LOG("ERROR","Listen failure [%s:%d][err:%d]\n", ip, port, nRet);
    #endif
    return nRet;
}

void CSip::close()
{
    if(NULL != m_evt_thd)
    {
        m_evt_thd_flag = false;
        osip_thread_join(m_evt_thd);
        osip_free(m_evt_thd);
        m_evt_thd = NULL;
    }
    
    if(NULL != m_pContext)
	    eXosip_quit(m_pContext);
}
#endif


int CSip::guess_local_ip(char *address, int size, int family/*=AF_INET*/)
{
    assert(NULL != m_pContext);
    return eXosip_guess_localip(m_pContext, family, address, size);
}

void* CSip::event_proc(void *arg)
{
    CSip *pSip = (CSip*)arg;
    while(pSip->m_evt_thd_flag)
    {
		eXosip_event_t *p_evt = eXosip_event_wait(pSip->m_pContext, 0, 50);// 监听事件
        eXosip_lock(pSip->m_pContext);
        eXosip_automatic_action(pSip->m_pContext);
        eXosip_unlock(pSip->m_pContext);
        //LOG("INFO","No event.\n");
		if (NULL == p_evt) {
			osip_usleep(1000);
			continue;
		}

		if(NULL != pSip->m_pEvtCbk)
		{
			pSip->m_pEvtCbk(p_evt, pSip->m_pUserData);
		}
        eXosip_event_free(p_evt);
    }

    osip_thread_exit();
	return NULL;
}

void CSip::set_callback(EVT_CBK pCallback, void* pUserData)
{
	m_pEvtCbk = pCallback;
	m_pUserData = pUserData;
}

int CSip::get_from(const osip_message_t *request, sip_from_t *from)
{
	int nRet = -1;
	if((NULL != request) && (NULL != from))
	{
		osip_from_t *osip_from = osip_message_get_from(request);
		if((NULL != osip_from) && (NULL != osip_from->url))
		{
            if(NULL != osip_from->url->username)
				strcpy(from->username, osip_from->url->username);
            if(NULL != osip_from->url->host)
                strcpy(from->ip, osip_from->url->host);
            if(NULL != osip_from->url->port)
                from->port = atoi(osip_from->url->port);

            osip_uri_param_t *from_tag = NULL;
            osip_from_get_tag(osip_from, &from_tag);
            if(NULL != from_tag)
                strcpy(from->tag, from_tag->gvalue);
            nRet = 0;
        }
    }
    return nRet;
}

int CSip::get_to(const osip_message_t *request, sip_to_t *to)
{
	int nRet = -1;
	if((NULL != request) && (NULL != to))
	{
		osip_to_t *osip_to = osip_message_get_to(request);
		if((NULL != osip_to) && (NULL != osip_to->url))
		{
            //osip_to->url->password;
            if(NULL != osip_to->url->username)
				strcpy(to->username, osip_to->url->username);
            if(NULL != osip_to->url->host)
                strcpy(to->ip, osip_to->url->host);
            if(NULL != osip_to->url->port)
                to->port = atoi(osip_to->url->port);

            osip_uri_param_t *to_tag = NULL;
            osip_from_get_tag(osip_to, &to_tag);
            if(NULL != to_tag)
                strcpy(to->tag, to_tag->gvalue);
            nRet = 0;
        }
    }
    return nRet;
}

int CSip::get_via(const osip_message_t *request, sip_via_t *via, int pos/*=0*/)
{
    int nRet = -1;
	if((NULL != request) && (NULL != via))
	{
		osip_via_t *v = NULL;
		nRet = osip_message_get_via(request, 0, &v);
		if((0 == nRet) && (NULL != v))
		{
			#if 0
			char *dest = NULL;
			osip_via_to_str(v, &dest);
			if(NULL != dest) {
				osip_free(dest);
			}
			#endif

			#if 0
				const char *host = NULL;
				int port = 0;
				osip_generic_param_t* br = NULL;
				osip_via_param_get_byname(v, (char*)"received", &br);
				if ((br != NULL) && (br->gvalue != NULL))
					host = br->gvalue;
				else host = v->host;

				osip_via_param_get_byname(v, (char*)"rport", &br);
				if ((NULL != br) && (NULL != br->gvalue))
					port = atoi(br->gvalue);
				else port = atoi(v->port);
                //osip_via_set_branch(v,"AAAAAAAA");
				if(NULL != host) {
					strcpy(via->ip, host);
					via->port = port;
				}
				else nRet = -1;
			#else
                if(NULL != v->host)
                    strcpy(via->ip, v->host);
                if(NULL != v->port)
                    via->port = atoi(v->port);
                #if 0
                if(NULL != v->version)
                    strncpy(via->version, v->version, sizeof(via->version)-1);
                if(NULL != v->protocol)
                    strncpy(via->protocol, v->protocol, sizeof(via->protocol)-1);
                if(NULL != v->via_params) {
                    osip_generic_param_t* br = NULL;
                    osip_via_param_get_byname(v, (char*)"rport", &br);
                    if ((br != NULL) && (br->gvalue != NULL))
                        via->rport = atoi(br->gvalue);
                    osip_via_param_get_byname(v, (char*)"branch", &br);
                    if ((br != NULL) && (br->gvalue != NULL))
                        strncpy(via->branch, br->gvalue, sizeof(via->branch));
                }
                #endif
			#endif
		}
		else LOG("ERROR", "osip_message_get_via failed  %d\n", nRet);
	}
	return nRet;
}
int CSip::get_contact(const osip_message_t *request, sip_contact_t *contact, int pos/*=0*/)
{
	int nRet = -1;
	if (NULL != request)
	{
		osip_contact_t *dest = NULL;
		nRet = osip_message_get_contact(request, pos, &dest);
		if ((0 == nRet) && (NULL != dest) && (NULL != dest->url))
		{
			if (NULL != dest->url->scheme)
				strncpy(contact->scheme, dest->url->scheme, sizeof(contact->scheme) - 1);
			if (NULL != dest->url->username)
				strncpy(contact->username, dest->url->username, sizeof(contact->username) - 1);
			if (NULL != dest->url->password)
				strncpy(contact->password, dest->url->password, sizeof(contact->password) - 1);
			if (NULL != dest->url->host)
				strncpy(contact->ip, dest->url->host, sizeof(contact->ip) - 1);
			if (NULL != dest->url->port)
				contact->port = atoi(dest->url->port);
			nRet = 0;
		}
	}
	return nRet;
}
std::string CSip::get_call_id(const osip_message_t *sip)
{
    if(NULL != sip)
    {
        osip_call_id_t *cid = osip_message_get_call_id(sip);
        if(NULL != cid)
        {
            char *dest = NULL;
            osip_call_id_to_str(cid, &dest);
            if(NULL != dest)
            {
                std::string call_id(dest);
                osip_free(dest);
                return call_id;
            }    
        }
    }
    return "";
}

int CSip::set_call_id(osip_message_t *sip, const char *call_id)
{
	if ((NULL != sip) && (NULL != call_id))
	{
		if (NULL != sip->call_id)
		{
			//osip_free(sip->call_id->number);
			//sip->call_id->number = osip_strdup(call_id);
			osip_call_id_free(sip->call_id);
			sip->call_id = NULL;
		}
		return osip_message_set_call_id(sip, call_id);
	}
	return -1;
}
int CSip::set_cseq(osip_message_t *sip, unsigned int number, const char *method)
{
	int nRet = -1;
	if ((NULL != sip) && (NULL != sip->cseq))
	{
		if (number >= 0) {
			osip_free(sip->cseq->number);
			char buff[16] = { 0 };
			sprintf(buff, "%u", number);
			sip->cseq->number = osip_strdup(buff);
			nRet = 0;
		}
		if (NULL != method) {
			osip_free(sip->cseq->method);
			sip->cseq->method = osip_strdup(method);
			nRet = 0;
		}
		//osip_cseq_free(sip->cseq);
		//sip->cseq = NULL;
		//osip_cseq_set_number(sip->cseq, osip_strdup("20"));
		//osip_cseq_set_method(sip->cseq, osip_strdup("REGISTER"));
		//osip_message_set_cseq(sip, osip_strdup("102 REGISTER"));
	}
	else LOG("ERROR","sip=%p\n", sip);
	return nRet;
}

void CvtHex (char* Bin, char* Hex)
{
  unsigned short i;
  unsigned char j;

  for (i = 0; i < 16; i++) {
    j = (Bin[i] >> 4) & 0xf;
    if (j <= 9)
      Hex[i * 2] = (j + '0');
    else
      Hex[i * 2] = (j + 'a' - 10);
    j = Bin[i] & 0xf;
    if (j <= 9)
      Hex[i * 2 + 1] = (j + '0');
    else
      Hex[i * 2 + 1] = (j + 'a' - 10);
  };
  Hex[32] = '\0';
}

osip_authorization_t* CSip::get_authorization(const osip_message_t *sip, int pos/*=0*/)
{
	if(NULL != sip)
	{
		osip_authorization_t *auth = NULL;
		osip_message_get_authorization(sip, pos, &auth);
		return auth;
	}
	return NULL;
}

int CSip::authorization(const osip_authorization_t *auth, const char *password)
{
    // 算法流程:
	// 1） HASH1 = MD5(username:realm:password)
	// 2） HASH2 = MD5(method:uri)
	// 3） Response = MD5(HA1:nonce:HA2)
    // 4） 比较注册请求中的Response与上面计算出来的Response是否相等,若相等则鉴权成功,否则失败

	//char *username = osip_authorization_get_username(auth);
	//if (NULL != username) {
	//	char *pszUserName = osip_strdup_without_quote(username); // 拷贝内存信息,去掉两头的引号
	//	if(NULL != pszUserName)
	//		osip_free(pszUserName);
	//}
    
    int nRet = -1;
    if((NULL != auth) && (NULL != password))
    {
        char *pAlgorithm = (NULL != auth->algorithm) ? osip_strdup_without_quote(auth->algorithm) : NULL;
        char *pUsername = (NULL != auth->username) ? osip_strdup_without_quote(auth->username) : NULL;
        char *pRealm = (NULL != auth->realm) ? osip_strdup_without_quote(auth->realm) : NULL;
        char *pNonce = (NULL != auth->nonce) ? osip_strdup_without_quote(auth->nonce) : NULL;
        char *pNonceCount = (NULL != auth->nonce_count) ? osip_strdup_without_quote(auth->nonce_count) : NULL;
        char *pUri = (NULL != auth->uri) ? osip_strdup_without_quote(auth->uri) : NULL;
        char *pResponse = (NULL != auth->response) ? osip_strdup_without_quote(auth->response) : NULL;
        if(pAlgorithm && pUsername && pRealm && pNonce && pUri && pResponse)
        {
            char szResponse[128] = {0};
            #if 1
                char szHASH1[128] = {0};
                sprintf(szHASH1, "%s:%s:%s", pUsername, pRealm, password);

                char szHASH2[128] = {0};
                sprintf(szHASH2, "REGISTER:%s", pUri);

                MD5 md5;
                md5.reset();
                md5.update(szHASH1);
                //string strHASH1 = md5.toString();
                strcpy(szHASH1, md5.toString().c_str());
                szHASH1[md5.toString().length()] = '\0';

                md5.reset();
                md5.update(szHASH2);
                //string strHASH2 = md5.toString();
                strcpy(szHASH2, md5.toString().c_str());
                szHASH2[md5.toString().length()] = '\0';

                //sprintf(szResponse, "%s:%s:%s", strHASH1.c_str(), pNonce, strHASH2.c_str());
                sprintf(szResponse, "%s:%s:%s", szHASH1, pNonce, szHASH2);
                md5.reset();
                md5.update(szResponse);
                //string strResponse = md5.toString();
                strcpy(szResponse, md5.toString().c_str());
                szResponse[md5.toString().length()] = '\0';
            #else
                //DigestCalcHA1();
                osip_MD5_CTX Md5Ctx;
                ////////////// 计算HA1 /////////////////
                //HASH HA1;
                char HA1[16] = {0};
                osip_MD5Init(&Md5Ctx);
                osip_MD5Update(&Md5Ctx, (unsigned char *) pUsername, (unsigned int)strlen (pUsername));
                osip_MD5Update(&Md5Ctx, (unsigned char *) ":", 1);
                osip_MD5Update(&Md5Ctx, (unsigned char *) pRealm, (unsigned int)strlen (pRealm));
                osip_MD5Update(&Md5Ctx, (unsigned char *) ":", 1);
                osip_MD5Update(&Md5Ctx, (unsigned char *) password, (unsigned int)strlen (password));
                osip_MD5Final((unsigned char*)HA1, &Md5Ctx);
                if((pAlgorithm != NULL) && osip_strcasecmp(pAlgorithm, "md5-sess") == 0)
                {
                    osip_MD5Init(&Md5Ctx);
                    osip_MD5Update(&Md5Ctx, (unsigned char*)HA1, 16);
                    osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);
                    osip_MD5Update(&Md5Ctx, (unsigned char*)pNonce, (unsigned int) strlen (pNonce));
                    osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);
                    //osip_MD5Update (&Md5Ctx, (unsigned char*)pszCNonce, (unsigned int) strlen (pszCNonce));
                    osip_MD5Final((unsigned char*)HA1, &Md5Ctx);
                }
                //HASHHEX SessionKey;
                char SessionKey[64];
                CvtHex(HA1, SessionKey);

                ////////////// 计算HA2 /////////////////
                char HA2[16] = {0};
                char HA2Hex[64] = {0};
                char szMethod[] = {"REGISTER"};     // sip->sip_method
                osip_MD5Init(&Md5Ctx);
                osip_MD5Update(&Md5Ctx, (unsigned char*)szMethod, (unsigned int)strlen(szMethod));
                osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);
                osip_MD5Update(&Md5Ctx, (unsigned char*)pUri, (unsigned int)strlen(pUri));
                osip_MD5Final((unsigned char*)HA2, &Md5Ctx);
                CvtHex(HA2, HA2Hex);

                char RespHash[16] = {0};
                osip_MD5Init(&Md5Ctx);
                osip_MD5Update(&Md5Ctx, (unsigned char*)HA1, 16);
                osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);
                osip_MD5Update(&Md5Ctx, (unsigned char*)pNonce, (unsigned int)strlen(pNonce));
                osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);
                osip_MD5Update(&Md5Ctx, (unsigned char *) HA2Hex, 32);
                osip_MD5Final((unsigned char*) RespHash, &Md5Ctx);
                CvtHex(RespHash, szResponse);
            #endif

            nRet = (0 == memcmp(pResponse, szResponse, strlen(szResponse))) ? 0 : -1;
        }
        if (NULL != pAlgorithm)
            osip_free(pAlgorithm);
        if (NULL != pUsername)
            osip_free(pUsername);
        if (NULL != pRealm)
            osip_free(pRealm);
        if (NULL != pNonce)
            osip_free(pNonce);
        if (NULL != pNonceCount)
            osip_free(pNonceCount);
        if (NULL != pUri)
            osip_free(pUri);
        if(NULL != pResponse)
            osip_free(pResponse);
    }
    return nRet;
}

int CSip::answer_401(int tid, const char *pRealm, const char *pNonce, const char* call_id/*=NULL*/)
{
	// 回复401 Unauthorized（无权限）响应,表明要求对UAC进行用户认证，并且通过WWW-Authenticate字段携带UAS
	// 支持的认证方式，产生本次认证的nonce		
	osip_www_authenticate_t *header = NULL;
	int nRet = osip_www_authenticate_init(&header);
	if((0 == nRet) && (NULL != header))
	{
		osip_www_authenticate_set_auth_type (header, osip_strdup("Digest"));
		osip_www_authenticate_set_realm(header, osip_enquote(pRealm));
		osip_www_authenticate_set_nonce(header, osip_enquote(pNonce));
		//osip_www_authenticate_set_domain(header, osip_enquote("ZKZKZKZK"));

		char *pDest = NULL;
		nRet |= osip_www_authenticate_to_str(header, &pDest);
		if((0 == nRet) && (NULL != pDest))
		{
            eXosip_lock(m_pContext);
			osip_message_t* answer = NULL;
			nRet |= eXosip_message_build_answer (m_pContext, tid, 401, &answer);
			if((0 == nRet) && (NULL != answer))
			{
				if(NULL != call_id)
					set_call_id(answer, call_id);

				osip_message_set_www_authenticate(answer, pDest);
				osip_message_set_content_type(answer, "Application/MANSCDP+xml");
				
				nRet |= eXosip_message_send_answer(m_pContext, tid, 401, answer);
				
				#ifdef USE_DEBUG_OUTPUT
				print_event(answer);
				#endif
				//osip_message_free(answer);
			}
			else LOG("ERROR", "eXosip_message_build_answer failed %d\n\n", nRet);
            eXosip_unlock(m_pContext);
			osip_free(pDest);
		}
		else LOG("ERROR", "osip_www_authenticate_to_str failed %d\n\n", nRet);
		osip_www_authenticate_free(header);	// 错误
		//osip_free(header);				// 正确
	}
	else LOG("ERROR", "osip_www_authenticate_init failed %d\n\n", nRet);
	return nRet;
}

int CSip::answer(int tid, int status, const char* call_id/*=NULL*/)
{
    assert(NULL != m_pContext);
    osip_message_t *answer = NULL;
    eXosip_lock(m_pContext);
    int nRet = eXosip_message_build_answer(m_pContext, tid, status, &answer);
    if((0 == nRet) && (NULL != answer))
	{
		if(200 == status)
		{
			char szTime[32] = {0};
			#ifdef _WIN32
				SYSTEMTIME sys;
				GetLocalTime(&sys);
				sprintf(szTime,"%d-%02d-%02dT%02d:%02d:%02d.%03d", sys.wYear, sys.wMonth, sys.wDay, sys.wHour, sys.wMinute, sys.wSecond, sys.wMilliseconds);
			#elif __linux__
				struct timeval	tv;
				struct timezone	tz;
				gettimeofday(&tv, &tz);
				struct tm *pt = localtime(&tv.tv_sec);
				sprintf(szTime,"%d-%02d-%02dT%02d:%02d:%02d.%03d", 1900+pt->tm_year, 1+pt->tm_mon, pt->tm_mday, pt->tm_hour, pt->tm_min, pt->tm_sec, tv.tv_usec/1000);
			#endif
			//osip_message_set_topheader(answer, "date", "2020-12-10T14:05:30.375");
			osip_message_set_topheader(answer, "date", szTime);
		}
		if(NULL != call_id)
			nRet = set_call_id(answer, call_id);

        nRet = eXosip_message_send_answer(m_pContext, tid, status, answer);
       
		#ifdef USE_DEBUG_OUTPUT
		print_event(answer);
		#endif
		//osip_message_free(answer);
    }
	#ifdef USE_DEBUG_OUTPUT
	else LOG("ERROR", "build message answer failed [ret:%d call_id:%s tid:%d answer:%p]\n\n", nRet, call_id, tid, answer);
	#endif
    eXosip_unlock(m_pContext);
    return nRet;
}

int CSip::get_expires(const osip_message_t *request)
{
    int nExpires = -1;
	if(NULL != request)
	{
		osip_header_t *det = NULL;
		int nRet = osip_message_get_expires(request, 0, &det);
		if(NULL != det)
        {
			if(NULL != det->hvalue)
				nExpires = atoi(det->hvalue);
		}
		else LOG("ERROR", "osip_message_get_expires() failed, error code %d.\n", nRet);
	}
	return nExpires;
}

const char* CSip::get_body(const osip_message_t *request)
{
  	if(NULL != request)
	{
		osip_body_t *body = NULL;
		int nRet = osip_message_get_body(request, 0, &body);
		if((0 == nRet) && (NULL != body) && (NULL != body->body))
			return body->body;
	}
	return NULL;  
}

int CSip::query_device(addr_info_t *from, addr_info_t *to, const operation_t *opt, const char *dev_id/*=NULL*/)
{
	int nRet = -1;
	if(NULL != opt->body)
	{
		nRet = message_request(opt->body, from, to, NULL, opt->call_id);
	}
	else
	{
		std::stringstream xml;
		//xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n";
		xml << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
		xml << "<Query>\r\n";
		switch (opt->cmd)
		{
			case QUERY_DEV_INFO: {		// 查询设备信息
				xml << "<CmdType>DeviceInfo</CmdType>\r\n";
			};break;
			case QUERY_DEV_CATALOG: {	// 查询设备目录信息
				xml << "<CmdType>Catalog</CmdType>\r\n";
			};break;
			case QUERY_DEV_STATUS: {	// 查询设备状态信息
				xml << "<CmdType>DeviceStatus</CmdType>\r\n";
			};break;
			case QUERY_DEV_RECORD: {	// 查询录像
				xml << "<CmdType>RecordInfo</CmdType>\r\n";
				if(NULL != opt->record)
				{
					if(1 == opt->record->type)
					    xml << "<Type>all</Type>\r\n";	// 录像类型,可选(1:all 2:manual(手动录像) 3:time 4:alarm )
                    else if(2 == opt->record->type)
                        xml << "<Type>manual</Type>\r\n";
                    else if(3 == opt->record->type)
                        xml << "<Type>time</Type>\r\n";
                    else if(4 == opt->record->type)
                        xml << "<Type>alarm</Type>\r\n";
					xml << "<StartTime>" << opt->record->start_time << "</StartTime>\r\n";
					xml << "<EndTime>" << opt->record->end_time << "</EndTime>\r\n";
					//xml << "<StartTime>" << "2021-01-11 02:02:35" << "</StartTime>\r\n";
					//xml << "<EndTime>" << "2021-01-11 02:03:35" << "</EndTime>\r\n";
					//xml << "<Address>Address 1</Address>\r\n";		// 录像地址(可选, 支持不完全查询)
					//xml << "<Secrecy>0</Secrecy>\r\n";				// 保密属性,可选 (缺省为0, 0:不涉密, 1:涉密)
					//xml << "<FilePath>file_path</FilePath>\r\n";		// 文件路径名 (可选)
					//xml << "<RecorderID>1001</RecorderID>\r\n";		// 录像触发者ID (可选)
					//xml << "<IndistinctQuery>0</IndistinctQuery>\r\n";	// 录像模糊查询属性,可选,缺省为0;
					//														0:不进行模糊查询,此时根据SIP消息中To头域URI中的ID值确定查询录像位置,若ID值为本域系统ID则进行中心历史记录检索,若为前端设备ID则进行前端设备历史记录检索;
					//														1:进行模糊查询,此时设备所在域应同时进行中心检索和前端检索并将结果统一返回
				}
			}break;
			case QUERY_DEV_CFG: {		// 查询设备配置信息
			// 查询配置参数类型(必选),可查询的配置类型包括:
			// 基本参数配置: "BasicParam"
			// 视频参数范围: "VideoParamOpt"
			// SVAC 编码配置: "SVACEncodeConfig"
			// SVAC 解码配置: "SVACDe-codeConfig"
			// 可同时查询多个配置类型,各类型以“/”分隔,可返回与查询SN值相同的多个响应,每个响应对应一个配置类型。
				xml << "<CmdType>ConfigDownload</CmdType>\r\n";
				xml << "<ConfigType>BasicParam</ConfigType>\r\n";
				//xml << "<ConfigType>SVACDecodeConfig</ConfigType>\r\n"
			}break;
			default : {
				LOG("ERROR","未知的操作命令: %d\n\n", opt->cmd);
				return -1;
			}
		}
		xml << "<SN>" << m_sn++ << "</SN>\r\n";
		if(NULL != dev_id)
			xml << "<DeviceID>" << dev_id << "</DeviceID>\r\n";
		else xml << "<DeviceID>" << to->id << "</DeviceID>\r\n";
		xml << "</Query>\r\n";
		nRet = message_request(xml.str().c_str(), from, to, NULL, opt->call_id);
	}
	return nRet;
}

int CSip::control_device(addr_info_t *from, addr_info_t *to, const operation_t *opt)
{
	int nRet = -1;
	if(NULL != opt->body)
	{
		nRet = message_request(opt->body, from, to, NULL, opt->call_id);
	}
	else
	{
		std::stringstream body;
		//if((CTRL_PLAYBAK_CTRL == opt->cmd) && (NULL != opt->pbk_ctrl))// 回播控制
		{
			body << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
			body << "<Control>\r\n";
			body << "<DeviceID>" << to->id << "</DeviceID>\r\n";
			body << "<SN>" << m_sn++ << "</SN>\r\n";
            if(CTRL_DEV_CFG == opt->cmd)   // 配置设备
            {
                body << "<CmdType>DeviceConfig</CmdType>\r\n";
                body << "<BasicParam>\r\n";
                if(NULL != opt->basic_param)
                {
                    body << "<Name>" << opt->basic_param->name << "</Name>\r\n";
                    body << "<Expiration>" << opt->basic_param->expires << "</Expiration>\r\n";
                    body << "<HeartBeatInterval>" << opt->basic_param->keepalive << "</HeartBeatInterval>\r\n";
                    body << "<HeartBeatCount>" << opt->basic_param->max_k_timeout << "</HeartBeatCount>\r\n";
                }
                body << "</BasicParam>\r\n";
            }
            else
            {
                body << "<CmdType>DeviceControl</CmdType>\r\n";
                switch (opt->cmd)
                {
                    case CTRL_PTZ_LEFT: {		// 向左
						LOG("INFO", "  ------------ [向左转] ------------\n");
                        body << "<PTZCmd>A50F0102B400006B</PTZCmd>\r\n";
                    }break;
                    case CTRL_PTZ_RIGHT: {	// 向右
						LOG("INFO", "  ------------ [向右转] ------------\n");
                        body << "<PTZCmd>A50F0101B400006A</PTZCmd>\r\n";
                    }break;
                    case CTRL_PTZ_UP:	{		// 向上
						LOG("INFO", "  ------------ [向上转] ------------\n");
                        body << "<PTZCmd>A50F010800B40071</PTZCmd>\r\n";
                    }break;
                    case CTRL_PTZ_DOWN: {		// 向下
						LOG("INFO", "  ------------ [向下转] ------------\n");
                        body << "<PTZCmd>A50F010400B4006D</PTZCmd>\r\n";
                    }break;
                    case CTRL_PTZ_LARGE: {	// 放大
						LOG("INFO", "  ------------ [放大] ------------\n");
                        body << "<PTZCmd>A50F01100000A065</PTZCmd>\r\n";
                    }break;
                    case CTRL_PTZ_SMALL: {	// 缩小
						LOG("INFO", "  ------------ [缩小] ------------\n");
                        body << "<PTZCmd>A50F01200000A075</PTZCmd>\r\n";
                    }break;
                    case CTRL_PTZ_STOP: {	// 停止遥控
						LOG("INFO", "  ------------ [停止] ------------\n");
                        body << "<PTZCmd>A50F0100000000B5</PTZCmd>\r\n";
                    }break;
                    case CTRL_REC_START: {	// 开始手动录像
						LOG("INFO", "  ------------ [开始录像] ------------\n");
                        body << "<RecordCmd>Record</RecordCmd>\r\n";
                    }break;
                    case CTRL_REC_STOP: {	// 停止手动录像
						LOG("INFO", "  ------------ [停止录像] ------------\n");
                        body << "<RecordCmd>StopRecord</RecordCmd>\r\n";
                    }break;
                    case CTRL_GUD_START: {	// 布防
						LOG("INFO", "  ------------ [布防] ------------\n");
                        body << "<GuardCmd>SetGuard</GuardCmd>\r\n";
                    }break;
                    case CTRL_GUD_STOP: {	// 撤防
						LOG("INFO", "  ------------ [撤防] ------------\n");
                        body << "<GuardCmd>ResetGuard</GuardCmd>\r\n";
                    }break;
                    case CTRL_ALM_RESET: {	// 报警复位
						LOG("INFO", "  ------------ [报警复位] ------------\n");
                        body << "<AlarmCmd>ResetAlarm</AlarmCmd>\r\n";
                    }break;
                    case CTRL_TEL_BOOT: {	// 设备远程启动
						LOG("INFO", "  ------------ [设备远程重启] ------------\n");
                        body << "<TeleBoot>Boot</TeleBoot>\r\n";
                    }break;
                    case CTRL_IFR_SEND: {	// 强制关键帧(I帧)
						LOG("INFO", "  ------------ [强制关键帧] ------------\n");
                        body << "<IFameCmd>Send</IFameCmd>\r\n";
                    }break;
                    default : {
						LOG("INFO", "未知的操作命令: %d\n\n", opt->cmd);
                        return -1;
                    }
                }
            }
			#if 0
				// 控制优先
				body << "<Info>\r\n";
				body << "<ControlPriority>5</ControlPriority>\r\n";
				body << "</Info>\r\n";
			#endif
			body << "</Control>\r\n";
		}
        nRet = message_request(body.str().c_str(), from, to, NULL, opt->call_id);
	}
	#if 0
		<AlarmCmd>0/ResetAlarm</AlarmCmd>//报警复位命令(可选)
		<IFameCmd>0/Send</IFameCmd>		// 强制关键帧命令,设备收到此命令应立刻发送一个 IDR 帧(可选)
		<DragZoomIn></DragZoomIn>		// 拉框放大控制命令(可选)
	#endif
	return nRet;
}

int CSip::message_request(const char *body, addr_info_t *from, addr_info_t *to, addr_info_t *route/*=NULL*/, const char* call_id/*=NULL*/)
{
	assert((NULL != m_pContext) && (NULL != from) && (NULL != to));

    char from_[128] = {0};
	if(NULL != from)
    	sprintf(from_, "sip:%s@%s:%d", from->id, from->ip, from->port);
	
	char to_[128] = {0};
	if(NULL != to)
    	sprintf(to_, "sip:%s@%s:%d", to->id, to->ip, to->port);
	
	char route_[128] = {0};
	char *p_route = NULL;
	if(NULL != route)
	{
    	sprintf(route_, "sip:%s@%s:%d", route->id, route->ip, route->port);
		p_route = route_;
	}
	osip_message_t *msg = NULL;
    eXosip_lock(m_pContext);
	int nRet = eXosip_message_build_request(m_pContext, &msg, "MESSAGE", to_, from_, p_route);
	if((0 == nRet) && (NULL != msg))
	{
		#if 0	// 释放旧的via,设置新的via
		if(osip_list_size(&msg->vias) > 0)
			osip_list_ofchar_free(&msg->vias);
		char via[128] = {0};
		snprintf(via, 128, "SIP/2.0/UDP %s:%d;rport=%d;branch=z9hG4bK%u",from->ip, from->port, from->port, osip_build_random_number());
		nRet |= osip_message_set_via(msg, via);
		//LOG("INFO","via size:%d, ret:%d\n\n", osip_list_size(&msg->vias), nRet);
        #else
			#if 0
            osip_via_t *via = NULL;
            osip_message_get_via(msg, 0, &via);
            if(NULL != via)
            {
                via_set_host(via, osip_strdup(from->ip));
                char port[8] = {0};
                sprintf(port, "%d", from->port);
                via_set_port(via, osip_strdup(port));

                osip_generic_param_t* rport = NULL;
                osip_via_param_get_byname(via, (char*)"rport", &rport);
                if ((NULL != rport) && (NULL != rport->gvalue)) {
					//LOG("INFO","1 rport:%s\n\n", rport->gvalue);
                    strcpy(rport->gvalue, port);
					//LOG("INFO", "2 rport:%s\n\n", rport->gvalue);
				}
				else {
					LOG("INFO", "rport=%p, rport->gvalue=%p\n\n", rport, rport->gvalue);
					rport->gvalue = osip_strdup(port);
				}
            }
			else LOG("INFO","via=%p\n\n", via);
			#endif
		#endif

		if(NULL != call_id)
			set_call_id(msg, call_id);
		
		if(NULL != body) {
			nRet = osip_message_set_body(msg, body, strlen(body));
            if(0 != nRet)
				LOG("ERROR","osip_message_set_body() failed ret:%d\n\n", nRet);
        }

		nRet = osip_message_set_content_type(msg, "Application/MANSCDP+xml");
        if(0 != nRet)
			LOG("ERROR","osip_message_set_content_type() failed ret:%d\n\n", nRet);

		nRet = eXosip_message_send_request(m_pContext, msg);
		if(0 != nRet)
			LOG("ERROR", "eXosip_message_send_request() failed ret:%d\n\n", nRet);
		#ifdef USE_DEBUG_OUTPUT
        print_event(msg);
		#endif
		//osip_message_free(msg);
	}
	else {
		LOG("ERROR", "message_build_request failed [ret=%d msg=%p from=%s to=%s]\n\n", nRet, msg, from_, to_);
	}
    eXosip_unlock(m_pContext);
	return nRet;
}

int CSip::call_request(int did, const char *body, const char* call_id/*=NULL*/)
{
	int nRet = -1;
	if ((did > 0) && (NULL != body))
	{
		osip_message_t *request = NULL;
		nRet = eXosip_call_build_info(m_pContext, did, &request);
		if ((0 == nRet) && (NULL != request))
		{
			nRet |= osip_message_set_body(request, body, strlen(body));
			if (NULL != call_id)
				nRet |= set_call_id(request, call_id);
			nRet |= osip_message_set_content_type(request, "application/RTSP");
			string tm = "";
			#if 0
			// 毫秒级
			struct timeval	tv;
			struct timezone	tz;
			gettimeofday(&tv, &tz);
			struct tm *pt = localtime(&tv.tv_sec);
			char szTime[32] = { 0 };
			sprintf(szTime, "%d-%02d-%02dT%02d:%02d:%02d.%03d", 1900 + pt->tm_year, 1 + pt->tm_mon, pt->tm_mday, pt->tm_hour, pt->tm_min, pt->tm_sec, tv.tv_usec / 1000);
			tm = szTime;
			#else
			tm = timestamp_to_string(time(NULL));
			#endif
			osip_message_set_topheader(request, "Date", tm.c_str());
			nRet |= eXosip_call_send_request(m_pContext, did, request);
			print_event(request);
			if (0 != nRet)
				LOG("ERROR","call_request failed %d \n\n", nRet);
		}
		else LOG("ERROR","request=%p, ret=%d\n\n", request, nRet);
	}
	else LOG("ERROR","did:%d, body:%p\n\n", did, body);
	return nRet;
}

int CSip::call_request(const char *dev_id, addr_info_t *from, addr_info_t *to, const char* call_id/*=NULL*/)
{
	if ((NULL == dev_id) || (NULL == from) || (NULL == to))
		return -1;

	stringstream xml;
	xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n";
	xml << "<Notify>\r\n";
	xml << "  <CmdType>MediaStatus</CmdType>\r\n";
	xml << "  <SN>" << m_sn++ << "</SN>\r\n";
	xml << "  <DeviceID>" << dev_id << "</DeviceID>\r\n";
	xml << "  <NotifyType>121</NotifyType>\r\n";
	xml << "</Notify>\r\n";
	return message_request(xml.str().c_str(), from, to, NULL, call_id);
}


#ifdef USE_GB28181_CLIENT
int CSip::register_remove(int rid)
{
	int nRet = -1;
	if (NULL != m_pContext)
	{
		nRet = eXosip_register_remove(m_pContext, rid);
	}
	return nRet;
}
int CSip::regist(addr_info_t *from, pfm_info_t *pfm, bool is_auth, int rid/*=0*/, const char *realm/*=NULL*/, const char *to_tag/*=NULL*/,size_t cseq/*=1*/)
{
    assert(NULL != m_pContext);
	static int register_id = 0;
	char fm[128] = {0};		// 格式: "sip:主叫用户名@源域名"
    char to[128] = {0};		// 格式: "sip:主叫用户名@源域名"
	sprintf(fm, "sip:%s@%s:%d", from->id, from->ip, from->port);
	//sprintf(fm, "sip:%s@%s", from->id, realm);
	sprintf(to, "sip:%s@%s:%d", pfm->addr.id, pfm->addr.ip, pfm->addr.port);
	//eXosip_masquerade_contact(m_pContext, from->ip, from->port);
	//eXosip_set_user_agent(m_pContext, "PJSUA v2.6 Linux-4.19.95.211/aarch64/glibc-2.28");
	osip_message_t *reg = NULL;
	int nRet = -1;
	if(is_auth)
	{
		nRet = eXosip_clear_authentication_info(m_pContext);	// 清除旧的认证信息
		// 添加主叫用户的认证信息
		nRet = eXosip_add_authentication_info(m_pContext, pfm->username, pfm->username, pfm->password, "MD5", (NULL!=realm)?realm:pfm->domain);
		nRet = eXosip_register_build_register(m_pContext, rid, pfm->expires, &reg);
		if((0 == nRet) && (NULL != reg))
		{
			#if 0	// 释放旧的via,设置新的via
			if(osip_list_size(&reg->vias) > 0)
				osip_list_ofchar_free(&reg->vias);
			char via[128] = {0};
			snprintf(via, 128, "SIP/2.0/UDP %s:%d;rport=%d;branch=z9hG4bK%u",from->ip, from->port, from->port, osip_build_random_number());
			nRet |= osip_message_set_via(reg, via);
			//LOG("INFO","via size:%d, ret:%d\n\n", osip_list_size(&msg->vias), nRet);
			if(NULL != to_tag)
			{
				osip_to_t *sip_to = osip_message_get_to(reg);
				if(NULL != sip_to)
				{
					if(osip_list_size(&sip_to->gen_params) > 0)
						osip_list_ofchar_free(&sip_to->gen_params);
					osip_to_set_tag(sip_to, (char*)to_tag);
				}
			}
			#endif
			// 发送携带认证信息的注册请求 
            eXosip_lock(m_pContext);
			nRet = eXosip_register_send_register(m_pContext, rid, reg);
            eXosip_unlock(m_pContext);
			if(0 != nRet)
				LOG("ERROR","send_register failed (ret:%d, rid:%d)\n\n", nRet, rid);
		}
		else LOG("ERROR","build_register failed (ret:%d, reg:%p, rid:%d\n\n", nRet, reg, rid);
	}
	else
	{
		char contacts[256] = { 0 };
		sprintf(contacts, "<sip:%s@%s:%d>", from->id, from->ip, from->port);

        eXosip_lock(m_pContext);
		// 发送不带认证信息的注册请求
		register_id = eXosip_register_build_initial_register(m_pContext, fm, to, contacts, pfm->expires, &reg);
		if((register_id >= 0) && (NULL != reg)) {
			// From:
			osip_from_free(reg->from);
			reg->from = NULL;
			char buf[256] = {0};
			sprintf(buf, "%s;tag=%s", fm, random_string(36).c_str());
			osip_message_set_from(reg, buf);
			
			// Via:
			if(osip_list_size(&reg->vias) > 0)
				osip_list_ofchar_free(&reg->vias); // 释放旧的via,设置新的via
			char via[256] = {0};
			snprintf(via, 128, "SIP/2.0/UDP %s:%d;rport;branch=z9hG4bKPj%s",from->ip, from->port, random_string(36).c_str());
			nRet |= osip_message_set_via(reg, via);
			// Call-ID:
			set_call_id(reg, random_string(36).c_str());
			//set_call_id(reg, "eda69056-a0d0-4403-a0df-ea521306d392");
			// Contact:
			if (osip_list_size(&reg->contacts) > 0)
				osip_list_ofchar_free(&reg->contacts);
			char contacts[256] = {0};
			sprintf(contacts, "<sip:%s@%s:%d;ob>", from->id, from->ip, from->port);
			osip_message_set_contact(reg, contacts);
			// Cseq:
			static size_t num = cseq;
			set_cseq(reg, num++, NULL);
			nRet = eXosip_register_send_register(m_pContext, register_id, reg);
			if(0 != nRet)
				LOG("ERROR","send_register failed (ret:%d, register_id:%d)\n\n", nRet, register_id);
		}
		else LOG("ERROR","build_initial_register failed (ret:%d, reg:%p, register_id:%d\n\n", register_id, reg, register_id);
        eXosip_unlock(m_pContext);
	}
	#ifdef USE_DEBUG_OUTPUT
	if(NULL != reg)
		print_event(reg);
	#endif
	//if(NULL != reg)
	//	osip_message_free(reg);
	return nRet;
}

int CSip::unregist(addr_info_t *from, pfm_info_t *pfm, bool is_auth, int rid/*=0*/, const char *realm/*=NULL*/, const char *to_tag/*=NULL*/)
{
    int expires = pfm->expires;
    pfm->expires = 0;
    int nRet = regist(from, pfm, is_auth, rid, realm, to_tag);
    pfm->expires = expires;
    return nRet;
}

osip_www_authenticate_t* CSip::get_www_authenticate(const osip_message_t *sip, int pos/*=0*/)
{
	if(NULL != sip)
	{
		osip_www_authenticate_t *www_auth = NULL;
		osip_message_get_www_authenticate(sip, pos, &www_auth);
		return www_auth;
	}
	return NULL;
}
int CSip::keepalive(addr_info_t *from, addr_info_t *to, int sn/*=0*/, const char *call_id/*=NULL*/)
{
	static int ssn = 1;
	stringstream xml;
	xml << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
	xml << "<Notify>\r\n";
	xml << "<CmdType>Keepalive</CmdType>\r\n";
	if (sn <= 0)
		sn = ssn++;
	xml << "<SN>" << sn << "</SN>\r\n";
	xml << "<DeviceID>" << from->id << "</DeviceID>\r\n";
	xml << "<Status>OK</Status>\r\n";
	xml << "</Notify>\r\n";
	//LOG("INFO","-------- 向平台'%s:%s:%d'发起 心跳保活 --------\n", ser->addr.id, ser->addr.ip, ser->addr.port);
	return message_request(xml.str().c_str(), from, to, NULL, call_id);
}
#endif

time_t CSip::string_to_timestamp(const char *time_str)
{
	struct tm time;
    #if 1
        if (time_str[4] == '-' || time_str[4] == '.' || time_str[4] == '/')
        {
            char szFormat[30] = { 0 };				//"%u-%u-%u","%u.%u.%u","%u/%u/%u")
            sprintf(szFormat, "%%d%c%%d%c%%dT%%d:%%d:%%d", time_str[4], time_str[4]);
            sscanf(time_str, szFormat, &time.tm_year, &time.tm_mon, &time.tm_mday, &time.tm_hour, &time.tm_min, &time.tm_sec);
            time.tm_year -= 1900;
            time.tm_mon -= 1;
            return mktime(&time);
        }
    #else
        // 字符串转时间戳
        struct tm tm_time;
        strptime("2010-11-15 10:39:30", "%Y-%m-%d %H:%M:%S", &time);
        return mktime(&time);
    #endif
	return -1;
}

string CSip::timestamp_to_string(time_t t, char ch/*='-'*/)
{
	#define buff_size 64
	char buff[buff_size] = {0};
    #if 0
		// localtime获取系统时间并不是线程安全的(函数内部申请空间,返回时间指针)
        //time_t tick = time(NULL);
        struct tm *pTime = localtime(&t);
        if (ch == '-')
            strftime(buff, buff_size, "%Y-%m-%dT%H:%M:%S", pTime);
        else if (ch == '/')
            strftime(buff, buff_size, "%Y/%m/%d %H:%M:%S", pTime);
        else if (ch == '.')
            strftime(buff, buff_size, "%Y.%m.%d %H:%M:%S", pTime);
        else strftime(buff, buff_size, "%Y%m%d%H%M%S", pTime);
    #else
		#ifdef _WIN32
			// localtime_s也是用来获取系统时间,运行于windows平台下,与localtime_r只有参数顺序不一样
			struct tm now_time;
			localtime_s(&now_time, &t);
			sprintf(buff, "%4.4d%c%2.2d%c%2.2dT%2.2d:%2.2d:%2.2d", now_time.tm_year + 1900, ch, now_time.tm_mon + 1, ch, now_time.tm_mday,
				now_time.tm_hour, now_time.tm_min, now_time.tm_sec);
		#elif __linux__
			// localtime_r也是用来获取系统时间,运行于linux平台下,支持线程安全
			//struct tm* localtime_r(const time_t * timep, struct tm* result);
			struct tm tm1;
			localtime_r(&t, &tm1);
			sprintf(buff, "%4.4d%c%2.2d%c%2.2dT%2.2d:%2.2d:%2.2d", tm1.tm_year + 1900, ch, tm1.tm_mon + 1, ch, tm1.tm_mday, tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
		#endif
	#endif
	return string(buff);
}
/*
int CSip::ansi_to_utf8(const char *pData, int nLen, char *pBuf)
{
	//步骤: 先将ANSI串转换为UNICODE串,然后再将UNICODE串转换为UTF-8串
	int iLen = MultiByteToWideChar(CP_ACP, 0, pData, nLen, NULL, 0);	//得到Unicode串的缓冲区所必需的宽字符数
	if (iLen > 0) {
		//第一步: Ansi转换为Unicode
		wchar_t *pwBuf = new wchar_t[iLen + 1];
		iLen = MultiByteToWideChar(CP_ACP, 0, pData, nLen, (LPWSTR)pwBuf, iLen);
		pwBuf[iLen] = 0;

		//第二步: Unicode转换为UTF-8
		int nRet = WideCharToMultiByte(CP_UTF8, 0, pwBuf, iLen, NULL, 0, NULL, NULL);//得到UTF-8串的缓冲区所必需的字符数
		if (nRet > 0) {
			iLen = WideCharToMultiByte(CP_UTF8, 0, pwBuf, iLen, pBuf, nRet, NULL, NULL);
		}
		delete pwBuf;
	}
	return iLen;
}
*/

int CSip::message_response(cmd_e cmd_type, dev_info_t *dev, addr_info_t *from, addr_info_t *to, addr_info_t *route/*=NULL*/)
{
    int nRet = -1;
    stringstream xml;
    if(QUERY_DEV_INFO == cmd_type)
    {
		//xml << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\n";
		//xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n";
		xml << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
		//xml << "<?xml version=\"1.0\"?>\n";
		xml << "<Response>\n";
		xml << "  <CmdType>DeviceInfo</CmdType>\n";
		xml << "  <SN>" << m_sn++ << "</SN>\n";
		xml << "  <DeviceID>"<< dev->addr.id << "</DeviceID>\n";
		xml << "  <Result>OK</Result>\n";
		xml << "  <DeviceName>" << dev->name << "</DeviceName>\n";
		xml << "  <Manufacturer>" << dev->manufacturer << "</Manufacturer>\n";
		xml << "  <Model>" << dev->model << "</Model>\n";
		xml << "  <Firmware>" << dev->firmware << "</Firmware>\n";
		xml << "  <Channel>" << dev->chl_map.size() << "</Channel>\n";
		xml << "</Response>\n";
        nRet = message_request(xml.str().c_str(), from, to, NULL, NULL);
    }
    else if(QUERY_DEV_STATUS == cmd_type)
    {
        xml << "<?xml version=\"1.0\"?>\n";
		//xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        xml << "<Response>\n";
        xml << "  <CmdType>DeviceStatus</CmdType>\n";
        xml << "  <SN>" << m_sn++ << "</SN>\n";
        xml << "  <DeviceID>"<< dev->addr.id << "</DeviceID>\n";
        xml << "  <Result>OK</Result>\n";
        xml << "  <Online>ONLINE</Online>\n";
        xml << "  <Status>OK</Status>\n";
        xml << "  <DeviceTime>" << timestamp_to_string(time(NULL)) << "</DeviceTime>\n";
        xml << "  <Alarmstatus Num=\"0\">\n";
        xml << "  </Alarmstatus>\n";
        xml << "  <Encode>OFF</Encode>\n";
        xml << "  <Record>OFF</Record>\n";
        xml << "</Response>\n";
        nRet = message_request(xml.str().c_str(), from, to, NULL, NULL);
    }
	else if(QUERY_DEV_CATALOG == cmd_type)
    {
		map<string, chl_info_t> chl_map;
		chl_map = dev->chl_map;
		stringstream xml;
		if (chl_map.size() > 0) {
			int i = 0, sum_num = chl_map.size();
			auto chl = chl_map.begin();
			int sn = m_sn++;
			for (; chl != chl_map.end();) {
				if (0 == i) {
					xml.clear();
					xml.str("");
					xml << "<?xml version=\"1.0\"?>\n";
					xml << "<Response>\n";
					xml << "  <CmdType>Catalog</CmdType>\n";
					xml << "  <SN>" << sn << "</SN>\n";
					xml << "  <DeviceID>" << dev->addr.id << "</DeviceID>\n";
					xml << "  <SumNum>" << chl_map.size() << "</SumNum>\n";
					xml << "  <DeviceList Num=\"" << (min(sum_num, ITEM_NUM)) << "\">\n";
				}
				if (i < ITEM_NUM) {
					xml << "    <Item>\n";
					xml << "      <DeviceID>" << chl->second.id << "</DeviceID>\n";
					xml << "      <Name>" << chl->second.name << "</Name>\n";
					xml << "      <Manufacturer>" << chl->second.manufacturer << "</Manufacturer>\n";
					xml << "      <Model>" << chl->second.model << "</Model>\n";
					xml << "      <Owner>" << chl->second.owner << "</Owner>\n";
					xml << "      <CivilCode>" << chl->second.civil_code << "</CivilCode>\n";
					xml << "      <Address>" << chl->second.address << "</Address>\n";
					xml << "      <Parental>" << (int)chl->second.parental << "</Parental>\n";		// 是否有子设备(必选) 1 有, 0 没有 
					xml << "      <ParentID>" << chl->second.parent_id << "</ParentID>\n";
					xml << "      <SafetyWay>" << (int)chl->second.safety_way << "</SafetyWay>\n";
					xml << "      <RegisterWay>" << (int)chl->second.register_way << "</RegisterWay>\n";
					xml << "      <Secrecy>" << (int)chl->second.secrecy << "</Secrecy>\n";
					xml << "      <Status>" << ((1 == chl->second.status) ? "ON" : "OFF") << "</Status>\n";
					xml << "      <IPAddress>" << chl->second.ip_address << "</IPAddress>\n";
					xml << "      <Port>" << chl->second.port << "</Port>\n";
					xml << "    </Item>\n";
					++i;
					++chl;
				}
				else {
					xml << "  </DeviceList>\n";
					xml << "</Response>\n";
					nRet = message_request(xml.str().c_str(), from, to, NULL, NULL);
					sum_num -= ITEM_NUM;
					i = 0;
				}
			}
			if (0 != i) {
				xml << "  </DeviceList>\n";
				xml << "</Response>\n";
				nRet = message_request(xml.str().c_str(), from, to, NULL, NULL);
			}
		}
		else {
			xml << "<?xml version=\"1.0\"?>\n";
			xml << "<Response>\n";
			xml << "  <CmdType>Catalog</CmdType>\n";
			xml << "  <SN>" << m_sn++ << "</SN>\n";
			xml << "  <DeviceID>" << dev->addr.id << "</DeviceID>\n";
			xml << "  <SumNum>" << 0 << "</SumNum>\n";
			xml << "  <DeviceList Num=\"" << 0 << "\">\n";
			xml << "  </DeviceList>\n";
			xml << "</Response>\n";
			nRet = message_request(xml.str().c_str(), from, to, NULL, NULL);
		}
    }
	else if(QUERY_DEV_RECORD == cmd_type)
	{
		#if 1
		stringstream xml;
		if (dev->rec_map.size() > 0) {
			int i = 0, sum_num = dev->rec_map.size();
			auto rec = dev->rec_map.begin();
			int sn = m_sn++;
			for (; rec != dev->rec_map.end();) {
				if (0 == i) {
					xml.clear();
					xml.str("");
					xml << "<?xml version=\"1.0\"?>\n";
					xml << "<Response>\n";
					xml << "  <CmdType>RecordInfo</CmdType>\n";
					xml << "  <SN>" << sn << "</SN>\n";
					xml << "  <DeviceID>" << dev->addr.id << "</DeviceID>\n";
					xml << "  <Name>" << rec->second.name << "</Name>\n";
					xml << "  <SumNum>" << dev->rec_map.size() << "</SumNum>\n";
					xml << "  <RecordList Num=\"" << (min(sum_num, ITEM_NUM)) << "\">\n";
				}
				if (i < ITEM_NUM) {
					xml << "    <Item>\n";
					xml << "      <DeviceID>" << dev->addr.id << "</DeviceID>\n";
					xml << "      <Name>" << rec->second.name << "</Name>\n";
					xml << "      <FilePath>" << rec->second.file_path << "</FilePath>\n";
					xml << "      <Address>" << rec->second.address << "</Address>\n";
					xml << "      <StartTime>" << rec->second.start_time << "</StartTime>\n";
					xml << "      <EndTime>" << rec->second.end_time << "</EndTime>\n";
					xml << "      <Secrecy>" << (int)rec->second.secrecy << "</Secrecy>\n";
					char type[16] = {0}; // 录像类型(1:all 2:manual(手动录像) 3:time 4:alarm)
					switch (rec->second.type) {
						case 1: strcpy(type, "all"); break;
						case 2: strcpy(type, "manual"); break;
						case 3: strcpy(type, "time"); break;
						case 4: strcpy(type, "alarm"); break;
						default: {
							LOG("ERROR","Unknown Record type %d\n\n", rec->second.type);
							strcpy(type, "unknown");
						}break;
					}
					xml << "      <Type>" << type << "</Type>\n";
					xml << "      <FileSize>" << rec->second.size << "</FileSize>\n";
					xml << "    </Item>\n";
					++i;
					++rec;
				}
				else {
					xml << "  </RecordList>\n";
					xml << "</Response>\n";
					nRet = message_request(xml.str().c_str(), from, to, NULL, NULL);
					sum_num -= ITEM_NUM;
					i = 0;
				}
			}
			if (0 != i) {
				xml << "  </RecordList>\n";
				xml << "</Response>\n";
				nRet = message_request(xml.str().c_str(), from, to, NULL, NULL);
			}
		}
		else {
			xml << "<?xml version=\"1.0\"?>\n";
			xml << "<Response>\n";
			xml << "  <CmdType>RecordInfo</CmdType>\n";
			xml << "  <SN>" << m_sn++ << "</SN>\n";
			xml << "  <DeviceID>" << dev->addr.id << "</DeviceID>\n";
			xml << "  <Name>" << dev->name << "</Name>\n";
			xml << "  <SumNum>" << 0 << "</SumNum>\n";
			xml << "  <RecordList Num=\"" << 0 << "\">\n";
			xml << "  </RecordList>\n";
			xml << "</Response>\n";
			nRet = message_request(xml.str().c_str(), from, to, NULL, NULL);
		}
		#endif
	}
	else if(QUERY_DEV_CFG == cmd_type)
	{
	}
    return nRet;
}

int CSip::subscribe_answer(int tid, int status, const char* call_id/*=NULL*/)
{
	if (NULL != m_pContext) {
		LOG("ERROR","m_pContext=%p\n\n", m_pContext);
		return -1;
	}
	osip_message_t *answer = NULL;
	eXosip_lock(m_pContext);
	int nRet = eXosip_insubscription_build_answer(m_pContext, tid, status, &answer);
	if ((0 == nRet) && (NULL != answer))
	{
		if (200 == status)
		{
			char szTime[32] = { 0 };
			#ifdef _WIN32
			SYSTEMTIME sys;
			GetLocalTime(&sys);
			sprintf(szTime, "%d-%02d-%02dT%02d:%02d:%02d.%03d", sys.wYear, sys.wMonth, sys.wDay, sys.wHour, sys.wMinute, sys.wSecond, sys.wMilliseconds);
			#elif __linux__
			struct timeval	tv;
			struct timezone	tz;
			gettimeofday(&tv, &tz);
			struct tm *pt = localtime(&tv.tv_sec);
			sprintf(szTime, "%d-%02d-%02dT%02d:%02d:%02d.%03d", 1900 + pt->tm_year, 1 + pt->tm_mon, pt->tm_mday, pt->tm_hour, pt->tm_min, pt->tm_sec, tv.tv_usec / 1000);
			#endif
			//osip_message_set_topheader(answer, "date", "2020-12-10T14:05:30.375");
			osip_message_set_topheader(answer, "date", szTime);
		}
		if (NULL != call_id)
			nRet = set_call_id(answer, call_id);

		nRet = eXosip_insubscription_send_answer(m_pContext, tid, status, answer);

		#ifdef USE_DEBUG_OUTPUT
		print_event(answer);
		#endif
		//osip_message_free(answer);
	}
	else LOG("ERROR", "build message answer failed [ret:%d call_id:%s tid:%d answer:%p]\n\n", nRet, call_id, tid, answer);
	eXosip_unlock(m_pContext);
	return nRet;
}
int CSip::subscribe_notify(const char *body, int did, int status, int reason, const char* call_id /*=NULL*/)
{
	osip_message_t *notify = NULL;
	eXosip_lock(m_pContext);
	int nRet = eXosip_insubscription_build_notify(m_pContext, did, status, reason, &notify);
	if ((0 == nRet) && (NULL != notify))
	{
		if (NULL != call_id)
			set_call_id(notify, call_id);

		if (NULL != body) {
			nRet = osip_message_set_body(notify, body, strlen(body));
			if (0 != nRet)
				LOG("ERROR", "osip_message_set_body() failed ret:%d\n\n", nRet);
		}
		//nRet = osip_message_set_content_type(notify, "Application/MANSCDP+xml");
		//if (0 != nRet)
		//	LOG("ERROR", "osip_message_set_content_type() failed ret:%d\n\n", nRet);

		nRet = eXosip_insubscription_send_request(m_pContext, did, notify);
		if (0 != nRet)
			LOG("ERROR", "eXosip_insubscription_send_request failed %d\n\n", nRet);
		#ifdef USE_DEBUG_OUTPUT
		print_event(notify);
		#endif
		//osip_message_free(msg);
	}
	else LOG("ERROR", "eXosip_insubscription_build_notify failed %d [notify=%p]\n\n", nRet, notify);
	eXosip_unlock(m_pContext);
	return nRet;
}
int CSip::notify_response(cmd_e cmd_type, dev_info_t *dev, int did, int status, int reason, const char* call_id/*=NULL*/)
{
	int nRet = -1;
	stringstream xml;
	if (QUERY_DEV_CATALOG == cmd_type) {
		map<string, chl_info_t> chl_map;
		chl_map = dev->chl_map;
		stringstream xml;
		if (chl_map.size() > 0) {
			int i = 0, sum_num = chl_map.size();
			auto chl = chl_map.begin();
			int sn = m_sn++;
			for (; chl != chl_map.end();) {
				if (0 == i) {
					xml.clear();
					xml.str("");
					xml << "<?xml version=\"1.0\"?>\n";
					xml << "<Response>\n";
					xml << "  <CmdType>Catalog</CmdType>\n";
					xml << "  <SN>" << sn << "</SN>\n";
					xml << "  <DeviceID>" << dev->addr.id << "</DeviceID>\n";
					xml << "  <SumNum>" << chl_map.size() << "</SumNum>\n";
					xml << "  <DeviceList Num=\"" << (min(sum_num, ITEM_NUM)) << "\">\n";
				}
				if (i < ITEM_NUM) {
					xml << "    <Item>\n";
					xml << "      <DeviceID>" << chl->second.id << "</DeviceID>\n";
					xml << "      <Name>" << chl->second.name << "</Name>\n";
					xml << "      <Manufacturer>" << chl->second.manufacturer << "</Manufacturer>\n";
					xml << "      <Model>" << chl->second.model << "</Model>\n";
					xml << "      <Owner>" << chl->second.owner << "</Owner>\n";
					xml << "      <CivilCode>" << chl->second.civil_code << "</CivilCode>\n";
					xml << "      <Address>" << chl->second.address << "</Address>\n";
					xml << "      <Parental>" << (int)chl->second.parental << "</Parental>\n";		// 是否有子设备(必选) 1 有, 0 没有 
					xml << "      <ParentID>" << chl->second.parent_id << "</ParentID>\n";
					xml << "      <SafetyWay>" << (int)chl->second.safety_way << "</SafetyWay>\n";
					xml << "      <RegisterWay>" << (int)chl->second.register_way << "</RegisterWay>\n";
					xml << "      <Secrecy>" << (int)chl->second.secrecy << "</Secrecy>\n";
					xml << "      <Status>" << ((1 == chl->second.status) ? "ON" : "OFF") << "</Status>\n";
					xml << "      <IPAddress>" << chl->second.ip_address << "</IPAddress>\n";
					xml << "      <Port>" << chl->second.port << "</Port>\n";
					xml << "    </Item>\n";
					++i;
					++chl;
				}
				else {
					xml << "  </DeviceList>\n";
					xml << "</Response>\n";
					nRet = subscribe_notify(xml.str().c_str(), did, status, reason, call_id);
					sum_num -= ITEM_NUM;
					i = 0;
				}
			}
			if (0 != i) {
				xml << "  </DeviceList>\n";
				xml << "</Response>\n";
				nRet = subscribe_notify(xml.str().c_str(), did, status, reason, call_id);
			}
		}
		else {
			xml << "<?xml version=\"1.0\"?>\n";
			xml << "<Response>\n";
			xml << "  <CmdType>Catalog</CmdType>\n";
			xml << "  <SN>" << m_sn++ << "</SN>\n";
			xml << "  <DeviceID>" << dev->addr.id << "</DeviceID>\n";
			xml << "  <SumNum>" << 0 << "</SumNum>\n";
			xml << "  <DeviceList Num=\"" << 0 << "\">\n";
			xml << "  </DeviceList>\n";
			xml << "</Response>\n";
			nRet = subscribe_notify(xml.str().c_str(), did, status, reason, call_id);
		}
	}
	else LOG("WARN","Unknown event type %d\n\n", cmd_type);
	return nRet;
}
int CSip::call_invite(addr_info_t *from, addr_info_t *to, operation_t *opt, const char *dev_id/*=NULL*/)
{
    char f[128] = {0};
    char t[128] = {0};
    sprintf(f, "sip:%s@%s:%d", from->id, from->ip, from->port);
	if(NULL == dev_id)
		sprintf(t, "sip:%s@%s:%d", to->id, to->ip, to->port);
	else sprintf(t, "sip:%s@%s:%d", dev_id, to->ip, to->port);
	std::stringstream sdp;
	if(NULL == opt->body) {
		// "v=0\r\n"					// SDP版本
		// "o=- 1256698598 0 IN IP4 192.168.2.105\r\n"	// 用户名、会话id、版本、网络类型、地址类型、IP地址
		// "s=Play\r\n"					// 会话名称(Play:点播请求 Playback:回播请求 Download:文件下载 Talk:语音对讲 EmbeddedNetDVR:)
		// "c=IN IP4 192.168.2.105\r\n"	// 网络类型、地址类型、音视频流的目的地址
		// "t=0 0\r\n"					// 视频开始和结束时间(单位:秒)十进制NTP (t = 0 0 表示实时视音频点播)
		// "u=34020000001320000001:3\r\n"	// u字段：(注意:若存在,一定要比第一个media先出现)视音频文件的URL(仅回放或下载时有效)。该URL的取值有两种：简捷方式和普通方式。简捷方式直接采用产生该历史媒体的媒体源（如某个摄像头）的设备ID以及相关参数，参数用“：”分隔；普通方式采样http://储存设备ID[/文件夹]*/文件名
		// "m=video 5080 RTP/AVP 96 97 98\r\n"			// video:请求视频流、目的端口、传输使用协议、格式列表(96 97 98:客户端支持码流格式)
		// "a=rtpmap:96 PS/90000\r\n"
		// "a=rtpmap:97 MPEG4/90000\r\n"
		// "a=rtpmap:98 H264/90000\r\n"
		// "a=recvonly\r\n"
		// a=downloadspeed:下载倍速			// 文件下载时有效
		// a=filezise:文件大小（单位Byte）	// 文件下载时有效
		// a=setup:active					// tcp传输时有效。active表示发送者是客户端，passive表示发送者是服务端。这个字段是2016国标28181中新增加的。用来实现TCP协商制。关于2016版新国标中TCP协商制的描述
		// a=connection:new					// tcp传输时有效。
		// a=svcspace:空域编码方式			// SVC参数
		// a=svctime:时域编码方式			// SVC参数
		// "y=0999999999\r\n"		// 标识SSRC值,为十进制整数字符串(如:0999999999),其中第1位为历史或者实时媒体流的标识位(0:实时 1:历史);第2~6位:截取20位SIP监控域ID之中的第4~8位作为域标识;第7~10位:作为域内媒体流标识,是一个与当前域内产生的媒体流SSRC值后4位不充分的四位十进制整数
		// "f=\r\n",				// f字段:v/编码格式/分辨率/帧率/码率类型/码率大小, a/编码格式/码率大小/采样率 (其中v表示video a表示audio)
		// m=audio 8000 RTP/AVP 8	//标识语音媒体流内容
		// a=rtpmap:8 PCMA/8000		//RTP+音频流
		// a=sendonly
		// y=0100000001
		// f=v/////a/1/8/1		//音频参数描述
		std::stringstream ssrc;
		static long num = 999991000;
		sdp << "v=0\r\n";
		//sdp << "o=" << from->id << " 0 0 IN IP4 " << opt->media.ip << "\r\n";
		sdp << "o=" << "-" << " 0 0 IN IP4 " << opt->media.ip << "\r\n";
		//sdp << "o=" << "- " << time(NULL) << " 0 IN IP4 " << opt->media.ip << "\r\n";
		// 注意: 点播sdp字段的顺序为: v字段 o字段 s字段 c字段 t字段 颠倒可能会点播失败
		//       回播sdp字段的顺序为: v字段 o字段 s字段 u字段 c字段 t字段 颠倒可能会回播失败
		if(CTRL_PLAY_START == opt->cmd) {		// 点播 或 回播
			sdp << "s=Play\r\n";				// 点播
			sdp << "c=IN IP4 " << opt->media.ip << "\r\n";
			sdp << "t=0 0\r\n";
			ssrc << "0" << num++;
		}
		else if (CTRL_PLAYBAK_START == opt->cmd) {	// 回播
			if (NULL != opt->record) {
				sdp << "s=Playback\r\n";			// 回播
				sdp << "u=" << to->id << ":3\r\n";
				sdp << "c=IN IP4 " << opt->media.ip << "\r\n";
				sdp << "t=" << string_to_timestamp(opt->record->start_time) << " " << string_to_timestamp(opt->record->end_time) << "\r\n";
				ssrc << "1" << num++;
			}
			else LOG("ERROR","opt->record=%p\n\n", opt->record);
		}
		else if (CTRL_DOWNLOAD_START == opt->cmd) {	// 文件下载
			sdp << "s=Download\r\n";
			sdp << "c=IN IP4 " << opt->media.ip << "\r\n";
		}
		else if(CTRL_TALK_START == opt->cmd)		// 语音对讲
			sdp << "s=Talk\r\n";
		
		sdp << "m=video " << opt->media.port << " RTP/AVP 96 98 97\r\n";
		sdp << "a=recvonly\r\n";
		sdp << "a=rtpmap:96 PS/90000\r\n";
		sdp << "a=rtpmap:98 H264/90000\r\n";
		sdp << "a=rtpmap:97 MPEG4/90000\r\n";
		sdp << "y=" << ssrc.str() << "\r\n";
	#if 0
		sdp << "m=audio " << opt->media.port  << " RTP/AVP 8\r\n";
		sdp << "a=rtpmap:8 AAC/8000\r\n";
		sdp << "a=recvonly\r\n";
		//sdp << "y=" << ssrc.str() << "\r\n";
	#endif
		sdp << "f=\r\n";
		//sdp << "f = v/////0a/1/8/1\r\n";
		//sdp << "f = v/////a/AAC/32768/16384\r\n";
		//sdp << "f=v/0/0/0/0/0a/AAC/0/0\r\n";
		//sdp << "f=v/0/0/0/0/0a/1/8/1\r\n";
		//printf("%s\n\n", sdp.str().c_str());
	}
	else {
		sdp << opt->body;
	}
	osip_message_t *invite = NULL;
	eXosip_lock(m_pContext);
	int nRet = eXosip_call_build_initial_invite(m_pContext, &invite, t, f, NULL, NULL);
	if((0 == nRet) && (NULL != invite))
	{
		if(NULL != opt->call_id) {
			nRet = set_call_id(invite, opt->call_id);
			if(0 != nRet)
				LOG("ERROR","set_call_id('%s') failed %d\n\n", opt->call_id, nRet);
		}
		//if(NULL != opt->username)
		//	invite->req_uri->username = (char*)opt->username;	// 待点播的设备ID
		// 原有的cseq固定为20，此处修改为自定义
		//osip_free(invite->cseq->number);
		//invite->cseq->number = osip_strdup("1");
		// 原有的Call-ID长度为33，修改为自定义Call-ID
		//osip_free(invite->call_id->number);
		//invite->call_id->number = osip_strdup("1111111111111111111");
		//if (NULL != invite->cseq) {
		//	static int num = 1;
		//	char number[24] = { 0 };
		//	sprintf(number, "%d", num++);
		//	osip_cseq_set_number(invite->cseq, number);
		//}

		#if 0
		if(osip_list_size(&invite->contacts) > 0)
			osip_list_ofchar_free(&invite->contacts);
		//osip_contact_t *contact = NULL;
		//osip_message_get_contact(invite, 0, &contact);
		char contact[128] = {0};
		sprintf(contact, "<sip:%s@%s:%d>", from->id, from->ip, from->port);
		osip_message_set_contact(invite, osip_strdup(contact));
		#endif
		//eXosip_masquerade_contact(m_pContext, m_pfm.addr.ip, m_pfm.addr.port);

		#if 0	// 释放旧的via,设置新的via
		if(osip_list_size(&invite->vias) > 0)
			osip_list_ofchar_free(&invite->vias);
		char via[128] = {0};
		snprintf(via, 128, "SIP/2.0/UDP %s:%d;rport=%d;branch=z9hG4bK%u",from->id, from->ip, from->port, osip_build_random_number());
		nRet |= osip_message_set_via(invite, via);
		//LOG("INFO","via size:%d, ret:%d\n\n", osip_list_size(&msg->vias), nRet);
		#endif

		#if 0
			#if 1
			struct timeval	tv;
			struct timezone	tz;
			gettimeofday(&tv, &tz);
			struct tm *pt = localtime(&tv.tv_sec);
			char szTime[32] = {0};
			sprintf(szTime,"%d-%02d-%02dT%02d:%02d:%02d.%03d", 1900+pt->tm_year, 1+pt->tm_mon, pt->tm_mday, pt->tm_hour, pt->tm_min, pt->tm_sec, tv.tv_usec/1000);
			osip_message_set_topheader(invite, "Date", szTime);
			#endif
			if((NULL != opt->rport) || (NULL != opt->branch)) {
				std::stringstream via;
				//SIP/2.0/UDP 192.168.2.162:5090;rport=5090;branch=z9hG4bK224523557
				via << "SIP/2.0/UDP " << "192.168.2.205:5060;";
				if(NULL != opt->rport)
					via << "rport=" << opt->rport << ";";
				if(NULL != opt->branch)
					via << "branch=" << opt->branch << ";";
				osip_message_set_via(invite, via.str().c_str());
			}
		#endif
		nRet = osip_message_set_body(invite, sdp.str().c_str(), sdp.str().length());
		if (0 != nRet)
			LOG("ERROR","osip_message_set_body failed %d\n\n", nRet);
		nRet = osip_message_set_content_type(invite, "application/sdp");
		if (0 != nRet)
			LOG("ERROR","osip_message_set_content_type failed %d\n\n", nRet);
		int nCallId = eXosip_call_send_initial_invite(m_pContext, invite);
		if (nCallId > 0) {	// 返回值为呼叫标识(call identifier),该标识可以用于发送CANCEL请求等
			//eXosip_call_set_reference(m_pContext, nCallId, reference);
			print_event(invite);
		}
		else {
			LOG("ERROR","eXosip_call_send_initial_invite failed ['%s:%s' ret:%d]\n\n", to->id, to->ip, nCallId);
			nRet = -1;
		}
		//osip_message_free(invite);
	}
	else {
		LOG("ERROR","eXosip_call_build_initial_invite failed '%s:%s' %d\n", to->id, to->ip, nRet);
		nRet = -1;
	}
	eXosip_unlock(m_pContext);
	//osip_message_free(invite);
	return nRet;
}

int CSip::call_ack(int did, const char* call_id/*=NULL*/)
{
	osip_message_t *ack = NULL;
	eXosip_lock(m_pContext);
	int nRet = eXosip_call_build_ack(m_pContext, did, &ack);
	if(NULL != ack)
	{
		if(NULL != call_id)
			nRet |= set_call_id(ack, call_id);

		nRet |= eXosip_call_send_ack(m_pContext, did, ack);
		#ifdef USE_DEBUG_OUTPUT
		print_event(ack);
		#endif
		//osip_message_free(ack);
	}
	eXosip_unlock(m_pContext);
	return nRet;
}

int CSip::get_remote_sdp(int did, string &sdp_str, sdp_info_t *sdp)
{
	int nRet = -1;
	sdp_message_t *remote_sdp = eXosip_get_remote_sdp(m_pContext, did);
	if (NULL != remote_sdp) 
	{
		char *p_sdp = NULL;
    	int ret = sdp_message_to_str(remote_sdp, &p_sdp);
    	if (NULL != p_sdp) {
			sdp_str = string(p_sdp);
			//sdp += "y=0000001065\n";
			//sdp += "f=\n";
			//LOG("INFO","SDP: %s\n\n", sdp.c_str());
			osip_free(p_sdp);
		}else LOG("ERROR","sdp_message_to_str failed, error code %d\n", ret);

		//const char *media = NULL;
		//sdp_connection_t *conn = eXosip_get_connection(remote_sdp, media);
		//if (NULL != conn) {
		//	DEBUG("     net_type: %s\n", conn->c_nettype);
		//	DEBUG(" address_type: %s\n", conn->c_addrtype);
		//	DEBUG("     video_ip: %s\n", conn->c_addr);
		//	DEBUG("    multicast: %s\n", conn->c_addr_multicast_int);
		//	DEBUG("multicast_ttl: %s\n", conn->c_addr_multicast_ttl);
		//	if (NULL != media)
		//		DEBUG("[%s]\n", media);
		//}

		#if 1
		if (NULL != sdp) {
			// v
			char *v = sdp_message_v_version_get(remote_sdp);
			if(NULL != v)
				strncpy(sdp->v, v, sizeof(sdp->v) - 1);
			
			// o
			if (NULL != remote_sdp->o_username)
				strncpy(sdp->o_username, remote_sdp->o_username, sizeof(sdp->o_username) - 1);
			
			if (NULL != remote_sdp->o_sess_id)
				strncpy(sdp->o_sess_id, remote_sdp->o_sess_id, sizeof(sdp->o_sess_id) - 1);
			
			if (NULL != remote_sdp->o_sess_version)
				strncpy(sdp->o_sess_version, remote_sdp->o_sess_version, sizeof(sdp->o_sess_version) - 1);
			if (NULL != remote_sdp->o_nettype)
				strncpy(sdp->o_nettype, remote_sdp->o_nettype, sizeof(sdp->o_nettype) - 1);
			if (NULL != remote_sdp->o_addrtype)
				strncpy(sdp->o_addrtype, remote_sdp->o_addrtype, sizeof(sdp->o_addrtype) - 1);
			if (NULL != remote_sdp->o_addr)
				strncpy(sdp->o_addr, remote_sdp->o_addr, sizeof(sdp->o_addr) - 1);
			// s
			if (NULL != remote_sdp->s_name)
				strncpy(sdp->s, remote_sdp->s_name, sizeof(sdp->s) - 1);

			// t
			for (int i = 0; i < remote_sdp->t_descrs.nb_elt; i++) {
				sdp_time_descr_t *sdp_time = (sdp_time_descr_t*)osip_list_get(&remote_sdp->t_descrs, i);
				if (NULL != sdp_time) {
					if(NULL != sdp_time->t_start_time)
						sdp->t_start_time = atoll(sdp_time->t_start_time);
					if (NULL != sdp_time->t_stop_time)
						sdp->t_end_time = atoll(sdp_time->t_stop_time);
				}
			}
			//////////////// 视频信息 //////////////////////////
			// m & c & a
			sdp_media_t *video_media = eXosip_get_video_media(remote_sdp);
			if (NULL != video_media) {
				sdp_info_t::m_media_t m;
				sdp_connection_t *video_conn = eXosip_get_video_connection(remote_sdp);
				if (NULL != video_conn) {
					if(NULL != video_conn->c_nettype)
						strncpy(m.c.nettype, video_conn->c_nettype, sizeof(m.c.nettype) - 1);
					if(NULL != video_conn->c_addrtype)
						strncpy(m.c.addrtype, video_conn->c_addrtype, sizeof(m.c.addrtype) - 1);
					if(NULL != video_conn->c_addr)
						strncpy(m.c.addr, video_conn->c_addr, sizeof(m.c.addr) - 1);
					if(NULL != video_conn->c_addr_multicast_ttl)
						strncpy(m.c.addr_multicast_ttl, video_conn->c_addr_multicast_ttl, sizeof(m.c.addr_multicast_ttl) - 1);
					if(NULL != video_conn->c_addr_multicast_int)
						strncpy(m.c.addr_multicast_int, video_conn->c_addr_multicast_int, sizeof(m.c.addr_multicast_int) - 1);
				}
				if(NULL != video_media->m_media)
					strncpy(m.media, video_media->m_media, sizeof(m.media) - 1);
				if(NULL != video_media->m_port)
					m.port = atoi(video_media->m_port);
				if(NULL != video_media->m_proto)
					strncpy(m.proto, video_media->m_proto, sizeof(m.proto) - 1);
				if(NULL != video_media->m_number_of_port)
					strncpy(m.number_of_port, video_media->m_number_of_port, sizeof(m.number_of_port) - 1);

				for (int i = 0; i < video_media->m_payloads.nb_elt; i++) {
					char *payloads = (char*)osip_list_get(&video_media->m_payloads, i);
					if (NULL != payloads)
						m.payloads.push_back(atoi(payloads));
				}

				for (int i = 0; i < video_media->a_attributes.nb_elt; i++) {
					sdp_attribute_t *a = (sdp_attribute_t*)osip_list_get(&video_media->a_attributes, i);
					if ((NULL != a) && (NULL != a->a_att_field)) {
						sdp_info_t::m_media_t::a_attribute_t attr;
						strncpy(attr.field, a->a_att_field, sizeof(attr.field)-1);
						if(NULL != a->a_att_value)
							strncpy(attr.value, a->a_att_value, sizeof(attr.value) - 1);
						m.a.push_back(attr);
					}
				}
				sdp->m.insert(map<string, sdp_info_t::m_media_t>::value_type(m.media, m));
				nRet = 0;
			}
			else LOG("ERROR","eXosip_get_video_media failed (did=%d).\n", did);
		}	
		#endif

		//////////////// 音频信息 //////////////////////////
		//  (c= 表示connector)
		#if 0
		sdp_media_t * audio_sdp = eXosip_get_audio_media(remote_sdp);
		if (NULL != audio_sdp) 
		{
			sdp_connection_t *audio_conn = eXosip_get_audio_connection(remote_sdp);
			if (NULL != audio_conn ) {
				LOG("INFO","audio_ip: %s\n", audio_conn->c_addr);
				LOG("INFO", "multicast: %s\n", audio_conn->c_addr_multicast_int);
				LOG("INFO", "multicast_ttl: %s\n", audio_conn->c_addr_multicast_ttl);
				LOG("INFO", "address_type: %s\n", audio_conn->c_addrtype);
				LOG("INFO", "net_type: %s\n", audio_conn->c_nettype);
			}
			LOG("INFO", "audio_port: %s\n", audio_sdp->m_port);
			LOG("INFO", "i_info: %s\n", audio_sdp->i_info);
			LOG("INFO", "m_proto: %s\n", audio_sdp->m_proto);
			LOG("INFO", "m_number_of_port: %s\n", audio_sdp->m_number_of_port);
			LOG("INFO", "m_media: %s\n", audio_sdp->m_media);
			for (int i=0; i<audio_sdp->a_attributes.nb_elt; i++) 
			{
				sdp_attribute_t *attr = (sdp_attribute_t*)osip_list_get(&audio_sdp->a_attributes, i);
				if(NULL != attr) {
					if (0 == strcmp(attr->a_att_field, "rtpmap")) {
						string g726_value = attr->a_att_value;
						if ( ! strstr(g726_value.c_str(),"726"))
							continue;

						string::size_type pt_idx = g726_value.find_first_of(0x20);
						if ( pt_idx == string::npos )
							break;

						string rtp_pt_type = g726_value.substr(0,pt_idx);
						string::size_type bitrate_idx = g726_value.find_first_of('/');

						if (bitrate_idx == string::npos )
							break;

						string bitrate = g726_value.substr( bitrate_idx+ 1 );
						string encode_type = g726_value.substr(pt_idx+1,bitrate_idx - pt_idx-1);
					}
				}
			}
			nRet = 0;
		}
		#endif
		//sdp_message_free(remote_sdp);
	}
	else LOG("ERROR","eXosip_get_remote_sdp failed (did=%d).\n", did);
	return nRet;	
}

int CSip::update_remote_sdp(int did, string &sdp_str, const char *media_ip, int media_port)
{
	int nRet = -1;
	sdp_message_t *remote_sdp = eXosip_get_remote_sdp(m_pContext, did);
	if (NULL != remote_sdp)
	{
		char *p_sdp = NULL;
		int ret = sdp_message_to_str(remote_sdp, &p_sdp);
		if (NULL != p_sdp) {
			sdp_str = string(p_sdp);
			//sdp += "y=0000001065\n";
			//sdp += "f=\n";
			//LOG("INFO","SDP: %s\n\n", sdp.c_str());
			osip_free(p_sdp);
		}
		else LOG("ERROR","sdp_message_to_str failed, error code %d\n", ret);
		//sdp_message_free(remote_sdp);
	}
	else LOG("ERROR","eXosip_get_remote_sdp failed (did=%d).\n", did);
	return nRet;
}

int CSip::make_sdp(const sdp_info_t *sdp, string &sdp_str)
{
	int nRet = -1;
	if (NULL == sdp)
		return nRet;
#if 0
	sdp_message_t *osip_sdp = NULL;
	nRet = sdp_message_init(&osip_sdp);
	if ((0 == nRet) && (NULL != osip_sdp))
	{
		// v
		sdp_message_v_version_set(osip_sdp, osip_strdup(sdp->v));
		// o
		osip_sdp->o_username = osip_strdup(sdp->o_username);
		osip_sdp->o_sess_id = osip_strdup(sdp->o_sess_id);
		osip_sdp->o_sess_version = osip_strdup(sdp->o_sess_version);
		osip_sdp->o_nettype = osip_strdup(sdp->o_nettype);
		osip_sdp->o_addrtype = osip_strdup(sdp->o_addrtype);
		osip_sdp->o_addr = osip_strdup(sdp->o_addr);
		// s
		sdp_message_s_name_set(osip_sdp, osip_strdup(sdp->s_name));
		
		//c - video
		auto c_v = sdp->m.find("video");
		if(c_v != sdp->m.end())
			sdp_message_c_connection_add(osip_sdp, -1, osip_strdup(c_v->second.c.nettype), osip_strdup(c_v->second.c.addrtype), osip_strdup(c_v->second.c.addr), osip_strdup(c_v->second.c.addr_multicast_ttl), osip_strdup(c_v->second.c.addr_multicast_int));

		// t
		char t_start[24] = { 0 };
		char t_end[24] = { 0 };
		sprintf(t_start, "%lld", sdp->t_start_time);
		sprintf(t_end, "%lld", sdp->t_end_time);
		sdp_message_t_time_descr_add(osip_sdp, osip_strdup(t_start), osip_strdup(t_end));

		// m
		//int sdp_message_m_media_add(sdp_message_t * sdp, char *media,char *port, char *number_of_port, char *proto);
		//char *sdp_message_m_media_get(sdp_message_t * sdp, int pos_media);
		//char *sdp_message_m_port_get(sdp_message_t * sdp, int pos_media);
		//int sdp_message_m_port_set(sdp_message_t * sdp, int pos_media, char *port);
		//char *sdp_message_m_number_of_port_get(sdp_message_t * sdp, int pos_media);
		//char *sdp_message_m_proto_get(sdp_message_t * sdp, int pos_media);
		//int sdp_message_m_payload_add(sdp_message_t * sdp, int pos_media,char *payload);
		//char *sdp_message_m_payload_get(sdp_message_t * sdp, int pos_media, int pos);
		//int sdp_message_m_payload_del(sdp_message_t * sdp, int pos_media, int pos);
		if (c_v != sdp->m.end()) {
			const sdp_info_t::m_media_t *v = &c_v->second;
			for (int i = 0; i < v->a.size(); i++) {
				if(strlen(v->a[i].value) > 0)
					sdp_message_a_attribute_add(osip_sdp, -1, osip_strdup(v->a[i].field), osip_strdup(v->a[i].value));
				else sdp_message_a_attribute_add(osip_sdp, -1, osip_strdup(v->a[i].field), NULL);
			}
			char v_port[8] = { 0 };
			sprintf(v_port, "%d", v->port);
			std::stringstream number_of_port;
			for (int i = 0; i < v->payloads.size(); i++)
				number_of_port << v->payloads[i] << " ";
			sdp_message_m_media_add(osip_sdp, osip_strdup(v->media), osip_strdup(v_port), osip_strdup(number_of_port.str().c_str()), osip_strdup(v->proto));
		}
		char *p_sdp = NULL;
		int ret = sdp_message_to_str(osip_sdp, &p_sdp);
		if (NULL != p_sdp)
		{
			sdp_str = p_sdp;
			osip_free(p_sdp);
		}
		sdp_message_free(osip_sdp);
	}
#else
	std::stringstream str;
	str << "v=0\r\n";
	str << "o=" << sdp->o_username << " " << sdp->o_sess_id << " " << sdp->o_sess_version << " " << sdp->o_nettype << " " << sdp->o_addrtype << " " << sdp->o_addr  << "\r\n";
	str << "s=" << sdp->s << "\r\n";
	auto m_iter = sdp->m.find("video");
	if (m_iter != sdp->m.end()) {
		const sdp_info_t::m_media_t *m = &m_iter->second;
		str << "c=" << m->c.nettype << " " << m->c.addrtype << " " << m->c.addr << "\r\n";
		str << "t=" << sdp->t_start_time << " " << sdp->t_end_time << "\r\n";
		str << "m=video " << m->port << " RTP/AVP 96\r\n";
		str << "a=sendonly\r\n";
		str << "a=rtpmap:96 PS/90000\r\n";
		str << "a=filesize:0\r\n";
	}
	sdp_str = str.str();
#endif
	return nRet;
}

int CSip::call_answer(int tid, int status_code, const char *sdp, const char *call_id)
{
	osip_message_t *answer = NULL;
	eXosip_lock(m_pContext);
	int nRet = eXosip_call_build_answer(m_pContext, tid, status_code, &answer);
	if((0 == nRet) && (NULL != answer))
	{
		if(NULL != call_id)
			nRet = set_call_id(answer, call_id);
		#if 0
		if(NULL != contact)
		{
			if(osip_list_size(&answer->contacts) > 0)
				osip_list_ofchar_free(&answer->contacts);
			osip_message_set_contact(answer, osip_strdup(contact));
		}
		#endif
		if(NULL != sdp)
		{
			osip_message_set_body(answer, sdp, strlen(sdp));
			nRet |= osip_message_set_content_type(answer, "application/sdp");
		}
		nRet = eXosip_call_send_answer(m_pContext, tid, status_code, answer);
		if(0 != nRet)
			LOG("ERROR","Call send answer failed [ret:%d tid:%d]\n\n\n\n\n\n", nRet, tid);
		#ifdef USE_DEBUG_OUTPUT
		print_event(answer);
		#endif
		//osip_message_free(answer);
	}
	else LOG("ERROR","[ret:%d answer:%p call_id:%s tid:%d] \n\n", nRet, answer, (NULL!=call_id)?call_id:"null", tid);
	eXosip_unlock(m_pContext);
	return nRet;	
}

int CSip::call_terminate(int cid, int did)
{
	if(NULL!= m_pContext)
	{
		eXosip_lock(m_pContext);
		int nRet = eXosip_call_terminate(m_pContext, cid, did);
		if(0 != nRet)
			LOG("ERROR","Call terminate failed [ret:%d cid:%d did:%d]\n\n", nRet, cid, did);
		eXosip_unlock(m_pContext);
		return nRet;
	}
	return -1;
}

unsigned int CSip::generate_random_number()
{
	return osip_build_random_number();
}





////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#ifdef USE_TEST
void CSip::print_event(osip_message_t *sip)
{
	if(NULL != sip)
	{
        char *dest = NULL;
        size_t message_length = 0;
        int nRet = osip_message_to_str(sip, &dest, &message_length);
        if(NULL != dest)
        {
			//LOG("INFO","-------\n");
			LOG("INFO", "\n\n%s\n\n", dest);
			//LOG("INFO", "-------\n");
            osip_free(dest);
        }
	}
    else LOG("INFO","sip_message=%p\n\n", sip);
}
#endif