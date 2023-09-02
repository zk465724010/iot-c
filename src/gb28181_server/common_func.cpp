
#include "common_func.h"
#include <stdio.h>
#include "log.h"

#ifdef _WIN32
#include <Windows.h>
#elif __linux__
#include <iconv.h>		// iconv_open(),iconv(),iconv_close()
#endif


// 函数功能：多字节 -> UTF8	
// 函数参数：①pData[in]: 待转换的多字节串
//			 ②nLen[in]:  待转换串的长度
//			 ③pBuf[out]: 接收UTF-8字符串的缓冲区
// 函数返回：转换成功则返回UTF-8字符串的长度,否则返回0
int ansi_to_utf8(char* pData, int nLen, char* pBuf, int nBufSize)
{
#ifdef _WIN32
	int nRet = -1;
	//步骤: 先将ANSI串转换为UNICODE串,然后再将UNICODE串转换为UTF-8串
	int iLen = MultiByteToWideChar(CP_ACP, 0, pData, nLen, NULL, 0);	//得到Unicode串的缓冲区所必需的宽字符数
	if (iLen > 0) {
		//第一步: Ansi转换为Unicode
		wchar_t* pwBuf = new wchar_t[iLen + 1];
		iLen = MultiByteToWideChar(CP_ACP, 0, pData, nLen, (LPWSTR)pwBuf, iLen);
		pwBuf[iLen] = 0;

		//第二步: Unicode转换为UTF-8
		int nSize = WideCharToMultiByte(CP_UTF8, 0, pwBuf, iLen, NULL, 0, NULL, NULL);//得到UTF-8串的缓冲区所必需的字符数
		if (nSize > 0) {
			nRet = WideCharToMultiByte(CP_UTF8, 0, pwBuf, iLen, pBuf, nSize, NULL, NULL);
			pBuf[nSize] = 0;
		}
		delete pwBuf;
	}
#elif __linux__
	int nRet = -1;
	if ((NULL != pData) && (nLen > 0) && (NULL != pBuf)) {
		// #include <iconv.h>
		// iconv_t iconv_open(const char *tocode, const char *fromcode);
		// size_t iconv(iconv_t cd,char **inbuf,size_t *inbytesleft,char **outbuf,size_t *outbytesleft);
		// int iconv_close(iconv_t cd);
		// TRANSLIT：遇到无法转换的字符就找相近字符替换
		// IGNORE：遇到无法转换字符跳过
		//
		// iconv()函数详解
		//作为入参
		//	第一个参数cd是转换句柄。
		//	第二个参数inbuf是输入字符串的地址的地址。
		//	第三个参数inbytesleft是输入字符串的长度。
		//	第四个参数outbuf是输出缓冲区的首地址
		//	第五个参数outbytesLeft 是输出缓冲区的长度。
		//作为出参
		//	第一个参数cd是转换句柄。
		//	第二个参数inbuf指向剩余字符串的地址
		//	第三个参数inbytesleft是剩余字符串的长度。
		//	第四个参数outbuf是输出缓冲区剩余空间的首地址
		//	第五个参数outbytesLeft 是输出缓冲区剩余空间的长度。
		//返回值
		//	函数执行后,由于各种原因(输出缓冲区太小,输入字符串不是一种编码,而是含有多种编码)，
		//	可能只转换了部分编码。这时返回值为-1,这时会改变后四个参数的值。另外,即使全部转换成功，
		//	这四个参数的值也要发生变化。
		const char* fromcode = "GB2312//IGNORE"; // "GB2312","GBK","UNICODE//IGNORE","UTF-8","GB2312//IGNORE"
		const char* tocode = "UTF-8";
		iconv_t cd = iconv_open(tocode, fromcode);
		if ((iconv_t)-1 != cd) {
			size_t data_size = nLen;
			size_t buff_size = nBufSize;
			char** src = &pData;
			char** dst = &pBuf;
			nRet = iconv(cd, src, &data_size, dst, &buff_size);
			LOG("INFO", "----- iconv()=%d\n\n", nRet);
			nRet = nBufSize - buff_size;
			pBuf[nRet] = 0;
			iconv_close(cd);
		}
		else LOG("ERROR", "iconv_open failed %d\n\n", cd);
	}
#endif
	return nRet;
}
int utf8_to_ansi(char* pData, int nLen, char* pBuf, int nBufSize)
{
	if ((NULL == pData) || (nLen <= 0) || (NULL == pBuf)) {
		LOG("ERROR", "pData=%p, nLen=%d, pBuf=%p\n\n", pData, nLen, pBuf);
		return -1;
	}
	int iLen = 0;
#ifdef _WIN32
	//步骤: 先将UTF-8串转换为UNICODE串,然后再将UNICODE串转换为ANSI串
	iLen = MultiByteToWideChar(CP_UTF8, 0, pData, nLen, NULL, 0);	//得到Unicode串的缓冲区所必需的宽字符数
	if (iLen > 0) {
		//第一步: UTF-8转换为Unicode
		wchar_t* pwBuf = new wchar_t[iLen + 4];
		iLen = MultiByteToWideChar(CP_UTF8, 0, pData, nLen, (LPWSTR)pwBuf, iLen);
		pwBuf[iLen] = 0;

		//第二步: Unicode转换为Ansi
		int	nRet = WideCharToMultiByte(CP_ACP, 0, pwBuf, iLen, NULL, 0, NULL, NULL);//得到Ansi串的缓冲区所必需的字符数
		if (nRet > 0) {
			iLen = WideCharToMultiByte(CP_ACP, 0, pwBuf, iLen, pBuf, nRet, NULL, NULL);
		}
		delete pwBuf;
	}
#elif __linux__
	int nRet = -1;
	const char* fromcode = "UTF-8//IGNORE";
	const char* tocode = "GB2312"; // "GB2312","GBK","UNICODE//IGNORE","UTF-8","GB2312//IGNORE"
	iconv_t cd = iconv_open(tocode, fromcode);
	if ((iconv_t)-1 != cd) {
		size_t data_size = nLen;
		size_t buff_size = nBufSize;
		char** src = &pData;
		char** dst = &pBuf;
		nRet = iconv(cd, src, &data_size, dst, &buff_size);
		LOG("INFO", "----- iconv()=%d\n\n", nRet);
		nRet = nBufSize - buff_size;
		iLen = nRet;
		pBuf[nRet] = 0;
		iconv_close(cd);
	}
	else LOG("ERROR", "iconv_open failed %d\n\n", cd);
#endif
	return iLen;
}