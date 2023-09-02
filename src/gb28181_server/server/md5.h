
/*
		Message Digest Algorithm MD5��������Ϊ��ϢժҪ�㷨����棩Ϊ�������ȫ����㷺ʹ�õ�һ��ɢ�к�����
	�����ṩ��Ϣ�������Ա��������㷨���ļ���ΪRFC 1321��R.Rivest,MIT Laboratory for Computer Science and 
	RSA Data Security Inc. April 1992����

		MD5��Message-Digest Algorithm 5����Ϣ-ժҪ�㷨5��������ȷ����Ϣ��������һ�¡��Ǽ�����㷺ʹ�õ��Ӵ��㷨֮һ
	������ժҪ�㷨����ϣ�㷨����������������ձ�����MD5ʵ�֡������ݣ��纺�֣�����Ϊ��һ�̶�����ֵ�����Ӵ��㷨��
	����ԭ��MD5��ǰ����MD2��MD3��MD4��
	
	MD5�㷨���������ص㣺
		1��ѹ���ԣ����ⳤ�ȵ����ݣ������MD5ֵ���ȶ��ǹ̶��ġ�
		2�����׼��㣺��ԭ���ݼ����MD5ֵ�����ס�
		3�����޸��ԣ���ԭ���ݽ����κθĶ�������ֻ�޸�1���ֽڣ����õ���MD5ֵ���кܴ�����
		4��ǿ����ײ����֪ԭ���ݺ���MD5ֵ�����ҵ�һ��������ͬMD5ֵ�����ݣ���α�����ݣ��Ƿǳ����ѵġ�
		
		MD5���������ô�������Ϣ��������ǩ�����ǩ��˽����Կǰ��"ѹ��"��һ�ֱ��ܵĸ�ʽ�����ǰ�һ�����ⳤ�ȵ��ֽڴ��任
	��һ������ʮ���������ִ���������MD5���⣬���бȽ������Ļ���sha-1��RIPEMD�Լ�Haval�ȡ�
*/

#ifndef __MD5_H__
#define __MD5_H__

#include <string>
#include <fstream>

// Type define
typedef unsigned char byte;
typedef unsigned int uint32;

using std::string;
using std::ifstream;

// MD5 declaration.
class MD5 
{
public:
	MD5();
	MD5(const void* input, size_t length);
	MD5(const string& str);
	MD5(ifstream& in);
	void update(const void* input, size_t length);	//ʹ�����뻺��������������
	void update(const string& str);					//���ַ�������������
	void update(ifstream& in);						//���ļ�����������
	const byte* digest();							//������ϢժҪ
	string toString();								//������ϢժҪ��ʮ�������ַ���
	void reset();									//���¼���״̬

private:
	void update(const byte* input, size_t length);
	void final();
	void transform(const byte block[64]);
	void encode(const uint32* input, byte* output, size_t length);
	void decode(const byte* input, uint32* output, size_t length);
	string bytesToHexString(const byte* input, size_t length);//����������ת��Ϊʮ�������ַ���

	//class uncopyable
	MD5(const MD5&);
	MD5& operator=(const MD5&);

private:
	uint32	m_uState[4];				//state (ABCD)
	uint32	m_uCount[2];				//number of bits, modulo 2^64 (low-order word first)
	byte	m_buffer[64];				//���뻺��
	byte	m_digest[16];				//��ϢժҪ,MD5ֵ
	bool	m_bFinished;				//�����������?

	static const byte PADDING[64];		//padding for calculate
	static const char HEX[16];
	enum { BUFFER_SIZE = 1024 };
};

#endif

/*
����ʾ��:

#include <stdio.h>
#include "md5.h"

int main()
{
	MD5 md5;

	char szData[] = "123456";

	md5.reset();
	md5.update(szData);				//���ַ�������������
	
	const byte *p = md5.digest();	//������ϢժҪ,��:MD5ֵ ����16�ֽ� (���ܺ������)

	string str = md5.toString();	//����ϢժҪ(MD5ֵ)ת��Ϊ�ַ���(�������ݵ�ʮ�������ַ�����ʽ)	
	printf("����:%s\n",str.c_str());

	return 0;
}

*/