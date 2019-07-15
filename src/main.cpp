
#include "AsyncSerial.h"

#include <iostream>
#include <boost/thread.hpp>
#include <json/json.h>

#include <ros/ros.h>
#include <ros/package.h>
#include <std_msgs/String.h>
#include "DiffDriverController.h"
#include "StatusPublisher.h"
#include "xiaoqiang_log/LogRecord.h"

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
    double separation=0,radius=0,crash_distance=0.2;
    bool DebugFlag = false;
    ros::param::param<double>("~wheel_separation", separation, 0.36);
    ros::param::param<double>("~wheel_radius", radius, 0.0825);
    ros::param::param<bool>("~debug_flag", DebugFlag, false);
    ros::param::param<double>("~crash_distance", crash_distance, 0.2);
    double power_scale;
    ros::param::param<double>("~power_scale", power_scale, 1.0);
    xqserial_server::StatusPublisher xq_status(separation,radius,crash_distance,power_scale);

    //获取小车控制参数
    double max_speed,r_min;
    string cmd_topic;
    ros::param::param<double>("~max_speed", max_speed, 5.0);
    ros::param::param<double>("~r_min", r_min, 0.25);
    ros::param::param<std::string>("~cmd_topic", cmd_topic, "cmd_vel");

    // 初始化log发布者和语音发布者
    ros::NodeHandle mNH;
    ros::Publisher log_pub = mNH.advertise<xiaoqiang_log::LogRecord>("/xiaoqiang_log", 1, true);
    ros::Publisher audio_pub = mNH.advertise<std_msgs::String>("/xiaoqiang_tts/text", 1, true);


    try {
        CallbackAsyncSerial serial(port,baud);
        serial.setCallback(boost::bind(&xqserial_server::StatusPublisher::Update,&xq_status,_1,_2));
        xqserial_server::DiffDriverController xq_diffdriver(max_speed,cmd_topic,&xq_status,&serial,r_min);
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

        ros::Duration(0.5).sleep();
        //下发底层红外开启命令
        char cmd_str[6]={(char)0xcd,(char)0xeb,(char)0xd7,(char)0x02,(char)0x44,(char)0x01};
        //serial.write(cmd_str,6);

        ros::Rate r(100);//发布周期为50hz
        while (ros::ok())
        {
            static int i=0;
            if(serial.errorStatus() || serial.isOpen()==false)
            {
                cerr<<"Error: serial port closed unexpectedly"<<endl;
                break;
            }
            xq_status.Refresh();//定时发布状态
            ros::WallDuration t_diff = ros::WallTime::now() - xq_diffdriver.last_ordertime;
            if(t_diff.toSec()>1.5 && t_diff.toSec()<1.7)
            {
              //safety security
              // char cmd_str[13]={(char)0xcd,(char)0xeb,(char)0xd7,(char)0x09,(char)0x74,(char)0x53,(char)0x53,(char)0x53,(char)0x53,(char)0x00,(char)0x00,(char)0x00,(char)0x00};
              // serial.write(cmd_str, 13);
              // std::cout << "oups!" << std::endl;
              //xq_diffdriver.last_ordertime=ros::WallTime::now();
            }
            if(i%100==0 && xq_diffdriver.DetectFlag_)
            {
              //下发底层红外开启命令
              char cmd_str[6]={(char)0xcd,(char)0xeb,(char)0xd7,(char)0x02,(char)0x44,(char)0x01};
              serial.write(cmd_str,6);
            }
            if(i%10 == 0)
            {
              xq_diffdriver.checkStop();
            }
            i++;
            r.sleep();
            //cout<<"run"<<endl;
        }

        quit:
        serial.close();

    }catch (std::exception& e) {
      ROS_ERROR_STREAM("Open " << port << " failed.");
      ROS_ERROR_STREAM("Exception: " << e.what());
      // 检查串口设备是否存在
      if (!exists(port))
      {
          // 发送语音提示消息
          std_msgs::String audio_msg;
          audio_msg.data = "未发现底盘串口，请检查串口连接";
          audio_pub.publish(audio_msg);
          xiaoqiang_log::LogRecord log_record;
          log_record.collection_name = "exception";
          log_record.stamp = ros::Time::now();
          Json::Value record;
          record["type"] = "HARDWARE_ERROR";
          record["info"] = "底盘串口设备未找到: " + port;
          Json::FastWriter fastWriter;
          log_record.record = fastWriter.write(record);
          // 发送日志消息
          log_pub.publish(log_record);
      }
      ros::shutdown();
      return 1;
    }

    ros::shutdown();
    return 0;
}
