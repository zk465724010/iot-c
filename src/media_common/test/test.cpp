
//#pragma comment（lib，"media_common.lib")
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include  "libavutil/time.h"
#include "media_common.h"

static bool typehead=false;

int write_buffer(void *opaque, unsigned char *buf, int buf_size)
{
    int true_size=0;
    FILE* fp_write=(FILE*)opaque;
    if(!feof(fp_write)){
        if(typehead==false)
            typehead=true;
        else
        {
          true_size=fwrite(buf,1,buf_size,fp_write);
        }
         printf("write %ld to file\n",true_size);
         return true_size;
    }else{
        return -1;
    }
}

static int slicename=0;
FILE* fp1=NULL;
static unsigned char *ftyp;
int ftyp_size;
int write_flag(void *opaque, unsigned char *buf, int buf_size)
{
    FILE* fp_write=(FILE*)opaque;
    FILE* fp;
    if(slicename==0)
    {
        ftyp=(unsigned char *)malloc(buf_size);
        memcpy(ftyp,buf,buf_size);
        ftyp_size=buf_size;
    }
    if(slicename==7)
    {
        fwrite(ftyp,1,ftyp_size,fp1);
    }

    if(slicename<7)
        fp=fp_write;
    else
        fp=fp1;
    slicename++;
    if(fp_write!=NULL){
        int true_size=fwrite(buf,1,buf_size,fp);
        printf("write %ld to file\n",true_size);
        return true_size;
    }else{
        return -1;
    }
}

int main()
{

    int choice;
    FILE* fp;
    char cmd[2]={0,'r'};

    printf("1 rtsp to mp4 2 slice to mp4 3 rtsp to slice 4 rtsp to mutil slices 5 direct rtsp 6 rtsp to silice autostop 7 265 to 264\n");
    scanf("%d",&choice);
    if(choice==1)
    {
        fp=fopen("record.mp4","wb+");
       // fp1=fopen("record1.mp4","wb+");
       // rtsp_stream("rtsp://Skwl@2020:Skwl@2021@192.168.41.252:554/Streaming/Channels/101?transportmode=unicast&profile=Profile_1", (void*)fp,write_buffer,cmd);
       rtsp_stream("rtsp://Skwl@2020:Skwl@2021@192.168.41.156:554/cam/realmonitor?channel=1&subtype=0&unicast=true&proto=Onvif", (void*)fp,write_buffer,cmd);
        fclose(fp);
        //fclose(fp1);
    }
    else if(choice==2){
        fp=fopen("curr.mp4","wb+");
        float speed=2.0;
        slice_mp4("zhi.mp4",18*60,(void*)fp,write_buffer,cmd,&speed);
        fflush(fp);
        fclose(fp);

    }
    else if(choice==3){
        rtsp_slice("rtsp://Skwl@2020:Skwl@2021@192.168.42.156:554/cam/realmonitor?channel=1&subtype=0&unicast=true&proto=Onvif","curr.mp4",cmd);
    }
    else if(choice==4){
        const char* slice_name[]={"1.mp4","2.mp4","3.mp4","4.mp4","5.mp4"} ;
        rtsp_muti_slice("rtsp://Skwl@2020:Skwl@2021@192.168.42.156:554/cam/realmonitor?channel=1&subtype=0&unicast=true&proto=Onvif",slice_name,5,60,cmd);
    }
    else if(choice==5){
        fp=fopen("direct.mp4","wb+");
        rtsp_stream_direct("rtsp://Skwl@2020:Skwl@2021@192.168.42.156:554/cam/realmonitor?channel=1&subtype=0&unicast=true&proto=Onvif",(void*)fp,write_buffer,cmd);
        fclose(fp);
    }
    else if(choice==6){
        int videotype=-1;
        int recordtime=0;
        int ret= rtsp_slice_autostop("rtsp://test:xz123456@192.168.41.252:554/Streaming/Channels/101","3600.mp4",cmd,3600,&videotype,&recordtime);
        printf("%d %d %d\n",videotype,recordtime,ret);
    }
    else if(choice==7){
        fp=fopen("transcode.mp4","wb+");
        slice_h265toh264_mp4("h265_high.mp4",0,(void*)fp,write_buffer,cmd);
        fflush(fp);
        fclose(fp);
    }
    else if (choice==8){
        int videotype=-1;
        int recordtime=0;
        int ret= rtsp_slice_codeautostop("rtsp://Skwl@2020:Skwl@2021@192.168.41.156:554/cam/realmonitor?channel=1&subtype=0&unicast=true&proto=Onvif","drop25.mp4",cmd,120,&videotype,&recordtime,25);
        printf("%d %d %d\n",videotype,recordtime,ret);
    }
   else if(choice==9){
       fp=fopen("record.mp4","wb+");
       tcp_h264_stream("192.168.0.104",5656, (void*)fp,write_buffer,cmd);
       fclose(fp);
    }
    else if(choice==10){
        int videotype=-1;
        int recordtime=0;
        int ret= tcp_slice_autostop("192.168.0.104",5656,"60.mp4",cmd,180,&videotype,&recordtime);
        printf("%d %d %d\n",videotype,recordtime,ret);
    }
    return 0;
}

