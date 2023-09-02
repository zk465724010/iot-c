
#include "minio.h"
#include "log.h"

#include <iostream>
#include <fstream>

//#define USE_IMPORT_EXPORT		 // 注意:必须定义此项 (且必须在aws系列头文件之前定义)
//#include "aws/core/Aws.h"
//#include "aws/s3/S3Client.h"
#include "aws/core/auth/AWSCredentialsProvider.h"
#include "aws/s3/model/PutObjectRequest.h"
#include "aws/s3/model/GetObjectRequest.h"
//#include "aws/s3/model/CreateMultipartUploadRequest.h"
//#include "aws/s3/model/UploadPartRequest.h"
//#include "aws/s3/model/CompleteMultipartUploadRequest.h"
//#include "aws/s3/model/AbortMultipartUploadRequest.h"
#include "aws/s3/model/ListPartsRequest.h"
#include "aws/s3/model/ListObjectsRequest.h"
#include "aws/s3/model/DeleteObjectRequest.h"
#include "aws/s3/model/DeleteBucketRequest.h"
#include "aws/s3/model/CreateBucketRequest.h"
#include "aws/s3/model/DeleteObjectsRequest.h"
//#include "aws/s3/model/GetBucketTaggingRequest.h"
#include "aws/s3/model/HeadObjectRequest.h"
#include <aws/core/utils/logging/ConsoleLogSystem.h> // ConsoleLogSystem

using namespace Aws::S3;
using namespace Aws::S3::Model;
using namespace Aws::Utils::Logging;  // ConsoleLogSystem

#ifdef _WIN32
    #include  <io.h>			// access(),_access()
    #define file_name(x) strrchr(x,'\\') ? strrchr(x,'\\')+1 : x
    #ifdef _WINDOWS_
    #define GetObject GetObject     // 由于Windows.h中的宏"#define GetObject GetObjectA"与此文件中的函数同名,此处还原函数名
    #define GetMessage GetMessage
    #endif
#elif __linux__
    #include <string.h>
    #include <unistd.h>			// access()
    #define file_name(x) strrchr(x,'/') ? strrchr(x,'/')+1 : x
#else
    #error Unknown system
#endif
#define ERR(n) ((n)>0) ? -(n) : (n)

#ifdef USE_CLASS_MINIO
CMinIO::CMinIO()
{
    //Aws::SDKOptions options;
    //m_opts.loggingOptions.logLevel = LogLevel::Debug;     // 控制台记录器,会将消息打印到标准输出
    //m_opts.loggingOptions.logger_create_fn = [] { return std::make_shared<ConsoleLogSystem>(LogLevel::Trace); };
    Aws::InitAPI(m_opts);
}
CMinIO::~CMinIO()
{
    Aws::ShutdownAPI(m_opts);
}
int CMinIO::init(minio_info_t* minio)
{
    if (NULL == minio) {
        LOG("ERROR","Input parameter error %p\n", minio);
        return -1;
    }
    m_minio = *minio;

    Aws::Client::ClientConfiguration cfg;
    char endpointOverride[24] = { 0 };
    sprintf(endpointOverride, "%s:%d", minio->host, minio->port);
    cfg.endpointOverride = endpointOverride;     // S3服务器地址和端口
    cfg.verifySSL = minio->verify_ssl;           // 若为true则采用HTTPS,为false则采用HTTP
    cfg.scheme = minio->verify_ssl ? cfg.scheme = Aws::Http::Scheme::HTTPS : cfg.scheme = Aws::Http::Scheme::HTTP;
    //cfg.region = Aws::Region::US_EAST_1;        // 域
    cfg.region = minio->region;                   // 域
    cfg.connectTimeoutMs = minio->connect_timeout;// 连接超时时间
    //cfg.requestTimeoutMs = minio->request_timeout;// 请求超时时间
    cfg.requestTimeoutMs = 2000;// 请求超时时间
    //cfg.proxyHost = "10.10.1.81";               // 代理服务主机
    //cfg.proxyPort = 9001;                       // 主机端口
    //cfg.proxyUserName = "admin";                // 用户名
    //cfg.proxyPassword = "12345678";             // 密码
    //cfg.userAgent = "zk";                       // 代理人
    // 
    // 参数3: 取值Never,Always
    m_s3_client = new S3Client(Aws::Auth::AWSCredentials(minio->username, minio->password), cfg, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Always, false);
    return 0;
}
void CMinIO::release()
{
    if (NULL != m_s3_client) {
        delete m_s3_client;
        m_s3_client = NULL;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能: 上传文件																			//
// 函数参数: ①local_name[in]:	本地待上传的文件名称										    //
//			 ②remote_name[in]: 上传到MinIO中的文件名称(为NULL时,自动与本地名称相同)			//
//           ③bucket[in]: 桶的名称(默认NULL,为NULL时必须在init时指定)                          //
// 函数返回: 成功则返回大于等于0,否则返回错误码(小于0)			       							//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::upload(const char* local_name, const char* remote_name/*=NULL*/, const char* bucket/*=NULL*/)
{
    if ((NULL == local_name) || (NULL == m_s3_client)) {
        LOG("ERROR", "Input parameter error. [local_name:%p,s3_client:%p]\n", local_name, m_s3_client);
        return -1;
    }
    int nRet = -1;
    do {
        nRet = access(local_name, 0);
        if (0 != nRet) {
            LOG("ERROR", "File does not exist '%s'\n", local_name);
            break;
        }
        auto fs = Aws::MakeShared<Aws::FStream>("PutObjectInputStream", local_name, std::ios_base::in | std::ios_base::binary);
        
        std::shared_ptr<std::iostream> content = std::make_shared<std::stringstream>();
        *content << "Thank you for using Alibaba Cloud Object Storage Service!";

        // 远程文件名称
        remote_name = (NULL != remote_name) ? remote_name : file_name(local_name);
        bucket = (NULL != bucket) ? bucket : m_minio.bucket;
        PutObjectRequest request;
        // [可选] 请参见如下示例设置访问权限'ACL'为私有(private),存储类型为标准存储(Standard)
        //request.AddMetadata("x-oss-object-acl", "private");
        //request.AddMetadata("x-oss-storage-class", "Standard");
        #if 1
        request.WithBucket(bucket).WithKey(remote_name);   // 桶和对象的名称
        #else
        request.SetBucket(bucket);                        // 桶的名称
        request.SetKey(remote_name);                      // 在MinIO中的名称
        #endif
        request.SetBody(fs);                               // 设置请求体 Body
        //request.SetContentLength();                      // 设置Body的长度
        //request.SetContentType("application/octet-stream");// 设置Content的类型
        //request.SetContentType("binary/octet-stream");   // 设置Content的类型(图片)
        auto result = m_s3_client->PutObject(request);
        if (result.IsSuccess()) {
            nRet = 0;
            printf("Upload file succeed '%s'\n", local_name);
        }
        else {
            auto& err = result.GetError();
            nRet = (int)err.GetResponseCode();
            LOG("ERROR", "PutObject function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
            nRet = ERR(nRet);
        }
    } while (false);
    return nRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能: 上传大文件																			//
// 函数参数: ①local_name[in]:	本地待上传的文件名称										    //
//			 ②remote_name[in]: 上传到MinIO中的文件名称(为NULL时,自动与本地名称相同)			//
//           ③bucket[in]: 桶的名称(默认NULL,为NULL时必须在init时指定)                          //
// 函数返回: 成功则返回大于等于0,否则返回错误码(小于0)			       							//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::upload_big(const char* local_name, const char* remote_name/*=NULL*/, const char* bucket/*=NULL*/)
{
    if ((NULL == local_name) || (NULL == m_s3_client)) {
        LOG("ERROR", "Input parameter error. [local_name:%p,s3_client:%p]\n", local_name, m_s3_client);
        return -1;
    }
    int nRet = -1;
    do {
        nRet = access(local_name, 0);
        if (0 != nRet) {
            LOG("ERROR", "File does not exist '%s'\n", local_name);
            nRet = -1;
            break;
        }
        // 远程文件名称(即在MinIO中的显示名称) 
        remote_name = (NULL != remote_name) ? remote_name : file_name(local_name);
        bucket = (NULL != bucket) ? bucket : m_minio.bucket;

        CreateMultipartUploadRequest creReq;
        creReq.WithBucket(bucket).WithKey(remote_name);

        // 1.创建分片上传请求 (获取上传ID)
        auto creRet = m_s3_client->CreateMultipartUpload(creReq);
        if (creRet.IsSuccess()) {
            Aws::String upload_id = creRet.GetResult().GetUploadId(); // 得到上传ID

            CompleteMultipartUploadRequest comReq;
            comReq.WithBucket(bucket).WithKey(remote_name);
            comReq.SetUploadId(upload_id);
            CompletedMultipartUpload com_mp_upload;
            FILE* fp = fopen(local_name, "rb");
            if (NULL != fp) {
                int size = 1024 * 1024 * 10;     // 注意: 分片大小不小于5M,分片个数不能超过10000个
                char* buffer = new char[size];
                int part_num = 1;
                while (!feof(fp)) {
                    int bytes = fread(buffer, 1, size, fp);
                    if (bytes > 0) {
                        const std::shared_ptr<Aws::IOStream> ios = Aws::MakeShared<Aws::StringStream>("tag", Aws::String(buffer));
                        ios.get()->write(buffer, bytes);

                        UploadPartRequest upReq;
                        upReq.WithBucket(bucket).WithKey(remote_name);
                        upReq.SetUploadId(upload_id);
                        upReq.SetContentLength(bytes);
                        upReq.SetContentType("binary/octet-stream");   // 设置Content的类型(图片)
                        upReq.SetBody(ios);   // 设置请求体 Body
                        upReq.SetPartNumber(part_num);

                        // 2.上传分片
                        auto upRet = m_s3_client->UploadPart(upReq);
                        if (upRet.IsSuccess()) {
                            static int index = 0;
                            //printf("%d size: %d\n", index++, bytes);
                            // 添加分片信息至集合中
                            CompletedPart part;
                            part.WithPartNumber(part_num).WithETag(upRet.GetResult().GetETag());
                            //part.SetPartNumber(part_num); // 设置分片号
                            //part.SetETag(upRet.GetResult().GetETag()); // 设置ETag
                            com_mp_upload.AddParts(part);   // 添加至分片集合
                            nRet = 0;
                        }
                        else {
                            auto& err = upRet.GetError();
                            nRet = (int)err.GetResponseCode();
                            LOG("ERROR", "UploadPart function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
                            nRet = ERR(nRet);
                            break;
                        }
                    }
                }
                fclose(fp);
                delete[] buffer;
            }
            else LOG("ERROR", "File open failed '%s'\n", local_name);
            comReq.SetMultipartUpload(com_mp_upload);

            // 3.合并分片 (此函数执行之前,在MinIO控制台是看不到上传对象的)
            auto comRet = m_s3_client->CompleteMultipartUpload(comReq);
            if (comRet.IsSuccess()) {
                printf("File upload succeed '%s'\n", local_name);
            }
            else {
                auto& err = comRet.GetError();
                nRet = (int)err.GetResponseCode();
                LOG("ERROR", "CompleteMultipartUpload function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
                nRet = ERR(nRet);
            }
            // 4.终止分片上传
            //AbortMultipartUploadRequest abtReq;
            //abtReq.WithBucket(bucket).WithKey(remote_name);
            //abtReq.SetUploadId(id);   // 设置上传ID
            //auto &abtRet = m_s3_client->AbortMultipartUpload(abtReq);
            //if (!abtRet.IsSuccess()) {
            //    auto& err = abtRet.GetError();
            //    nRet = (int)err.GetResponseCode();
            //    LOG("ERROR", "AbortMultipartUpload function failed %d '%s' '%s'\n", nRet, err.GetExceptionName().c_str(), err.GetMessage().c_str());
            //}
        }
        else {
            auto& err = creRet.GetError();
            nRet = (int)err.GetResponseCode();
            LOG("ERROR", "CreateMultipartUpload function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
            nRet = ERR(nRet);
            break;
        }
    } while (false);
    return nRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能: 上传数据																			//
// 函数参数: ①data[in]: 待上传的数据										                    //
//			 ②size[in]: 数据大小			                                                    //
//           ③remote_name[in]: 上传到MinIO中的文件名称(为NULL时)                               //
//           bucket[in]: 桶的名称(默认NULL,为NULL时必须在init时指定)                            //
// 函数返回: 成功则返回大于等于0,否则返回错误码(小于0)			       							//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::upload(const char* data, int size, const char* remote_name, const char* bucket/*=NULL*/)
{
    if ((NULL == data) || (size <= 0) || (NULL == remote_name) || (NULL == m_s3_client)) {
        LOG("ERROR", "Input parameter error. [data=%p,size=%d,remote_name=%p,s3_client:%p]\n", data, size, remote_name, m_s3_client);
        return -1;
    }
    int nRet = -1;
    do {
        bucket = (NULL != bucket) ? bucket : m_minio.bucket;
        const std::shared_ptr<Aws::IOStream> ios = Aws::MakeShared<Aws::StringStream>("tag", Aws::String(data));
        ios.get()->write(data, size);

        PutObjectRequest request;
        request.WithBucket(bucket).WithKey(remote_name);   // 桶和对象的名称
        request.SetBody(ios);                               // 设置请求体 Body
        request.SetContentLength(size);                    // 设置Body的长度
        request.SetContentType("binary/octet-stream");     // 设置Content的类型(图片)
        //request.SetContentType("application/octet-stream");// 设置Content的类型
        auto result = m_s3_client->PutObject(request);
        if (result.IsSuccess()) {
            nRet = 0;
            LOG("INFO", "Upload data succeed %d\n", size);
        }
        else {
            auto& err = result.GetError();
            nRet = (int)err.GetResponseCode();
            LOG("ERROR", "PutObject function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
            nRet = ERR(nRet);
        }
    } while (false);
    return nRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能：下载文件																			//
// 函数参数：①remote_name[in]: 远程文件名称                                                    //
//			 ②local_name[in]: 本地文件名称(可为路径, 若为路径时则本地文件名称与远程相同)  	    //
//           ③bucket[in]: 桶的名称(默认NULL,为NULL时必须在init时指定)                          //
// 函数返回：成功则返回0,否则返回错误码(小于0)			        								//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::download(const char* remote_name, const char* local_name, const char* bucket/*=NULL*/)
{
    if ((NULL == remote_name) || (NULL == local_name) || (NULL == m_s3_client)) {
        LOG("ERROR", "Input parameter error. [remote_name:%p,local_name:%p,s3_client:%p]\n", remote_name, local_name, m_s3_client);
        return -1;
    }
    int nRet = -1;
    bucket = (NULL != bucket) ? bucket : m_minio.bucket;
    Aws::S3::Model::GetObjectRequest request;
    request.WithBucket(bucket).WithKey(remote_name);   // 桶和对象的名称

    // 范围下载
    std::size_t start = 5, end = 10;
    Aws::StringStream range;
    range << "bytes=" << start << "-" << end;
    request.SetRange(range.str().c_str());
    //TransferProgress progressCallback = { ProgressCallback , nullptr };
    //request.SetTransferProgress(progressCallback);
    //request.SetResponseStreamFactory([=]() {return std::make_shared<std::fstream>(local_name, std::ios_base::out | std::ios_base::in | std::ios_base::trunc | std::ios_base::binary); });
    //Aws::IOStreamFactory factory;// = std::make_shared<std::fstream>(local_name, std::ios_base::out | std::ios_base::in | std::ios_base::trunc | std::ios_base::binary);
    //request.SetResponseStreamFactory();
    auto result = m_s3_client->GetObject(request);
    if (result.IsSuccess()) {
        LOG("INFO", "Download file succeed '%s'\n", remote_name);
        #if 1
            // 写本地文件
            Aws::OFStream ofs;
            ofs.open(local_name, std::ios::out | std::ios::binary);
            bool is_open = ofs.is_open();
            if (is_open) {
                ofs << result.GetResult().GetBody().rdbuf();
                nRet = 0;
            }
            else LOG("ERROR", "Write to local file failed '%s'\n", local_name);
        #else
            // 处理下载下来的流
            //Aws::String content_tye = result.GetResult().GetContentType();
            Aws::IOStream& ios = result.GetResultWithOwnership().GetBody();
            ios.seekg(0, ios.end);
            int file_size = ios.tellg();
            ios.seekg(0, ios.beg);
            printf("file size: %d\n", file_size);
            char* buffer = new char[1024];
            //ios.getline(buffer, file_size);       // 读取一行到Buffer中
            streamsize size = 0;
            while (!ios.eof()) {
                ios.read(buffer, 1024);            // 读取流到Buffer中
                printf("data %lld\n", ios.gcount());
                size += ios.gcount();
            }
            printf("file size: %lld\n", size);
            //Aws::StringStream stream(Aws::String(buffer));
            //const std::shared_ptr<Aws::IOStream> ios2 = Aws::MakeShared<Aws::StringStream>("tag", Aws::String(buffer));
            //delete[] buffer;
            nRet = 0;
        #endif 
    }
    else {
        auto& err = result.GetError();
        nRet = (int)err.GetResponseCode();
        LOG("ERROR", "GetObject function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
        nRet = ERR(nRet);
    }
    return nRet;
}
long long CMinIO::get_file_size(const char* remote_name)
{
    if (NULL == remote_name) {
        LOG("ERROR", "Input parameter error. [remote_name:%p]\n", remote_name);
        return -1;
    }
    long long filesize = 0;
    // 获取整个文件的大小
    Aws::S3::Model::HeadObjectRequest head_request;
    head_request.WithBucket(m_minio.bucket).WithKey(remote_name);
    auto outcome = m_s3_client->HeadObject(head_request);
    if (outcome.IsSuccess()) {
        filesize = outcome.GetResult().GetContentLength(); // 获取文件字节数
        //outcome.GetResult().GetPartsCount(); // 获取文件分片数
    }
    else {
        auto& err = outcome.GetError();
        int nRet = (int)err.GetResponseCode();
        LOG("ERROR", "HeadObject function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
        nRet = ERR(nRet);
        return nRet;
    }
    return filesize;
}
int64_t CMinIO::download(const char* remote_name, size_t start, size_t end, minio_data_cbk cbk, void* arg, const char* bucket/*=NULL*/)
{
    if ((NULL == remote_name) || (start < 0) || (start >= end) || (NULL == cbk) || (NULL == m_s3_client)) {
        LOG("ERROR", "Input parameter error. [remote_name:%p,start:%lld,end:%lld,ckb:%p,s3_client:%p]\n", remote_name, start, end, cbk, m_s3_client);
        return 0;
    }
    int64_t nRet = 0;
    bucket = (NULL != bucket) ? bucket : m_minio.bucket;
    Aws::S3::Model::GetObjectRequest request;
    request.WithBucket(bucket).WithKey(remote_name);   // 桶和对象的名称
    size_t count = 0;   // 测试
    Aws::StringStream range;
    range << "bytes=" << start << "-" << end;
    request.SetRange(range.str().c_str()); // 按字节下载
    auto result = m_s3_client->GetObject(request);
    if (result.IsSuccess()) {
        int64_t len = result.GetResult().GetContentLength();
        Aws::IOStream& ios = result.GetResultWithOwnership().GetBody();
        if (!ios.eof()) {
            char* buffer = (char*)malloc(len + 1);
            if (NULL != buffer) {
                ios.read(buffer, len);
                if (ios.gcount() > 0) {
                    cbk(buffer, ios.gcount(), arg);
                    nRet = ios.gcount();
                }
                free(buffer);
            }
            else LOG("ERROR", "Allocation memory failed %lld\n", len + 1);
        }
    }
    else {
        auto& err = result.GetError();
        int code = (int)err.GetResponseCode();
        LOG("ERROR", "GetObject function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), code, err.GetMessage().c_str());
    }
    //cbk(NULL, 0, arg);
    return nRet;
}
void CMinIO::download_async(const char* remote_name, int (STDCALL* cbk)(void* data, size_t size, void* arg), void* arg, const char* bucket/*=NULL*/)
{
    if ((NULL == remote_name) || (NULL == cbk)) {
        LOG("ERROR", "Input parameter error. [remote_name:%p,ckb:%p]\n", remote_name, cbk);
        return;
    }
    bucket = (NULL != bucket) ? bucket : m_minio.bucket;

    auto callback = [&](const Aws::S3::S3Client* client,
        const Aws::S3::Model::GetObjectRequest& request,
        const Aws::S3::Model::GetObjectOutcome& outcome,
        const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context)
    {
            printf("ContentLength: %lld\n", outcome.GetResult().GetContentLength());
            if (outcome.IsSuccess()) {
            }
            else {
                auto& err = outcome.GetError();
                int nRet = (int)err.GetResponseCode();
                LOG("ERROR", "GetObject function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
            }
    };
    auto context = Aws::MakeShared<Aws::Client::AsyncCallerContext>("");
    //pthread_mutex_lock(&service_mutex);
    //s3_client.GetObjectAsync(object_request, callback, context);
    //pthread_cond_wait(&service_cond, &service_mutex);
    //pthread_mutex_unlock(&service_mutex);

    Aws::S3::Model::GetObjectRequest request;
    request.WithBucket(bucket).WithKey(remote_name);   // 桶和对象的名称

    m_s3_client->GetObjectAsync(request, callback, context); // 异步请求,函数立刻返回,函数GetObject为阻塞式
}

Model::GetObjectOutcome CMinIO::download(const char* remote_name)
{
    if (NULL == remote_name) {
        LOG("ERROR", "Input parameter error. [remote_name:%p]\n", remote_name);
        return Model::GetObjectOutcome();
    }
    const char* bucket = m_minio.bucket;
    Aws::S3::Model::GetObjectRequest request;
    request.WithBucket(bucket).WithKey(remote_name);   // 桶和对象的名称
    time_t t1 = time(NULL);
    return m_s3_client->GetObject(request);
    //const Aws::String s = request.GetRange(); // 设置下载范围
    //printf("request time %llds, '%s'\n", time(NULL) - t1, s.c_str());
    //if (result.IsSuccess()) {
    //    return &result;
    //}
    //else {
    //    auto& err = result.GetError();
    //    int nRet = (int)err.GetResponseCode();
    //    LOG("ERROR", "GetObject function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
    //    nRet = ERR(nRet);
    //}
    //return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能：扫描对象          																    //
// 函数参数：①remote_name[in]: 远程对象名称                                                    //
//			 ②bucket[int]: 存储桶的名称                                                        //
// 函数返回：成功则返回0,否则返回错误码(小于0)			        								//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::scan_object(const char* remote_name, const char* bucket/*=NULL*/)
{
    if (NULL == remote_name) {
        LOG("ERROR", "Input parameter error. [remote_name:%p]\n", remote_name);
        return -1;
    }
    int nRet = -1;
    bucket = (NULL != bucket) ? bucket : m_minio.bucket;
    Aws::S3::Model::HeadObjectRequest request;
    request.WithBucket(bucket).WithKey(remote_name);
    auto outcome = m_s3_client->HeadObject(request);
    if (outcome.IsSuccess()) {
        long long filesize = outcome.GetResult().GetContentLength();
        nRet = 0;
    }
    else {
        auto& err = outcome.GetError();
        int nRet = (int)err.GetResponseCode();
        LOG("ERROR", "GetObject function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
        nRet = ERR(nRet);
    }
    return nRet;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能：创建桶          																    //
// 函数参数：①bucket[int]: 待创建桶的名称                                                      //
// 函数返回：成功则返回0,否则返回错误码(小于0)			        								//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::create_bucket(const char* bucket)
{
    if ((NULL == m_s3_client) || (NULL == bucket)) {
        LOG("ERROR", "Input parameter error, [s3_client:%p,bucket:%p]\n", m_s3_client, bucket);
        return -1;
    }
    int nRet = 0;
    CreateBucketRequest request;
    request.WithBucket(bucket);
    auto outcome = m_s3_client->CreateBucket(request);
    if (!outcome.IsSuccess()) {
        auto& err = outcome.GetError();
        nRet = (int)err.GetResponseCode();
        LOG("ERROR", "CreateBucket function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
        nRet = ERR(nRet);
    }
    return nRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能：删除桶          																    //
// 函数参数：①bucket[int]: 待删除桶的名称                                                      //
// 函数返回：成功则返回0,否则返回错误码(小于0)			        								//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::delete_bucket(const char* bucket)
{
    if ((NULL == m_s3_client) || (NULL == bucket)) {
        LOG("ERROR", "Input parameter error, [s3_client:%p,bucket:%p]\n", m_s3_client, bucket);
        return -1;
    }
    int nRet = 0;
    DeleteBucketRequest request;
    request.WithBucket(bucket);
    auto outcome = m_s3_client->DeleteBucket(request);
    if (!outcome.IsSuccess()) {
        auto& err = outcome.GetError();
        nRet = (int)err.GetResponseCode();
        LOG("ERROR", "DeleteBucket function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
        nRet = ERR(nRet);
    }
    return nRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能：枚举桶          																    //
// 函数参数：①out[out]: 输出桶的列表                                                           //
// 函数返回：成功则返回0,否则返回错误码(小于0)			        								//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::list_buckets(vector<bucket_t>& out)
{
    if (NULL == m_s3_client) {
        LOG("ERROR", "Input parameter error, [s3_client:%p]\n", m_s3_client);
        return -1;
    }
    int nRet = 0;
    auto response = m_s3_client->ListBuckets();
    if (response.IsSuccess()) {
        auto buckets = response.GetResult().GetBuckets();
        for (auto iter = buckets.begin(); iter != buckets.end(); ++iter) {
            //printf("bucket: %s, %s\n", iter->GetName().c_str(), iter->GetCreationDate().ToLocalTimeString(Aws::Utils::DateFormat::ISO_8601).c_str());
            bucket_t b;
            strcpy(b.name, iter->GetName().c_str());
            strcpy(b.create_date, iter->GetCreationDate().ToLocalTimeString(Aws::Utils::DateFormat::ISO_8601).c_str());
            out.push_back(b);
        }
    }
    else {
        auto& err = response.GetError();
        nRet = (int)err.GetResponseCode();
        LOG("ERROR", "ListBuckets function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
        nRet = ERR(nRet);
    }
    return nRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能：删除文件          																    //
// 函数参数：①remote_name[in]: 待删除文件名称                                                  //
//			 ②bucket[in]: 桶的名称  	                                                        //
// 函数返回：成功则返回0,否则返回错误码(小于0)			        								//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::delete_file(const char* remote_name, const char* bucket/*=NULL*/)
{
    if ((NULL == m_s3_client) || (NULL == remote_name)) {
        LOG("ERROR", "Input parameter error, [s3_client:%p,remote_name:%p]\n", m_s3_client, remote_name);
        return -1;
    }
    int nRet = 0;
    bucket = (NULL != bucket) ? bucket : m_minio.bucket;
    DeleteObjectRequest request;
    request.WithBucket(bucket).WithKey(remote_name);
    auto outcome = m_s3_client->DeleteObject(request);
    if (!outcome.IsSuccess()) {
        auto& err = outcome.GetError();
        nRet = (int)err.GetResponseCode();
        LOG("ERROR", "DeleteObject function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
        nRet = ERR(nRet);
    }
    return nRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能：删除文件          																    //
// 函数参数：①objs[in]: 待删除的文件列表                                                       //
//			 ②bucket[in]: 桶的名称  	                                                        //
// 函数返回：成功则返回0,否则返回错误码(小于0)			        								//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::delete_files(vector<object_t>& objs, const char* bucket/*=NULL*/)
{
    if (NULL == m_s3_client) {
        LOG("ERROR", "Input parameter error, [s3_client:%p]\n", m_s3_client);
        return -1;
    }
    int nRet = 0;
    int size = objs.size();
    if (size <= 0)
        return nRet;

    bucket = (NULL != bucket) ? bucket : m_minio.bucket;
    Delete items;
    for (int i = 0; i < size; i++) {
        ObjectIdentifier item;
        item.WithKey(objs[i].name);        
        items.AddObjects(item);
    }
    DeleteObjectsRequest request;
    request.WithBucket(bucket).WithDelete(items);
    auto outcome = m_s3_client->DeleteObjects(request);
    if (!outcome.IsSuccess()) {
        auto& err = outcome.GetError();
        nRet = (int)err.GetResponseCode();
        LOG("ERROR", "DeleteObject function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
        nRet = ERR(nRet);
    }
    return nRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能：获取文件列表          																//
// 函数参数：①out[out]: 输出文件列表                                                           //
//			 ②bucket[in]: 桶的名称  	                                                        //
// 函数返回：成功则返回0,否则返回错误码(小于0)			        								//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::list_files(vector<object_t>& out, const char* bucket/*=NULL*/)
{
    if (NULL == m_s3_client) {
        LOG("ERROR", "Input parameter error, [s3_client:%p]\n", m_s3_client);
        return -1;
    }
    int nRet = -1;
    bucket = (NULL != bucket) ? bucket : m_minio.bucket;
    Aws::S3::Model::ListObjectsRequest request;
    request.WithBucket(bucket);
    auto outcome = m_s3_client->ListObjects(request);
    if (outcome.IsSuccess()) {
        nRet = 0;
        Aws::Vector<Object> objs = outcome.GetResult().GetContents();
        for (const auto& obj : objs) {
            //printf("%s, %lldB, %s, %s\n", obj.GetKey().c_str(), obj.GetSize(), obj.GetLastModified().ToGmtString("%Y-%m-%d %H:%M:%S").c_str(), obj.GetETag().c_str());
            object_t o;
            strcpy(o.name, obj.GetKey().c_str());
            o.size = obj.GetSize();
            strcpy(o.etag, obj.GetETag().c_str());
            strcpy(o.last_modified, obj.GetLastModified().ToGmtString("%Y-%m-%d %H:%M:%S").c_str());
            out.push_back(o);
        }
    }
    else {
        auto& err = outcome.GetError();
        nRet = (int)err.GetResponseCode();
        LOG("ERROR", "ListObjects function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
        nRet = ERR(nRet);
    }
    return nRet;
}
void CMinIO::print_info(minio_info_t* cfg)
{
    if (NULL != cfg) {
        printf("            host: %s:%d\n", cfg->host, cfg->port);
        printf("        username: %s:%s\n", cfg->username, cfg->password);
        printf("      verify_ssl: %s\n", cfg->verify_ssl?"true":"false");
        printf("          region: %s\n", cfg->region);
        printf(" connect_timeout: %dms\n", cfg->connect_timeout);
        printf(" request_timeout: %dms\n", cfg->request_timeout);
        printf("          bucket: %s\n", cfg->bucket);
        printf("\n\n");
    }
}

#ifdef USE_MULTIPART_UPLOAD
//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能：创建多分片上传请求																	//
// 函数参数：①remote_name[in]: 远程文件名称                                                    //
//			 ②upload_id[out]: 上传ID (对应每一个多分片上传请求)  	                            //
//           ③bucket[in]: 桶的名称(默认NULL,为NULL时必须在init时指定)                          //
// 函数返回：成功则返回大于等于0,否则返回错误码(小于0)		       								//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::create_multipart_upload(const char* remote_name, Aws::String& upload_id,const char* bucket/*=NULL*/)
{
    if ((NULL == remote_name) || (NULL == m_s3_client)) {
        LOG("ERROR", "Input parameter error. [remote_name:%p,s3_client:%p]\n", remote_name, m_s3_client);
        return -1;
    }
    bucket = (NULL != bucket) ? bucket : m_minio.bucket;

    // 创建多分片上传传请求
    CreateMultipartUploadRequest create_multipart_request;
    create_multipart_request.WithBucket(bucket).WithKey(remote_name);

    int nRet = -1;
    // 1.创建分片上传请求 (获取上传ID)
    auto creRet = m_s3_client->CreateMultipartUpload(create_multipart_request);
    if (creRet.IsSuccess()) {
        nRet = 0;
        upload_id = creRet.GetResult().GetUploadId(); // 得到上传ID
        multipart_t mpart;
        mpart.create = time(NULL);  // 创建多分片上传请求的时间 (测试用)
        strcpy(mpart.bucket, bucket);
        strcpy(mpart.remote_name, remote_name);
        mpart.cache.size = PART_SIZE;
        mpart.cache.buffer = (unsigned char*)malloc(PART_SIZE);
        m_upload_map.add(upload_id.c_str(), &mpart);
        //printf("create multipart succeed ^-^ [id:%s, cache:%d/%d]\n", upload_id.c_str(),mpart.cache.len, mpart.cache.size);
    }
    else {
        auto& err = creRet.GetError();
        nRet = (int)err.GetResponseCode();
        LOG("ERROR", "CreateMultipartUpload function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
        nRet = ERR(nRet);
    }
    return nRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能：上传分片请求																        //
// 函数参数：①id[in]: 上传ID                                                                   //
//			 ②data[in]: 待上传的数据  	                                                        //
//           ③size[in]: 数据大小                                                               //
// 函数返回：成功则返回大于等于0,否则返回错误码(小于0)		       								//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::upload_part(const char* id, const char* data, int size)
{
    if ((NULL == id) || (NULL == m_s3_client)) {
        LOG("ERROR", "Input parameter error [id:%p,s3_client:%p]\n", id, m_s3_client);
        return -1;
    }
    auto iter = m_upload_map.find(id);
    if (iter == m_upload_map.end()) {
        LOG("ERROR", "Upload id does not exist '%s'\n", id);
        return -1;
    }
    if ((NULL != iter->second.cache.buffer) && (size <= iter->second.cache.size)) {
        if ((NULL != data) && (size > 0)) {
            if ((iter->second.cache.len + size) < iter->second.cache.size) { // 开始缓存数据
                memcpy(iter->second.cache.buffer + iter->second.cache.len, data, size);
                iter->second.cache.len += size;
                //printf("cache data [cache:%d/%d]\n", iter->second.cache.len, iter->second.cache.size);
                return 0;
            }
        }
        // 缓存已满,开始上传
        const std::shared_ptr<Aws::IOStream> ios = Aws::MakeShared<Aws::StringStream>("tag", Aws::String((char*)iter->second.cache.buffer));
        ios.get()->write((char*)iter->second.cache.buffer, iter->second.cache.len);
        // 设置请求参数
        UploadPartRequest request;
        request.WithBucket(iter->second.bucket).WithKey(iter->second.remote_name);
        request.SetUploadId(id);
        request.SetContentType("binary/octet-stream");   // 设置Content的类型(图片)
        request.SetContentLength(iter->second.cache.len);
        request.SetBody(ios);   // 设置请求体 Body
        request.SetPartNumber(iter->second.number++);
        int nRet = -1;
        // 2.上传分片
        auto upRet = m_s3_client->UploadPart(request);
        if (upRet.IsSuccess()) {
            //printf("etag:[%s]\n", upRet.GetResult().GetETag().c_str());
            //////////////////////////////////////////////////////////////
            // ETag用来判断请求的文件是否被修改
            CompletedPart part;
            part.WithPartNumber(iter->second.number - 1).WithETag(upRet.GetResult().GetETag());
            iter->second.parts.AddParts(part);
            //////////////////////////////////////////////////////////////
            nRet = 0;
            iter->second.cache.len = 0;
            if ((NULL != data) && (size > 0)) {
                memcpy(iter->second.cache.buffer, data, size); // 重新开始缓存数据
                iter->second.cache.len = size;
            }
            //printf("upload part %d succeed [id:%s, cache:%d/%d]\n", (iter->second.number-1),id,iter->second.cache.len, iter->second.cache.size);
        }
        else {
            auto& err = upRet.GetError();
            nRet = (int)err.GetResponseCode();
            LOG("ERROR", "UploadPart function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
            nRet = ERR(nRet);
        }
        return nRet;
    }
    else LOG("ERROR", "cache.buffer=%p,cache.size=%d,data.size=%d\n", iter->second.cache.buffer, iter->second.cache.size, size);
    return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能：完成多分片上传请求 (合并多分片)												    //
// 函数参数：①id[in]: 上传ID                                                                   //
// 函数返回：成功则返回大于等于0,否则返回错误码(小于0)		       								//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::complete_multipart_upload(const char* id)
{
    if ((NULL == id) || (NULL == m_s3_client)) {
        LOG("ERROR", "Input parameter error [id:%p,s3_client:%p]\n", id, m_s3_client);
        return -1;
    }
    int nRet = -1;
    do {
        auto iter = m_upload_map.find(id);
        if (iter == m_upload_map.end()) {
            LOG("ERROR", "Upload id does not exist '%s'\n", id);
            break;
        }
        // 上传缓冲中剩余的数据
        if ((NULL != iter->second.cache.buffer) && (iter->second.cache.len > 0)) {
            upload_part(id, NULL, 0);
        }
        ////////////////////////////////////////////////////////////////////////////////
        //CompletedMultipartUpload com_parts;
        //ListPartsRequest list_part_request;
        //list_part_request.WithBucket(iter->second.bucket).WithKey(iter->second.remote_name);
        //list_part_request.SetUploadId(id);
        //auto list_part_ret = m_s3_client->ListParts(list_part_request);
        //if (!list_part_ret.IsSuccess()) {
        //    auto& err = list_part_ret.GetError();
        //    nRet = (int)err.GetResponseCode();
        //    LOG("ERROR", "ListParts function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
        //    nRet = ERR(nRet);
        //    break;
        //}
        //int size = list_part_ret.GetResult().GetParts().size();
        //if (size <= 0) {
        //    LOG("WARN","Parts list %d\n", size);
        //    break;
        //}
        //auto begin = list_part_ret.GetResult().GetParts().begin();
        //auto end = list_part_ret.GetResult().GetParts().end();
        //for (; begin != end; begin++) {
        //    CompletedPart part;
        //    part.WithPartNumber(begin->GetPartNumber()).WithETag(begin->GetETag());
        //    com_parts.AddParts(part);
        //}
        ////////////////////////////////////////////////////////////////////////////////
        // 完成多分片上传请求 (合并分片请求)
        CompleteMultipartUploadRequest complete_request;
        complete_request.WithBucket(iter->second.bucket).WithKey(iter->second.remote_name);
        complete_request.SetUploadId(id);
        //complete_request.
        //complete_request.SetMultipartUpload(com_parts);
        complete_request.SetMultipartUpload(iter->second.parts);
        // 3.合并分片 (此函数执行之前,在MinIO控制台是看不到上传对象的)
        auto comRet = m_s3_client->CompleteMultipartUpload(complete_request);
        if (comRet.IsSuccess()) {
            nRet = 0;
            int count = iter->second.parts.GetParts().size(); // 分片的数量
            printf("merge multipart succeed ^-^ [id:%s, parts:%d,time:%llds,cache:%d/%d]\n", id, count, (time(NULL)-iter->second.create),iter->second.cache.len, iter->second.cache.size);
        }
        else {
            auto& err = comRet.GetError();
            nRet = (int)err.GetResponseCode();
            LOG("ERROR", "CompleteMultipartUpload function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
            nRet = ERR(nRet);
        }
        if (NULL != iter->second.cache.buffer) {
            free(iter->second.cache.buffer);
            iter->second.cache.buffer = NULL;
        }
        m_upload_map.erase(iter);
    } while (false);
    return nRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// 函数功能：终止多分片上传请求															        //
// 函数参数：①id[in]: 上传ID                                                                   //
// 函数返回：成功则返回大于等于0,否则返回错误码(小于0)		       								//
//////////////////////////////////////////////////////////////////////////////////////////////////
int CMinIO::abort_multipart_upload(const char* id)
{
    if ((NULL == id) || (NULL == m_s3_client)) {
        LOG("ERROR", "Input parameter error, [id:%p,s3_client:%p]\n", id, m_s3_client);
        return -1;
    }
    auto iter = m_upload_map.find(id);
    if (iter == m_upload_map.end()) {
        LOG("ERROR", "Upload id does not exist '%s'\n", id);
        return -1;
    }
    int nRet = 0;
    // 4.终止分片上传
    AbortMultipartUploadRequest abort_upload_request;
    abort_upload_request.WithBucket(iter->second.bucket).WithKey(iter->second.remote_name);
    abort_upload_request.SetUploadId(id);
    auto abtRet = m_s3_client->AbortMultipartUpload(abort_upload_request);
    if (abtRet.IsSuccess()) {
        int count = iter->second.parts.GetParts().size(); // 分片的数量
        printf("abort multipart succeed. [id:%s, parts:%d,time:%llds,cache:%d/%d]\n", id, count, (time(NULL) - iter->second.create), iter->second.cache.len, iter->second.cache.size);
    }
    else {
        auto& err = abtRet.GetError();
        nRet = (int)err.GetResponseCode();
        LOG("ERROR", "AbortMultipartUpload function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
        nRet = ERR(nRet);
    }
    if (NULL != iter->second.cache.buffer) {
        free(iter->second.cache.buffer);
        iter->second.cache.buffer = NULL;
        iter->second.cache.len = 0;
    }
    m_upload_map.erase(iter);
    return nRet;
}
#endif  // #ifdef USE_MULTIPART_UPLOAD

#else  // #ifdef USE_CLASS_MINIO

int upload(const minio_info_t* minio, const char* local_name, const char* remote_name/*=NULL*/)
{
    if ((NULL == minio) || NULL == local_name) {
        LOG("ERROR", "Input parameter error %p %p\n", minio, local_name);
        return -1;
    }
    int nRet = -1;
    Aws::SDKOptions opts;				// SDKOptions
    Aws::InitAPI(opts);
    do {
        nRet = access(local_name, 0);
        if (0 != nRet) {
            LOG("ERROR", "Upload file does not exist '%s'\n", local_name);
            break;
        }
        auto fs = Aws::MakeShared<Aws::FStream>("PutObjectInputStream", local_name, std::ios_base::in | std::ios_base::binary);
        // 远程文件名称(即在MinIO中的显示名称) 
        remote_name = (NULL != remote_name) ? remote_name : file_name(local_name);
        PutObjectRequest request;
        #if 1
        request.WithBucket(minio->bucket).WithKey(remote_name);// 桶和对象的名称
        #else
        request.SetBucket(minio->bucket);                     // 桶的名称
        request.SetKey(remote_name);                      // 在MinIO中的名称
        #endif
        request.SetBody(fs);                               // 设置请求体 Body
        //request.SetContentLength();                      // 设置Body的长度
        //request.SetContentType("application/octet-stream");// 设置Content的类型
        //request.SetContentType("binary/octet-stream");   // 设置Content的类型(图片)

        Aws::Client::ClientConfiguration cfg;
        char endpointOverride[24] = { 0 };
        sprintf(endpointOverride, "%s:%d", minio->host, minio->port);
        cfg.endpointOverride = endpointOverride;     // S3服务器地址和端口
        cfg.verifySSL = minio->verify_ssl;           // 若为true则采用HTTPS,为false则采用HTTP
        cfg.scheme = minio->verify_ssl ? cfg.scheme = Aws::Http::Scheme::HTTPS : cfg.scheme = Aws::Http::Scheme::HTTP;
        //cfg.region = Aws::Region::US_EAST_1;        // 域
        cfg.region = minio->region;                   // 域
        cfg.connectTimeoutMs = minio->connect_timeout;// 连接超时时间
        cfg.requestTimeoutMs = minio->request_timeout;// 请求超时时间
        //cfg.proxyHost = "10.10.1.81";               // 代理服务主机
        //cfg.proxyPort = 9001;                       // 主机端口
        //cfg.proxyUserName = "admin";                // 用户名
        //cfg.proxyPassword = "12345678";             // 密码
        //cfg.userAgent = "zk";                       // 代理人
        // 
        // 参数3: 取值Never,Always
        S3Client client(Aws::Auth::AWSCredentials(minio->username, minio->password), cfg, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Always, false);
        auto& result = client.PutObject(request);
        if (result.IsSuccess()) {
            nRet = 0;
            LOG("INFO", "Upload file succeed '%s'\n", local_name);
        }
        else {
            auto& err = result.GetError();
            nRet = (int)err.GetResponseCode();
            LOG("ERROR", "PutObject function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
            nRet = ERR(nRet);
        }
    } while (false);
    Aws::ShutdownAPI(opts);
    return nRet;
}
int download(const minio_info_t* minio, const char* remote_name, const char* local_name)
{
    if ((NULL == minio) || (NULL == remote_name) || (NULL == local_name)) {
        LOG("ERROR", "Input parameter error %p %p\n", minio, remote_name, local_name);
        return -1;
    }
    Aws::SDKOptions opts;				// SDKOptions
    Aws::InitAPI(opts);
    Aws::S3::Model::GetObjectRequest request;
    #if 1
    request.WithBucket(minio->bucket).WithKey(remote_name);// 桶和对象的名称
    #else
    request.SetBucket(minio->bucket);                 // 桶的名称
    request.SetKey(remote_name);                      // 在MinIO中的名称
    #endif

    //S3GetObjectRequest* getObjectRequest;
    //AmazonS3Client* s3;

    Aws::Client::ClientConfiguration cfg;
    char endpointOverride[24] = { 0 };
    sprintf(endpointOverride, "%s:%d", minio->host, minio->port);
    cfg.endpointOverride = endpointOverride;     // S3服务器地址和端口
    cfg.verifySSL = minio->verify_ssl;           // 若为true则采用HTTPS,为false则采用HTTP
    cfg.scheme = minio->verify_ssl ? cfg.scheme = Aws::Http::Scheme::HTTPS : cfg.scheme = Aws::Http::Scheme::HTTP;
    //cfg.region = Aws::Region::US_EAST_1;        // 域
    //cfg.region = minio->region;                   // 域
    cfg.connectTimeoutMs = 2000;// minio->connect_timeout;// 连接超时时间
    cfg.requestTimeoutMs = 2000;// minio->request_timeout;// 请求超时时间
    cfg.proxyHost = "10.180.16.60";               // 代理服务主机
    cfg.proxyPort = 9002;                       // 主机端口
    cfg.proxyUserName = "admin";                // 用户名
    cfg.proxyPassword = "12345678";             // 密码
    cfg.userAgent = "zk";                       // 代理人
    int nRet = -1;
    // 参数3: 取值Never,Always
    //S3Client(const Aws::Client::ClientConfiguration & clientConfiguration = Aws::Client::ClientConfiguration(), Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy signPayloads = Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, bool useVirtualAdressing = true);
    //S3Client(const Aws::Auth::AWSCredentials & credentials, const Aws::Client::ClientConfiguration & clientConfiguration = Aws::Client::ClientConfiguration(), Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy signPayloads = Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, bool useVirtualAdressing = true);
    //S3Client client(Aws::Client::ClientConfiguration(minio->username, minio->password));
    S3Client client(Aws::Auth::AWSCredentials(minio->username, minio->password), cfg, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);
    
    //client.DownloadPart();
    time_t t1 = time(NULL);
    auto& result = client.GetObject(request);
    time_t t2 = time(NULL);
    int n = t2 - t1;
    printf("time: %ds\n\n", n);
    if (result.IsSuccess()) {
        LOG("INFO", "Download file succeed '%s'\n", remote_name);
        #if 1
        // 写本地文件
        Aws::OFStream ofs;
        ofs.open(local_name, std::ios::out | std::ios::binary);
        bool is_open = ofs.is_open();
        if (is_open) {
            ofs << result.GetResult().GetBody().rdbuf();
            nRet = 0;
        }
        else LOG("ERROR", "Write to local file failed '%s'\n", local_name);
        #else
        // 处理下载下来的流
        //Aws::String content_tye = result.GetResult().GetContentType();
        Aws::IOStream& ios = result.GetResultWithOwnership().GetBody();

        ios.seekg(0, ios.end);
        int file_size = ios.tellg();
        ios.seekg(0, ios.beg);
        //printf("file size: %d\n", file_size);

        char* buffer = new char[file_size + 1];
        memset(buffer, 0, file_size + 1);
        //ios.getline(buffer, file_size);       // 读取一行到Buffer中
        ios.read(buffer, file_size);            // 读取流到Buffer中
        //Aws::Utils::StringUtils::to_string(file_size).c_str()
        //LOG("INFO", "data: %s\n", buffer);     // 这里也可以将buffer写文件

        Aws::StringStream stream(Aws::String(buffer));
        const std::shared_ptr<Aws::IOStream> ios2 = Aws::MakeShared<Aws::StringStream>("tag", Aws::String(buffer));
        delete[] buffer;
        nRet = 0;
        #endif 
    }
    else {
        auto& err = result.GetError();
        nRet = (int)err.GetResponseCode();
        LOG("ERROR", "GetObject function failed, [exception:'%s', err_code:%d, msg:'%s']\n", err.GetExceptionName().c_str(), nRet, err.GetMessage().c_str());
        nRet = ERR(nRet);
    }
    Aws::ShutdownAPI(opts);
    return nRet;
}
bool downloadfile(std::string BucketName, std::string objectKey, std::string pathkey)
{
    Aws::SDKOptions m_options;
    S3Client* m_client = { NULL };

    Aws::InitAPI(m_options);
    Aws::Client::ClientConfiguration cfg;
    cfg.endpointOverride = "10.180.16.60:9002";  // S3服务器地址和端口
    cfg.scheme = Aws::Http::Scheme::HTTP;
    cfg.verifySSL = false;
    //Aws::Auth::AWSCredentials cred("RPW421T9GSIO4A45Y9ZR", "2owKYy9emSS90Q0pXuyqpX1OxBCyEDYodsiBemcq");  // 认证的Key
    m_client = new S3Client(Aws::Auth::AWSCredentials("admin", "12345678"), cfg, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);

#if 1
    Aws::S3::Model::GetObjectRequest object_request;
    object_request.WithBucket(BucketName.c_str()).WithKey(objectKey.c_str());
    // 范围下载
    std::size_t start = 5, end = 30;
    Aws::StringStream range;
    range << "bytes=" << start << "-" << end;
    object_request.SetRange(range.str().c_str());

    time_t t1 = time(NULL);
    //auto outcome = m_client->GetObject(object_request);
    Aws::S3::Model::GetObjectOutcome& outcome = m_client->GetObject(object_request);

    time_t t2 = time(NULL);
    printf("time1: %ds\n\n", (t2 - t1));
    if (outcome.IsSuccess())
    {
        Aws::OFStream local_file;
        local_file.open(pathkey.c_str(), std::ios::out | std::ios::binary);
        local_file << outcome.GetResult().GetBody().rdbuf();
        std::cout << "Done!" << std::endl;
        return true;
    }
    else
    {
        std::cout << "GetObject error: " <<
            outcome.GetError().GetExceptionName() << " " <<
            outcome.GetError().GetMessage() << std::endl;
        return false;
    }
#else
    Aws::S3::Model::GetObjectRequest object_request;
    object_request.WithBucket(BucketName.c_str()).WithKey(objectKey.c_str());

    //object_request.SetCustomizedAccessLogTag();
    //object_request.SetContinueRequestHandler([handle](const Aws::Http::HttpRequest*) { return handle->ShouldContinue(); });
    //object_request.SetBucket(handle->GetBucketName());
    //object_request.WithKey(handle->GetKey());
    //object_request.SetRange(FormatRangeSpecifier(rangeStart, rangeEnd));
    //object_request.SetResponseStreamFactory(responseStreamFunction);
    //object_request.SetVersionId(handle->GetVersionId());

    //auto self = shared_from_this(); // keep transfer manager alive until all callbacks are finished.

    object_request.SetDataReceivedEventHandler([](const Aws::Http::HttpRequest*, Aws::Http::HttpResponse*, long long progress)
    {
        //partState->OnDataTransferred(progress, handle);
        //self->TriggerDownloadProgressCallback(handle);
    });
    //object_request.SetRequestRetryHandler([](const Aws::AmazonWebServiceRequest&)
    //{
    //    //partState->Reset();
    //    //self->TriggerDownloadProgressCallback(handle);
    //});


    auto callback = [](const Aws::S3::S3Client* client, const Aws::S3::Model::GetObjectRequest& request,
        const Aws::S3::Model::GetObjectOutcome& outcome, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context)
    {
        //self->HandleGetObjectResponse(client, request, outcome, context);
        printf("#\n");
    };
    time_t t1 = time(NULL);
    m_client->GetObjectAsync(object_request, callback);
    time_t t2 = time(NULL);
    printf("time2: %ds\n\n", (t2 - t1));

#endif
    if (m_client != nullptr)
    {
        delete m_client;
        m_client = NULL;
    }
    Aws::ShutdownAPI(m_options);

    return true;
}

void upload_cbk(
    const S3Client* s3_client,
    const Aws::S3::Model::PutObjectRequest& request,
    const Aws::S3::Model::PutObjectOutcome& outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context)
{
}


void download_cbk(const Aws::S3::S3Client* client, const Aws::S3::Model::GetObjectRequest& request,
    const Aws::S3::Model::GetObjectOutcome& outcome, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context)
{
    printf("#\n");
}

bool uploadfile(const minio_info_t* minio, const char* local_name, const char* remote_name)
{
    if ((NULL == minio) || (NULL == remote_name) || (NULL == local_name)) {
        LOG("ERROR", "Input parameter error %p %p\n", minio, remote_name, local_name);
        return -1;
    }
    //Aws::InitAPI(Aws::SDKOptions{});

    Aws::S3::Model::GetObjectRequest request;
#if 1
    request.WithBucket(minio->bucket).WithKey(remote_name);// 桶和对象的名称
#else
    request.SetBucket(minio->bucket);                 // 桶的名称
    request.SetKey(remote_name);                      // 在MinIO中的名称
#endif

    Aws::Client::ClientConfiguration cfg;
    char endpointOverride[24] = { 0 };
    sprintf(endpointOverride, "%s:%d", minio->host, minio->port);
    cfg.endpointOverride = endpointOverride;     // S3服务器地址和端口
    cfg.verifySSL = minio->verify_ssl;           // 若为true则采用HTTPS,为false则采用HTTP
    cfg.scheme = minio->verify_ssl ? cfg.scheme = Aws::Http::Scheme::HTTPS : cfg.scheme = Aws::Http::Scheme::HTTP;
    //cfg.region = Aws::Region::US_EAST_1;        // 域
    //cfg.region = minio->region;                   // 域

    int nRet = -1;
    // 参数3: 取值Never,Always
    S3Client client(Aws::Auth::AWSCredentials(minio->username, minio->password), cfg, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);
    //request.SetBody(std::make_shared<std::stringstream>("test"));
    client.GetObjectAsync(request, download_cbk, nullptr);


    //Aws::ShutdownAPI(opts);
}
#endif   // #ifdef USE_CLASS_MINIO
