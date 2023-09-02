/*
RTP sender example
time   : 2012,5,8
writer : zwg
*/

#ifndef __H264__H__
#define __H264__H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>
#ifdef _WIN32
#include <conio.h>
//#include <WinSock2.h>
//#pragma comment(lib,"ws2_32.lib")
#endif

#define  USE_PORT 20000
#define  USE_IP "127.0.0.1"
#define  MAXDATASIZE 1500
#define  H264 96                   //��������

//extern FILE * pinfile;	
//extern char * inputfilename;

typedef struct
{
	unsigned char v;               //!< Version, 2 bits, MUST be 0x2
	unsigned char p;			   //!< Padding bit, Padding MUST NOT be used
	unsigned char x;			   //!< Extension, MUST be zero
	unsigned char cc;       	   //!< CSRC count, normally 0 in the absence of RTP mixers 		
	unsigned char m;			   //!< Marker bit
	unsigned char pt;			   //!< 7 bits, Payload Type, dynamically established
	unsigned int seq;			   //!< RTP sequence number, incremented by one for each sent packet 
	unsigned int timestamp;	       //!< timestamp, 27 MHz for H.264
	unsigned int ssrc;			   //!< Synchronization Source, chosen randomly
	unsigned char * payload;      //!< the payload including payload headers
	unsigned int paylen;		   //!< length of payload in bytes
} RTPpacket_t;
#pragma  pack(push)
#pragma pack(1)
typedef struct 
{
	/*  0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|V=2|P|X|  CC   |M|     PT      |       sequence number         |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                           timestamp                           |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|           synchronization source (SSRC) identifier            |
	+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	|            contributing source (CSRC) identifiers             |
	|                             ....                              |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */
	//intel ��cpu ��intelΪС���ֽ��򣨵Ͷ˴浽�׵�ַ�� ��������Ϊ����ֽ��򣨸߶˴浽�͵�ַ��
	/*intel ��cpu �� �߶�->csrc_len:4 -> extension:1-> padding:1 -> version:2 ->�Ͷ�
	 ���ڴ��д洢 ��
	 ��->4001���ڴ��ַ��version:2
	     4002���ڴ��ַ��padding:1
		 4003���ڴ��ַ��extension:1
	 ��->4004���ڴ��ַ��csrc_len:4

     ���紫����� �� �߶�->version:2->padding:1->extension:1->csrc_len:4->�Ͷ�  (Ϊ��ȷ���ĵ�������ʽ)

	 ��������ڴ� ��
	 ��->4001���ڴ��ַ��version:2
	     4002���ڴ��ַ��padding:1
	     4003���ڴ��ַ��extension:1
	 ��->4004���ڴ��ַ��csrc_len:4
	 �����ڴ���� ���߶�->csrc_len:4 -> extension:1-> padding:1 -> version:2 ->�Ͷ� ��
	 ����
	 unsigned char csrc_len:4;        // expect 0 
	 unsigned char extension:1;       // expect 1
	 unsigned char padding:1;         // expect 0 
	 unsigned char version:2;         // expect 2 
	*/
	/* byte 0 */
	 unsigned char csrc_len:4;        /* expect 0 */
	 unsigned char extension:1;       /* expect 1, see RTP_OP below */
	 unsigned char padding:1;         /* expect 0 */
	 unsigned char version:2;         /* expect 2 */
	/* byte 1 */
	 unsigned char payloadtype:7;     /* RTP_PAYLOAD_RTSP */
	 unsigned char marker:1;          /* expect 1 */
	/* bytes 2,3 */
	 unsigned short seq_no;          
	/* bytes 4-7 */
	 unsigned int timestamp;        
	/* bytes 8-11 */
	 unsigned int ssrc;              /* stream number is used here. */
}RTP_HEADER;

typedef struct
{
	unsigned char forbidden_bit = 0;           //! Should always be FALSE
	unsigned char nal_reference_idc = 0;       //! NALU_PRIORITY_xxxx
	unsigned char nal_unit_type = 0;           //! NALU_TYPE_xxxx  
	unsigned int startcodeprefix_len = 0;      //! ǰ׺�ֽ���
	unsigned int len = 0;                      //! ����nal ͷ��nal ���ȣ��ӵ�һ��00000001����һ��000000001�ĳ���
	unsigned int max_size = 0;                 //! ����һ��nal �ĳ���
	unsigned char *buf = NULL;                   //! ����nal ͷ��nal ����
	unsigned int lost_packets = 0;             //! Ԥ��
} NALU_t;
#pragma pack(pop)
/*
+---------------+
|0|1|2|3|4|5|6|7|
+-+-+-+-+-+-+-+-+
|F|NRI|  Type   |
+---------------+
*/
typedef struct 
{
	//byte 0
	unsigned char TYPE:5;
	unsigned char NRI:2;
	unsigned char F:1;        
} NALU_HEADER; // 1 BYTE 

/*
+---------------+
|0|1|2|3|4|5|6|7|
+-+-+-+-+-+-+-+-+
|F|NRI|  Type   |
+---------------+
*/
typedef struct 
{
	//byte 0
	unsigned char TYPE:5;
	unsigned char NRI:2; 
	unsigned char F:1;              
} FU_INDICATOR; // 1 BYTE 

/*
+---------------+
|0|1|2|3|4|5|6|7|
+-+-+-+-+-+-+-+-+
|S|E|R|  Type   |
+---------------+
*/
typedef struct 
{
	//byte 0
	unsigned char TYPE:5;
	unsigned char R:1;
	unsigned char E:1;
	unsigned char S:1;    
} FU_HEADER;   // 1 BYTES 


void FreeNALU(NALU_t *n);                                   //�ͷ�nal ��Դ     
NALU_t *AllocNALU(int buffersize);                          //����nal ��Դ
int FindStartCode2 (unsigned char *Buf);                    //�ж�nal ǰ׺�Ƿ�Ϊ3���ֽ�
int FindStartCode3 (unsigned char *Buf);                    //�ж�nal ǰ׺�Ƿ�Ϊ4���ֽ�
int GetAnnexbNALU (NALU_t *nalu, FILE *fp);                 //��дnal ���ݺ�ͷ

#endif