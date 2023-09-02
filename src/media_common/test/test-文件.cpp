
//#pragma comment（lib，"media_common.lib")
#include <stdio.h>
#include "media_common.h"

int main(int argc,char* argv[])
{
   int choice;
   printf("1 rtsp to video and audio 2 video and audio to mp4 3get 10-25 second mp4\n");
   scanf("%d",&choice);
   if(choice==1)
    rtsp_stream("rtsp://Skwl@2020:Skwl@2021@192.168.42.217:554/Streaming/Channels/101?transportmode=unicast&profile=Profile_1", "1.264","2.mp3");
   else if(choice==2)
    videoaudio_mp4( "1.264","2.mp3","3.mp4");
   else if(choice==3)
   {
     const char* slice_names[]={"0-20.mp4","20-40.mp4"};
     slice_mp4(slice_names,2,10,30,"10-30.mp4");
   }
   else
       return 0;
   return 0;
}

