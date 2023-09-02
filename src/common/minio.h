//////////////////////////////////////////////////////////////////////////////////////////////
// 文件名称：minio.h  &  minio.cpp															//
// 类的名称：CMinIO																			//
// 类的作用：分布式对象存储(上传与下载)														//
// 作    者：zk	 																			//
// 编写日期：2022.04.15 13:53 五 															//
//////////////////////////////////////////////////////////////////////////////////////////////
// MinIO分布式存储

#ifndef __MINIO_H__
#define __MINIO_H__

#define USE_CLASS_MINIO			// 是否使用类
#define USE_MULTIPART_UPLOAD	// 是否使用分片上传

#define USE_IMPORT_EXPORT		 // 注意:必须定义此项 (且必须在aws系列头文件之前定义)
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
	char host[24] = { 0 };			// MinIO服务主机
	int port = 0;					// MinIO服务端口
	char username[32] = { 0 };		// 用户名
	char password[32] = { 0 };		// 密码
	bool verify_ssl = false;		// 是否验证SSL (为true表示使用HTTPS,false表示使用HTTP)
	char region[16] = { 0 };		// 域
	int connect_timeout = 3000;		// 连接超时时间(单位:毫秒)
	int request_timeout = 5400000;	// 连接超时时间(单位:毫秒,注意:不能设置太短,上传大文件时请求时间会比较长)
	//char agent[24] = {0};			// 用户代理
	char bucket[128] = { 0 };		// 桶的名称
	tag_minio_info() {
		strcpy(region, US_EAST_1);
	}
} minio_info_t;

typedef struct tag_bucket {			// 桶
	char name[64] = { 0 };			// 桶名称
	char create_date[32] = { 0 };	// 上一次修改时间
}bucket_t;

typedef struct tag_object {			// 对象(文件)
	char name[256] = {0};			// 对象名称(key)
	char last_modified[32] = {0};	// 上一次修改时间
	char etag[64] = {0};			// eTag
	long long size = 0;				// 对象大小
}object_t;

#ifdef USE_MULTIPART_UPLOAD
#include "cmap.h"
// 分片规则:
//   在构建InputStream时, 会进行分片操作, 我们可以了解到上传文件大小的一些限制 :
//	   (1).分片大小不能小于5MB,大于5GB
//	   (2).对象大小不能超过5TiB
//	   (3).partSize传入-1,默认按照5MB进行分割
//	   (4).分片数量不能超过10000
#define PART_SIZE 10485760			// 分片大小(10M)
typedef struct tag_part_cache {
	unsigned char* buffer = NULL;	// 缓冲区
	int size = 0;					// 缓冲区大小
	int len = 0;					// 缓冲区中数据长度
}part_cache_t;
typedef struct tag_multipart {
	char bucket[128] = { 0 };		// 多分片所属桶
	char remote_name[256] = { 0 };	// 多分片所属远程文件名称(唯一性)
	int number = 1;					// 多分片起始号
	time_t create = 0;				// 创建多分片上传请求的时间 (测试用)
	part_cache_t cache;				// 多分片缓存
	CompletedMultipartUpload parts;	// 每个分片的上传信息
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
	// 上传
	int upload(const char* local_name, const char* remote_name = NULL, const char* bucket = NULL);
	int upload_big(const char* local_name, const char* remote_name = NULL, const char* bucket = NULL);
	int upload(const char* data, int size, const char* remote_name, const char* bucket = NULL);

	// 下载
	int download(const char* remote_name, const char* local_name, const char* bucket = NULL);
	int64_t download(const char* remote_name, size_t start, size_t end, minio_data_cbk cbk, void* arg, const char* bucket = NULL);
	void download_async(const char* remote_name, int (STDCALL* cbk)(void* data, size_t size, void* arg), void* arg, const char* bucket = NULL);
	Model::GetObjectOutcome download(const char* remote_name);

	int scan_object(const char* remote_name, const char* bucket = NULL);
	long long get_file_size(const char* remote_name);

	///////////////////////////////////////////
	// [桶管理]
	int list_buckets(vector<bucket_t>& out);	// 获取远端桶的列表
	int create_bucket(const char* bucket);		// 创建桶
	int delete_bucket(const char* bucket);		// 删除桶

	// [文件管理]
	int list_files(vector<object_t>& out, const char* bucket = NULL);	// 枚举文件
	int delete_file(const char* remote_name, const char* bucket = NULL);// 删除文件
	int delete_files(vector<object_t>& objs, const char* bucket = NULL);// 删除所有文件

	void print_info(minio_info_t *cfg);

public:
	S3Client* m_s3_client = NULL;		// S3 客户端
	minio_info_t m_minio;				// MinIO连接配置

private:
	Aws::SDKOptions m_opts;				// SDKOptions

	//void* m_arg = NULL;

#ifdef USE_MULTIPART_UPLOAD
public:
	// 1.创建多分片上传
	int create_multipart_upload(const char* remote_name, Aws::String& upload_id,const char* bucket=NULL);
	// 2.上传分片
	int upload_part(const char* id, const char* data, int size);
	// 3.完成多分片上传 (合并分片)
	int complete_multipart_upload(const char* id);
	// 4.终止多分片上传 [可选]
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


// 参考链接: https://www.jianshu.com/p/74f13cd08cc7


