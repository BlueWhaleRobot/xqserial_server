#ifndef DIFFDRIVERCONTROLLER_H
#define DIFFDRIVERCONTROLLER_H
#include <ros/ros.h>
#include "StatusPublisher.h"
#include "AsyncSerial.h"
#include <std_msgs/Bool.h>
#include "galileo_serial_server/GalileoNativeCmds.h"
#include "galileo_serial_server/GalileoStatus.h"

namespace xqserial_server
{

typedef struct
{
    int nav_status;  //导航服务状态,0 表示没开启 closed,1 表示开启 opened 。
    int visual_status;  //视觉系统状态,0 表示没初始化 uninit,1 表示正在追踪 tracking,2表示丢失 lost,1 和 2
                        //都表示视觉系统已经初始化完成。
    int charge_status;  //充电状态，0 free 未充电状态, 1 charging 充电中, 2 charged 已充满，但仍在小电流充电, 3 finding
                        //寻找充电桩, 4 docking 停靠充电桩, 5 error 错误
    int map_status;   //0表示不在建图状态，1表示处于建图状态
    float power;       //电源电压【9 46】v 。
    int target_numID;  //当前目标点编号,默认值为-1 表示无效值。
    int target_status;  //当前目标点状态,0 表示已经到达或者取消 free,1 表示正在前往目标点过程中 working,2
                        //表示当前目标点的移动任务被暂停 paused,3 表示目标点出现错误 error, 默认值为-1 表示无效值。
    float target_distance;  //机器人距离当前目标点的距离,单位为米,-1 表示无效值,该值的绝对值小于 0.01 时表示已经到达。
    int angle_goal_status;  //目标角度达到情况,0 表示未完成,1 表示完成,2 表示 error,默认值为-1 表示无效值。
    float control_speed_x;      //导航系统计算给出的前进速度控制分量,单位为 m/s 。
    float control_speed_theta;  //导航系统计算给出的角速度控制分量,单位为 rad/s 。
    float current_speed_x;      //当前机器人实际前进速度分量,单位为 m/s 。
    float current_speed_theta;  //当前机器人实际角速度分量,单位为 rad/s 。
    unsigned int time_stamp;    //时间戳,单位为 1/30 毫秒,用于统计丢包率。
} NAV_STATUS;

class DiffDriverController
{
public:
    DiffDriverController();
    DiffDriverController(double max_speed_,std::string cmd_topic_,StatusPublisher* xq_status_,CallbackAsyncSerial* cmd_serial_car_,CallbackAsyncSerial* cmd_serial_imu_);

    void run();
    void dealCmd_vel(const geometry_msgs::Twist& command);
    void imuCalibration(const std_msgs::Bool& calFlag);
    void updateMoveFlag(const std_msgs::Bool& moveFlag);
    void updateBarDetectFlag(const std_msgs::Bool& DetectFlag);
    void updateFastStopFlag(const std_msgs::Int32& fastStopmsg);
    void send_speed();
    void filterSpeed();
    void send_stop();
    void send_fasterstop();
    void send_release();
    void UpdateNavStatus(const galileo_serial_server::GalileoStatus& current_receive_status);
    ros::WallTime last_ordertime;
    NAV_STATUS galileoStatus_;
private:
    double max_wheelspeed;//单位为转每秒,只能为正数
    std::string cmd_topic;
    StatusPublisher* xq_status;
    CallbackAsyncSerial* cmd_serial_car;
    CallbackAsyncSerial* cmd_serial_imu;
    boost::mutex mMutex;
    boost::mutex mStausMutex_;
    bool MoveFlag;
    bool BarFlag;
    bool fastStopFlag_;
    bool updateOrderflag_;
    int16_t left_speed_;
    int16_t right_speed_;
    float linear_x_;
    float theta_z_;
    bool send_flag_;
};

}
#endif // DIFFDRIVERCONTROLLER_H
