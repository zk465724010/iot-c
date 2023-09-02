//////////////////////////////////////////////////////////////////////////////////////////////
// �ļ����ƣ�minio.h  &  minio.cpp															//
// ������ƣ�CMinIO																			//
// ������ã��ֲ�ʽ����洢(�ϴ�������)														//
// ��    �ߣ�zk	 																			//
// ��д���ڣ�2022.04.15 13:53 �� 															//
//////////////////////////////////////////////////////////////////////////////////////////////
// MinIO�ֲ�ʽ�洢

#ifndef __MINIO_H__
#define __MINIO_H__

#define USE_CLASS_MINIO			// �Ƿ�ʹ����
#define USE_MULTIPART_UPLOAD	// �Ƿ�ʹ�÷�Ƭ�ϴ�

#define USE_IMPORT_EXPORT		 // ע��:���붨����� (�ұ�����awsϵ��ͷ�ļ�֮ǰ����)
#include "aws/core/Aws.h"
#include "aws/s3/S3Client.h"
#include "aws/s3/model/CreateMultipartUploadRequest.h"
#include "aws/s3/model/UploadPartRequest.h"
#include "aws/s3/model/CompleteMultipartUploadRequest.h"
#include "aws/s3/model/AbortMultipartUploadRequest.h"

using namespace Aws::S3;
using namespace Aws::S3::Model;
using namespace Aws::Region;	// US_EAST_1

#ifdef _WIN32
#define STDCALL __stdcall
#elif __linux__
#define STDCALL __attribute__((__stdcall__))
#else
#error Unknown system
#endif

typedef struct tag_minio_info
{
	char host[24] = { 0 };			// MinIO��������
	int port = 0;					// MinIO����˿�
	char username[32] = { 0 };		// �û���
	char password[32] = { 0 };		// ����
	bool verify_ssl = false;		// �Ƿ���֤SSL (Ϊtrue��ʾʹ��HTTPS,false��ʾʹ��HTTP)
	char region[16] = { 0 };		// ��
	int connect_timeout = 3000;		// ���ӳ�ʱʱ��(��λ:����)
	int request_timeout = 5400000;	// ���ӳ�ʱʱ��(��λ:����,ע��:��������̫��,�ϴ����ļ�ʱ����ʱ���Ƚϳ�)
	//char agent[24] = {0};			// �û�����
	char bucket[128] = { 0 };		// Ͱ������
	tag_minio_info() {
		strcpy(region, US_EAST_1);
	}
} minio_info_t;

typedef struct tag_bucket {			// Ͱ
	char name[64] = { 0 };			// Ͱ����
	char create_date[32] = { 0 };	// ��һ���޸�ʱ��
}bucket_t;

typedef struct tag_object {			// ����(�ļ�)
	char name[256] = {0};			// ��������(key)
	char last_modified[32] = {0};	// ��һ���޸�ʱ��
	char etag[64] = {0};			// eTag
	long long size = 0;				// �����С
}object_t;

#ifdef USE_MULTIPART_UPLOAD
#include "cmap.h"
// ��Ƭ����:
//   �ڹ���InputStreamʱ, ����з�Ƭ����, ���ǿ����˽⵽�ϴ��ļ���С��һЩ���� :
//	   (1).��Ƭ��С����С��5MB,����5GB
//	   (2).�����С���ܳ���5TiB
//	   (3).partSize����-1,Ĭ�ϰ���5MB���зָ�
//	   (4).��Ƭ�������ܳ���10000
#define PART_SIZE 10485760			// ��Ƭ��С(10M)
typedef struct tag_part_cache {
	unsigned char* buffer = NULL;	// ������
	int size = 0;					// ��������С
	int len = 0;					// �����������ݳ���
}part_cache_t;
typedef struct tag_multipart {
	char bucket[128] = { 0 };		// ���Ƭ����Ͱ
	char remote_name[256] = { 0 };	// ���Ƭ����Զ���ļ�����(Ψһ��)
	int number = 1;					// ���Ƭ��ʼ��
	time_t create = 0;				// �������Ƭ�ϴ������ʱ�� (������)
	part_cache_t cache;				// ���Ƭ����
	CompletedMultipartUpload parts;	// ÿ����Ƭ���ϴ���Ϣ
}multipart_t;
#endif  // #ifdef USE_MULTIPART_UPLOAD

#ifdef USE_CLASS_MINIO
typedef int (STDCALL* minio_data_cbk)(void* data, size_t size, void* arg);

class CMinIO
{
public:
	CMinIO();
	~CMinIO();
	int init(minio_info_t *minio);
	void release();
	// �ϴ�
	int upload(const char* local_name, const char* remote_name = NULL, const char* bucket = NULL);
	int upload_big(const char* local_name, const char* remote_name = NULL, const char* bucket = NULL);
	int upload(const char* data, int size, const char* remote_name, const char* bucket = NULL);

	// ����
	int download(const char* remote_name, const char* local_name, const char* bucket = NULL);
	int64_t download(const char* remote_name, size_t start, size_t end, minio_data_cbk cbk, void* arg, const char* bucket = NULL);
	void download_async(const char* remote_name, int (STDCALL* cbk)(void* data, size_t size, void* arg), void* arg, const char* bucket = NULL);
	Model::GetObjectOutcome download(const char* remote_name);

	int scan_object(const char* remote_name, const char* bucket = NULL);
	long long get_file_size(const char* remote_name);

	///////////////////////////////////////////
	// [Ͱ����]
	int list_buckets(vector<bucket_t>& out);	// ��ȡԶ��Ͱ���б�
	int create_bucket(const char* bucket);		// ����Ͱ
	int delete_bucket(const char* bucket);		// ɾ��Ͱ

	// [�ļ�����]
	int list_files(vector<object_t>& out, const char* bucket = NULL);	// ö���ļ�
	int delete_file(const char* remote_name, const char* bucket = NULL);// ɾ���ļ�
	int delete_files(vector<object_t>& objs, const char* bucket = NULL);// ɾ�������ļ�

	void print_info(minio_info_t *cfg);

public:
	S3Client* m_s3_client = NULL;		// S3 �ͻ���
	minio_info_t m_minio;				// MinIO��������

private:
	Aws::SDKOptions m_opts;				// SDKOptions

	//void* m_arg = NULL;

#ifdef USE_MULTIPART_UPLOAD
public:
	// 1.�������Ƭ�ϴ�
	int create_multipart_upload(const char* remote_name, Aws::String& upload_id,const char* bucket=NULL);
	// 2.�ϴ���Ƭ
	int upload_part(const char* id, const char* data, int size);
	// 3.��ɶ��Ƭ�ϴ� (�ϲ���Ƭ)
	int complete_multipart_upload(const char* id);
	// 4.��ֹ���Ƭ�ϴ� [��ѡ]
	int abort_multipart_upload(const char* id);
private:
	CMap<string, multipart_t> m_upload_map;
#endif
};
#else

int upload(const minio_info_t* minio, const char* local_name, const char* remote_name = NULL);
int download(const minio_info_t* minio, const char* remote_name, const char* local_name);
bool downloadfile(std::string BucketName, std::string objectKey, std::string pathkey);
bool uploadfile(const minio_info_t* minio, const char* local_name, const char* remote_name);

#endif   // #ifdef USE_CLASS_MINIO

#endif   // #ifndef __MINIO_H__


// �ο�����: https://www.jianshu.com/p/74f13cd08cc7


