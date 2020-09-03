
#include "AsyncSerial.h"

#include <iostream>
#include <boost/thread.hpp>

#include <ros/ros.h>
#include <ros/package.h>
#include "DiffDriverController.h"
#include "StatusPublisher.h"
#include <std_msgs/String.h>

using namespace std;

inline bool exists(const std::string &name)
{
  struct stat buffer;
  return (stat(name.c_str(), &buffer) == 0);
}

int main(int argc, char **argv)
{
  cout<<"welcome to xiaoqiang serial server,please feel free at home!"<<endl;

  ros::init(argc, argv, "xqserial_server");
  ros::start();

  //获取串口参数
  std::string port;
  ros::param::param<std::string>("~port", port, "/dev/ttyUSB0");
  int baud;
  ros::param::param<int>("~baud", baud, 115200);
  cout<<"port:"<<port<<" baud:"<<baud<<endl;
  //获取小车机械参数
  double separation=0,radius=0;
  bool DebugFlag = false;
  ros::param::param<double>("~wheel_separation", separation, 0.36);
  ros::param::param<double>("~wheel_radius", radius, 0.0825);
  ros::param::param<bool>("~debug_flag", DebugFlag, false);

  double rot_dist,tran_dist;
  ros::param::param<double>("~rot_dist", rot_dist, -0.21);
  ros::param::param<double>("~tran_dist", tran_dist, -0.3);


  double power_scale;
  ros::param::param<double>("~power_scale", power_scale, 1.0);
  xqserial_server::StatusPublisher xq_status(separation,radius,power_scale);
  xq_status.setBarParams(rot_dist,tran_dist);

  //获取小车控制参数
  double max_speed,r_min;
  string cmd_topic;
  ros::param::param<double>("~max_speed", max_speed, 200);
  ros::param::param<std::string>("~cmd_topic", cmd_topic, "cmd_vel");
  ros::param::param<double>("~r_min", r_min, 0.25);

  double angle_limit,x_limit,y_limit;
  ros::param::param<double>("~angle_limit", angle_limit, 1.6);
  ros::param::param<double>("~x_limit", x_limit, 1.2);
  ros::param::param<double>("~y_limit", y_limit, 0.2);
  // 初始化语音发布者1
  ros::NodeHandle mNH;
  ros::Publisher audio_pub = mNH.advertise<std_msgs::String>("/xiaoqiang_tts/text", 1, true);

  //初始化c2 c4输出
  // ros::param::set("/xqserial_server/params/out1", 0);
  // ros::param::set("/xqserial_server/params/out2", 0);

  try {
    CallbackAsyncSerial serial(port,baud);
    serial.setCallback(boost::bind(&xqserial_server::StatusPublisher::Update,&xq_status,_1,_2));
    xqserial_server::DiffDriverController xq_diffdriver(max_speed,cmd_topic,&xq_status,&serial,r_min);
    xq_diffdriver.setBarParams(angle_limit,tran_dist,x_limit,y_limit);
    boost::thread cmd2serialThread(& xqserial_server::DiffDriverController::run,&xq_diffdriver);
    // send test flag
    char debugFlagCmd[] = {(char)0xcd, (char)0xeb, (char)0xd7, (char)0x01, 'T'};
    if(DebugFlag){
      std::cout << "Debug mode Enabled" << std::endl;
      serial.write(debugFlagCmd, 5);
    }
    // send reset cmd
    char resetCmd[] = {(char)0xcd, (char)0xeb, (char)0xd7, (char)0x01, 'I'};
    serial.write(resetCmd, 5);

    ros::Duration(1).sleep();

    ros::Rate r(100);//发布周期为50hz
    static int i=0;
    while (ros::ok())
    {
      if(serial.errorStatus() || serial.isOpen()==false)
      {
        cerr<<"Error: serial port closed unexpectedly"<<endl;
        break;
      }
      xq_status.Refresh();//定时发布状态

      if(i%2 == 0)
      {
        xq_diffdriver.Refresh();//更新驱动速度 50hz
      }
       
      if(i%100 == 0)
      {
        //每隔1秒下发心跳包
        xq_diffdriver.sendHeartbag();
      }

      // //更新按钮
      // if(xq_diffdriver.dealBackSwitch())
      // {
      //   //告诉用户回去了
      //   std_msgs::String audio_msg;
      //   audio_msg.data = "好的，我回去了!";
      //   audio_pub.publish(audio_msg);
      // }
      //
      // if(i%10 == 0)
      // {
      //   xq_diffdriver.updateC2C4();
      // }

      i++;
      r.sleep();
    }

    quit:
    serial.close();

  } catch (std::exception& e) {
    ROS_ERROR_STREAM("Open " << port << " failed.");
    ROS_ERROR_STREAM("Exception: " << e.what());
    // 检查串口设备是否存在
    if (!exists(port))
    {
      // 发送语音提示消息
      std_msgs::String audio_msg;
      audio_msg.data = "未发现底盘串口，请检查串口连接";
      audio_pub.publish(audio_msg);
    }
    ros::shutdown();
    return 1;
  }

  ros::shutdown();
  return 0;
}
