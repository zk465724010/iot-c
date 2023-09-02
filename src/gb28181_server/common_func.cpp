
#include "common_func.h"
#include <stdio.h>
#include "log.h"

#ifdef _WIN32
#include <Windows.h>
#elif __linux__
#include <iconv.h>		// iconv_open(),iconv(),iconv_close()
#endif


// �������ܣ����ֽ� -> UTF8	
// ������������pData[in]: ��ת���Ķ��ֽڴ�
//			 ��nLen[in]:  ��ת�����ĳ���
//			 ��pBuf[out]: ����UTF-8�ַ����Ļ�����
// �������أ�ת���ɹ��򷵻�UTF-8�ַ����ĳ���,���򷵻�0
int ansi_to_utf8(char* pData, int nLen, char* pBuf, int nBufSize)
{
#ifdef _WIN32
	int nRet = -1;
	//����: �Ƚ�ANSI��ת��ΪUNICODE��,Ȼ���ٽ�UNICODE��ת��ΪUTF-8��
	int iLen = MultiByteToWideChar(CP_ACP, 0, pData, nLen, NULL, 0);	//�õ�Unicode���Ļ�����������Ŀ��ַ���
	if (iLen > 0) {
		//��һ��: Ansiת��ΪUnicode
		wchar_t* pwBuf = new wchar_t[iLen + 1];
		iLen = MultiByteToWideChar(CP_ACP, 0, pData, nLen, (LPWSTR)pwBuf, iLen);
		pwBuf[iLen] = 0;

		//�ڶ���: Unicodeת��ΪUTF-8
		int nSize = WideCharToMultiByte(CP_UTF8, 0, pwBuf, iLen, NULL, 0, NULL, NULL);//�õ�UTF-8���Ļ�������������ַ���
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
		// TRANSLIT�������޷�ת�����ַ���������ַ��滻
		// IGNORE�������޷�ת���ַ�����
		//
		// iconv()�������
		//��Ϊ���
		//	��һ������cd��ת�������
		//	�ڶ�������inbuf�������ַ����ĵ�ַ�ĵ�ַ��
		//	����������inbytesleft�������ַ����ĳ��ȡ�
		//	���ĸ�����outbuf��������������׵�ַ
		//	���������outbytesLeft ������������ĳ��ȡ�
		//��Ϊ����
		//	��һ������cd��ת�������
		//	�ڶ�������inbufָ��ʣ���ַ����ĵ�ַ
		//	����������inbytesleft��ʣ���ַ����ĳ��ȡ�
		//	���ĸ�����outbuf�����������ʣ��ռ���׵�ַ
		//	���������outbytesLeft �����������ʣ��ռ�ĳ��ȡ�
		//����ֵ
		//	����ִ�к�,���ڸ���ԭ��(���������̫С,�����ַ�������һ�ֱ���,���Ǻ��ж��ֱ���)��
		//	����ֻת���˲��ֱ��롣��ʱ����ֵΪ-1,��ʱ��ı���ĸ�������ֵ������,��ʹȫ��ת���ɹ���
		//	���ĸ�������ֵҲҪ�����仯��
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
	//����: �Ƚ�UTF-8��ת��ΪUNICODE��,Ȼ���ٽ�UNICODE��ת��ΪANSI��
	iLen = MultiByteToWideChar(CP_UTF8, 0, pData, nLen, NULL, 0);	//�õ�Unicode���Ļ�����������Ŀ��ַ���
	if (iLen > 0) {
		//��һ��: UTF-8ת��ΪUnicode
		wchar_t* pwBuf = new wchar_t[iLen + 4];
		iLen = MultiByteToWideChar(CP_UTF8, 0, pData, nLen, (LPWSTR)pwBuf, iLen);
		pwBuf[iLen] = 0;

		//�ڶ���: Unicodeת��ΪAnsi
		int	nRet = WideCharToMultiByte(CP_ACP, 0, pwBuf, iLen, NULL, 0, NULL, NULL);//�õ�Ansi���Ļ�������������ַ���
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