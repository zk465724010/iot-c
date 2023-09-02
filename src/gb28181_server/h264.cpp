#include "h264.h"
#include "log.h"

NALU_t *AllocNALU(int buffersize)
{
	NALU_t *n = (NALU_t*)calloc(1, sizeof(NALU_t));
	if (NULL == n)
	{
		LOG("ERROR", "Allocate Meory To NALU_t Failed ");
		return NULL;
	}
	n->max_size = buffersize;//Assign buffer size 

	n->buf = (unsigned char*)calloc(buffersize, sizeof(char));
	if (NULL == n->buf)
	{
		free (n);
		LOG("ERROR","Allocate Meory To NALU_t Buffer Failed ");
		return NULL;
	}
	return n;
}

void FreeNALU(NALU_t *n)
{
	if (NULL != n)
	{
		if (n->buf)
		{
			free(n->buf);
			n->buf=NULL;
		}
		free (n);
	}
}

int FindStartCode2 (unsigned char *Buf)
{
	if(Buf[0]!=0 || Buf[1]!=0 || Buf[2] !=1 || (Buf[3] !=0xba && Buf[3]!=0xc0) )//Check whether buf is 0x000001
	{
		return 0;
	}
	else 
	{
		return 1;
	}
}

int FindStartCode3 (unsigned char *Buf)
{
	//Check whether buf is 0x00000001
	if(Buf[0]!=0 || Buf[1]!=0 || Buf[2] !=0 || Buf[3] !=1 || (Buf[4] !=0xba && Buf[4]!=0xc0)) 
	{
		return 0;
	}
	else 
	{
		return 1;
	}
}

int GetAnnexbNALU (NALU_t *nalu, FILE *fp)
{
	int pos = 0;                  //一个nal到下一个nal 数据移动的指针
	int StartCodeFound  = 0;      //是否找到下一个nal 的前缀
	int rewind = 0;               //判断 前缀所占字节数 3或 4
	unsigned char * Buf = NULL;
	static int info2 =0 ;
	static int info3 =0 ;

	if ((Buf = (unsigned char*)calloc (nalu->max_size , sizeof(char))) == NULL) 
	{
		LOG("ERROR", "Could not allocate Buf memory\n");
	}

	nalu->startcodeprefix_len = 4;      //初始化前缀位三个字节

	if (4 != fread (Buf, 1, 4, fp))//从文件读取三个字节到buf
	{
		free(Buf);
		return 0;
	}
	info2 = FindStartCode2 (Buf);       //Check whether Buf is 0x000001ba or 0x000001c0
	if(info2 != 1) 
	{
		//If Buf is not 0x000001,then read one more byte
		if(1 != fread(Buf+4, 1, 1, fp))
		{
			free(Buf);
			return 0;
		}
		info3 = FindStartCode3(Buf);   //Check whether Buf is 0x00000001
		if (info3 != 1)                 //If not the return -1
		{ 
			free(Buf);
			//system("pause");
			return -1;
		}
		else 
		{
			//If Buf is 0x00000001,set the prefix length to 5 bytes
			pos = 5;
			nalu->startcodeprefix_len = 5;
		}
	} 
	else
	{
		//If Buf is 0x000001,set the prefix length to 3 bytes
		pos = 4;
		nalu->startcodeprefix_len = 4;
	}
	//寻找下一个字符符号位， 即 寻找一个nal 从一个0000001 到下一个00000001
	StartCodeFound = 0;
	info2 = 0;
	info3 = 0;
	while (!StartCodeFound)
	{
		if (feof (fp))                                 //如果到了文件结尾
		{
			//nalu->len = (pos-1) - nalu->startcodeprefix_len;  //从0 开始
			nalu->len = pos - 1;
			//memcpy (nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);  
			memcpy(nalu->buf,Buf,nalu->len);

			nalu->forbidden_bit = nalu->buf[nalu->startcodeprefix_len] & 0x80;      // 1 bit--10000000
			nalu->nal_reference_idc = nalu->buf[nalu->startcodeprefix_len] & 0x60;  // 2 bit--01100000
			nalu->nal_unit_type = (nalu->buf[nalu->startcodeprefix_len]) & 0x1f;    // 5 bit--00011111
			free(Buf);
			return pos-1;
		}
		Buf[pos++] = fgetc (fp);                       //Read one char to the Buffer 一个字节一个字节从文件向后找
		info3 = FindStartCode3(&Buf[pos-5]);		        //Check whether Buf is 0x00000001 
		if(info3 != 1)
		{
			info2 = FindStartCode2(&Buf[pos-4]);            //Check whether Buf is 0x000001
		}
		StartCodeFound = (info2 == 1 || info3 == 1);        //如果找到下一个前缀
	}

	rewind = (info3 == 1)? -5 : -4;

	if (0 != fseek (fp, rewind, SEEK_CUR))			    //将文件内部指针移动到 nal 的末尾
	{
		free(Buf);
		LOG("ERROR","Cannot fseek in the bit stream file");
	}

	//nalu->len = (pos + rewind) -  nalu->startcodeprefix_len;       //设置包含nal 头的数据长度
	nalu->len = pos +rewind;
	//memcpy (nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);//拷贝一个nal 数据到数组中
	memcpy(nalu->buf,Buf,nalu->len);
	nalu->forbidden_bit = nalu->buf[nalu->startcodeprefix_len] & 0x80;                     //1 bit  设置nal 头
	nalu->nal_reference_idc = nalu->buf[nalu->startcodeprefix_len] & 0x60;                 // 2 bit
	nalu->nal_unit_type = (nalu->buf[nalu->startcodeprefix_len]) & 0x1f;                   // 5 bit
	free(Buf);

	return (pos + rewind);                                         //Return the length of bytes from between one NALU and the next NALU
}