
/*
		Message Digest Algorithm MD5（中文名为消息摘要算法第五版）为计算机安全领域广泛使用的一种散列函数，
	用以提供消息的完整性保护。该算法的文件号为RFC 1321（R.Rivest,MIT Laboratory for Computer Science and 
	RSA Data Security Inc. April 1992）。

		MD5即Message-Digest Algorithm 5（信息-摘要算法5），用于确保信息传输完整一致。是计算机广泛使用的杂凑算法之一
	（又译摘要算法、哈希算法），主流编程语言普遍已有MD5实现。将数据（如汉字）运算为另一固定长度值，是杂凑算法的
	基础原理，MD5的前身有MD2、MD3和MD4。
	
	MD5算法具有以下特点：
		1、压缩性：任意长度的数据，算出的MD5值长度都是固定的。
		2、容易计算：从原数据计算出MD5值很容易。
		3、抗修改性：对原数据进行任何改动，哪怕只修改1个字节，所得到的MD5值都有很大区别。
		4、强抗碰撞：已知原数据和其MD5值，想找到一个具有相同MD5值的数据（即伪造数据）是非常困难的。
		
		MD5的作用是让大容量信息在用数字签名软件签署私人密钥前被"压缩"成一种保密的格式（就是把一个任意长度的字节串变换
	成一定长的十六进制数字串）。除了MD5以外，其中比较有名的还有sha-1、RIPEMD以及Haval等。
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
	void update(const void* input, size_t length);	//使用输入缓冲区更新上下文
	void update(const string& str);					//用字符串更新上下文
	void update(ifstream& in);						//用文件更新上下文
	const byte* digest();							//返回消息摘要
	string toString();								//返回消息摘要的十六进制字符串
	void reset();									//重新计算状态

private:
	void update(const byte* input, size_t length);
	void final();
	void transform(const byte block[64]);
	void encode(const uint32* input, byte* output, size_t length);
	void decode(const byte* input, uint32* output, size_t length);
	string bytesToHexString(const byte* input, size_t length);//将输入数据转换为十六进制字符串

	//class uncopyable
	MD5(const MD5&);
	MD5& operator=(const MD5&);

private:
	uint32	m_uState[4];				//state (ABCD)
	uint32	m_uCount[2];				//number of bits, modulo 2^64 (low-order word first)
	byte	m_buffer[64];				//输入缓冲
	byte	m_digest[16];				//消息摘要,MD5值
	bool	m_bFinished;				//计算完成了吗?

	static const byte PADDING[64];		//padding for calculate
	static const char HEX[16];
	enum { BUFFER_SIZE = 1024 };
};

#endif

/*
调用示例:

#include <stdio.h>
#include "md5.h"

int main()
{
	MD5 md5;

	char szData[] = "123456";

	md5.reset();
	md5.update(szData);				//用字符串更新上下文
	
	const byte *p = md5.digest();	//返回消息摘要,即:MD5值 定长16字节 (加密后的数据)

	string str = md5.toString();	//将消息摘要(MD5值)转换为字符串(加密数据的十六进制字符串形式)	
	printf("加密:%s\n",str.c_str());

	return 0;
}

*/