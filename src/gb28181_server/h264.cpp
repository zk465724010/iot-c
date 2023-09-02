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
	int pos = 0;                  //һ��nal����һ��nal �����ƶ���ָ��
	int StartCodeFound  = 0;      //�Ƿ��ҵ���һ��nal ��ǰ׺
	int rewind = 0;               //�ж� ǰ׺��ռ�ֽ��� 3�� 4
	unsigned char * Buf = NULL;
	static int info2 =0 ;
	static int info3 =0 ;

	if ((Buf = (unsigned char*)calloc (nalu->max_size , sizeof(char))) == NULL) 
	{
		LOG("ERROR", "Could not allocate Buf memory\n");
	}

	nalu->startcodeprefix_len = 4;      //��ʼ��ǰ׺λ�����ֽ�

	if (4 != fread (Buf, 1, 4, fp))//���ļ���ȡ�����ֽڵ�buf
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
	//Ѱ����һ���ַ�����λ�� �� Ѱ��һ��nal ��һ��0000001 ����һ��00000001
	StartCodeFound = 0;
	info2 = 0;
	info3 = 0;
	while (!StartCodeFound)
	{
		if (feof (fp))                                 //��������ļ���β
		{
			//nalu->len = (pos-1) - nalu->startcodeprefix_len;  //��0 ��ʼ
			nalu->len = pos - 1;
			//memcpy (nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);  
			memcpy(nalu->buf,Buf,nalu->len);

			nalu->forbidden_bit = nalu->buf[nalu->startcodeprefix_len] & 0x80;      // 1 bit--10000000
			nalu->nal_reference_idc = nalu->buf[nalu->startcodeprefix_len] & 0x60;  // 2 bit--01100000
			nalu->nal_unit_type = (nalu->buf[nalu->startcodeprefix_len]) & 0x1f;    // 5 bit--00011111
			free(Buf);
			return pos-1;
		}
		Buf[pos++] = fgetc (fp);                       //Read one char to the Buffer һ���ֽ�һ���ֽڴ��ļ������
		info3 = FindStartCode3(&Buf[pos-5]);		        //Check whether Buf is 0x00000001 
		if(info3 != 1)
		{
			info2 = FindStartCode2(&Buf[pos-4]);            //Check whether Buf is 0x000001
		}
		StartCodeFound = (info2 == 1 || info3 == 1);        //����ҵ���һ��ǰ׺
	}

	rewind = (info3 == 1)? -5 : -4;

	if (0 != fseek (fp, rewind, SEEK_CUR))			    //���ļ��ڲ�ָ���ƶ��� nal ��ĩβ
	{
		free(Buf);
		LOG("ERROR","Cannot fseek in the bit stream file");
	}

	//nalu->len = (pos + rewind) -  nalu->startcodeprefix_len;       //���ð���nal ͷ�����ݳ���
	nalu->len = pos +rewind;
	//memcpy (nalu->buf, &Buf[nalu->startcodeprefix_len], nalu->len);//����һ��nal ���ݵ�������
	memcpy(nalu->buf,Buf,nalu->len);
	nalu->forbidden_bit = nalu->buf[nalu->startcodeprefix_len] & 0x80;                     //1 bit  ����nal ͷ
	nalu->nal_reference_idc = nalu->buf[nalu->startcodeprefix_len] & 0x60;                 // 2 bit
	nalu->nal_unit_type = (nalu->buf[nalu->startcodeprefix_len]) & 0x1f;                   // 5 bit
	free(Buf);

	return (pos + rewind);                                         //Return the length of bytes from between one NALU and the next NALU
}