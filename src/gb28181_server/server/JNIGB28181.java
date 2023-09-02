
package gb28181;
import java.util.*;
//package com.hirain.sdk.DeviceEntity;

import test.Operation;

public class JNIGB28181
{
      static
      {
            System.loadLibrary("gb28181server");
            //System.load("D:/lib/gb28181server.dll");
      }

      public native int init(LocalConfig cfg);              // 初始化GB28181服务
      public native void uninit();                          // 反初始化GB28181服务
      public native int open(String ip, int port);          // 开启GB28181服务
      public native void close();                           // 关闭GB28181服务
      public native int query(Operation opt);               // 设备查询
      public native int control(Operation opt);             // 设备控制
      public native int test(int n);                        // 测试用

      public native int play(Operation opt);                // 设备点播与回播

      
}
