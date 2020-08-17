#include "DiffDriverController.h"
#include <time.h>

namespace xqserial_server
{


DiffDriverController::DiffDriverController()
{
    max_wheelspeed=2.0;
    cmd_topic="cmd_vel";
    xq_status=new StatusPublisher();
    cmd_serial=NULL;
    MoveFlag=true;
    last_ordertime=ros::WallTime::now();
    DetectFlag_=true;
}

DiffDriverController::DiffDriverController(double max_speed_,std::string cmd_topic_,StatusPublisher* xq_status_,CallbackAsyncSerial* cmd_serial_)
{
    MoveFlag=true;
    max_wheelspeed=max_speed_;
    cmd_topic=cmd_topic_;
    xq_status=xq_status_;
    cmd_serial=cmd_serial_;
    speed_debug[0]=0.0;
    speed_debug[1]=0.0;
    last_ordertime=ros::WallTime::now();
    DetectFlag_=true;
}

void DiffDriverController::run()
{
    ros::NodeHandle nodeHandler;
    ros::Subscriber sub = nodeHandler.subscribe(cmd_topic, 1, &DiffDriverController::sendcmd, this);
    ros::Subscriber sub2 = nodeHandler.subscribe("/imu_cal", 1, &DiffDriverController::imuCalibration,this);
    ros::Subscriber sub3 = nodeHandler.subscribe("/global_move_flag", 1, &DiffDriverController::updateMoveFlag,this);
    ros::Subscriber sub4 = nodeHandler.subscribe("/barDetectFlag", 1, &DiffDriverController::updateBarDetectFlag,this);
    ros::spin();
}
void DiffDriverController::updateMoveFlag(const std_msgs::Bool& moveFlag)
{
  boost::mutex::scoped_lock lock(mMutex);
  MoveFlag=moveFlag.data;
  last_ordertime=ros::WallTime::now();
}
void DiffDriverController::imuCalibration(const std_msgs::Bool& calFlag)
{
  if(calFlag.data)
  {
    //下发底层ｉｍｕ标定命令
    char cmd_str[5]={(char)0xcd,(char)0xeb,(char)0xd7,(char)0x01,(char)0x43};
    if(NULL!=cmd_serial)
    {
        cmd_serial->write(cmd_str,5);
    }
  }
}
void DiffDriverController::updateBarDetectFlag(const std_msgs::Bool& DetectFlag)
{
  if(DetectFlag.data)
  {
    //下发底层红外开启命令
    char cmd_str[6]={(char)0xcd,(char)0xeb,(char)0xd7,(char)0x02,(char)0x44,(char)0x01};
    if(NULL!=cmd_serial)
    {
        cmd_serial->write(cmd_str,6);
    }
    DetectFlag_=true;
  }
  else
  {
    //下发底层红外禁用命令
    char cmd_str[6]={(char)0xcd,(char)0xeb,(char)0xd7,(char)0x02,(char)0x44,(char)0x00};
    if(NULL!=cmd_serial)
    {
        cmd_serial->write(cmd_str,6);
    }
    DetectFlag_=false;
  }
}

geometry_msgs::Twist DiffDriverController::get_cmdTwist(void)
{
  boost::mutex::scoped_lock lock(mMutex);
  return cmdTwist_;
}
void DiffDriverController::sendcmd2(const geometry_msgs::Twist &command)
{
  {
    boost::mutex::scoped_lock lock(mMutex);
    cmdTwist_ = command;
  }

  geometry_msgs::Twist  cmdTwist = cmdTwist_;

  float cmd_v = cmdTwist.linear.x;
  if(DetectFlag_)
  {
    //超声波预处理
    double distances[2]={0.0,0.0},distance=0;
    xq_status->get_distances(distances);
    distance=std::min(distances[0],distances[1]);

    if(distance>=0.2001 && distance<=0.45)
    {
        float k=0.4;
        float max_v = std::max(0.0,k*std::sqrt(distance-0.35)); //根据当前距离设置线速度最大值

        geometry_msgs::Twist  carTwist = xq_status->get_CarTwist();
        if(max_v<0.01 && carTwist.linear.x<0.1 && carTwist.linear.x>-0.1 && cmdTwist.linear.x> 0.01)
        {
            if(distances[0]>(distances[1]+0.05) )
            {
              if(cmdTwist.angular.z<0.005)
              {
                //可以右转
                cmdTwist.angular.z = std::min(-0.1,cmdTwist.angular.z);
              }
              else
              {
                cmdTwist.angular.z = 0.1;
              }
            }
            else if(distances[1]>(distances[0]+0.05)  && cmdTwist.angular.z > -0.01)
            {
              if(cmdTwist.angular.z > -0.005)
              {
                //可以左转
                cmdTwist.angular.z = std::max(0.1,cmdTwist.angular.z);
              }
              else
              {
                cmdTwist.angular.z = -0.1;
              }
            }
        }
        if(cmd_v >= max_v)
        {
          cmd_v = max_v;
        }
    }
  }
  cmdTwist.linear.x = cmd_v;
  sendcmd(cmdTwist);
}
void DiffDriverController::sendcmd(const geometry_msgs::Twist &command)
{

    static time_t t1=time(NULL),t2;
    int i=0,wheel_ppr=1;
    double separation=0,radius=0,speed_lin=0,speed_ang=0,speed_temp[2];
    char speed[2]={0,0};//右一左二
    char cmd_str[13]={(char)0xcd,(char)0xeb,(char)0xd7,(char)0x09,(char)0x74,(char)0x53,(char)0x53,(char)0x53,(char)0x53,(char)0x00,(char)0x00,(char)0x00,(char)0x00};

    if(xq_status->get_status()==0) return;//底层还在初始化
    separation=xq_status->get_wheel_separation();
    radius=xq_status->get_wheel_radius();
    wheel_ppr=xq_status->get_wheel_ppr();
    geometry_msgs::Twist  carTwist = xq_status->get_CarTwist();

    double vx_temp,vtheta_temp;
    vx_temp=command.linear.x;
    vtheta_temp=command.angular.z;
    if(std::fabs(vx_temp)<0.3)
    {
      if(vtheta_temp>0.0002&&vtheta_temp<0.3) vtheta_temp=0.3;
      if(vtheta_temp<-0.0002&&vtheta_temp>-0.3) vtheta_temp=-0.3;
    }
    if(vx_temp>0 && vx_temp<0.1) vx_temp=0.1;
    if(vx_temp<0 && vx_temp>-0.1) vx_temp=-0.1;
    //转换速度单位，由米转换成转
    speed_lin=command.linear.x/(2.0*PI*radius);
    //speed_ang=command.angular.z*separation/(2.0*PI*radius);
    speed_ang=vtheta_temp*separation/(2.0*PI*radius);

    float scale=std::max(std::abs(speed_lin+speed_ang/2.0),std::abs(speed_lin-speed_ang/2.0))/max_wheelspeed;
    if(scale>1.0)
    {
      scale=1.0/scale;
    }
    else
    {
      scale=1.0;
    }
    //转出最大速度百分比,并进行限幅
    speed_temp[0]=scale*(speed_lin+speed_ang/2)/max_wheelspeed*100.0;
    speed_temp[0]=std::min(speed_temp[0],100.0);
    speed_temp[0]=std::max(-100.0,speed_temp[0]);

    speed_temp[1]=scale*(speed_lin-speed_ang/2)/max_wheelspeed*100.0;
    speed_temp[1]=std::min(speed_temp[1],100.0);
    speed_temp[1]=std::max(-100.0,speed_temp[1]);

  //std::cout<<" "<<speed_temp[0]<<" " << speed_temp[1] <<  " "<< command.linear.x <<" "<< command.angular.z <<  " "<< carTwist.linear.x <<" "<< carTwist.angular.z <<std::endl;
  //std::cout<<"radius "<<radius<<std::endl;
  //std::cout<<"ppr "<<wheel_ppr<<std::endl;
  //std::cout<<"pwm "<<speed_temp[0]<<std::endl;
  //  command.linear.x/
    for(i=0;i<2;i++)
    {
     speed[i] = (int8_t)speed_temp[i];
     speed_debug[i] = (int8_t)speed_temp[i];
     if(speed[i]<0)
     {
         //if(speed[i]>-5) speed[i]=-4;
         cmd_str[5+i]=(char)0x42;//B
         cmd_str[9+i]=-speed[i];
     }
     else if(speed[i]>0)
     {
         //if(speed[i]<5) speed[i]=4;
         cmd_str[5+i]=(char)0x46;//F
         cmd_str[9+i]=speed[i];
     }
     else
     {
         cmd_str[5+i]=(char)0x53;//S
         cmd_str[9+i]=(char)0x00;
     }
    }

    // std::cout<<"hbz1 "<<xq_status->car_status.hbz1<<std::endl;
    // std::cout<<"hbz2 "<<xq_status->car_status.hbz2<<std::endl;
    // std::cout<<"hbz3 "<<xq_status->car_status.hbz3<<std::endl;
    // if(xq_status->get_status()==2)
    // {
    //   //有障碍物
    //   if(xq_status->car_status.hbz1<30&&xq_status->car_status.hbz1>0&&cmd_str[6]==(char)0x46)
    //   {
    //     cmd_str[6]=(char)0x53;
    //   }
    //   if(xq_status->car_status.hbz2<30&&xq_status->car_status.hbz2>0&&cmd_str[5]==(char)0x46)
    //   {
    //     cmd_str[5]=(char)0x53;
    //   }
    //   if(xq_status->car_status.hbz3<20&&xq_status->car_status.hbz3>0&&(cmd_str[5]==(char)0x42||cmd_str[6]==(char)0x42))
    //   {
    //     cmd_str[5]=(char)0x53;
    //     cmd_str[6]=(char)0x53;
    //   }
    //   if(xq_status->car_status.hbz1<15&&xq_status->car_status.hbz1>0&&(cmd_str[5]==(char)0x46||cmd_str[6]==(char)0x46))
    //   {
    //     cmd_str[5]=(char)0x53;
    //     cmd_str[6]=(char)0x53;
    //   }
    //   if(xq_status->car_status.hbz2<15&&xq_status->car_status.hbz2>0&&(cmd_str[5]==(char)0x46||cmd_str[6]==(char)0x46))
    //   {
    //     cmd_str[5]=(char)0x53;
    //     cmd_str[6]=(char)0x53;
    //   }
    // }

    boost::mutex::scoped_lock lock(mMutex);

    if(!MoveFlag)
    {
      cmd_str[5]=(char)0x53;
      cmd_str[6]=(char)0x53;
    }
    if(NULL!=cmd_serial)
    {
        cmd_serial->write(cmd_str,13);
    }
    last_ordertime=ros::WallTime::now();
   // command.linear.x
}






















}
