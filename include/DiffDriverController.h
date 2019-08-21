#ifndef DIFFDRIVERCONTROLLER_H
#define DIFFDRIVERCONTROLLER_H
#include <ros/ros.h>
#include "StatusPublisher.h"
#include "AsyncSerial.h"
#include <std_msgs/Bool.h>

namespace xqserial_server
{

class DiffDriverController
{
  public:
    DiffDriverController();
    DiffDriverController(double max_speed_, std::string cmd_topic_, StatusPublisher *xq_status_,CallbackAsyncSerial* cmd_serial_left_,CallbackAsyncSerial* cmd_serial_right_);
    void run();
    void sendcmd(const geometry_msgs::Twist &command);
    void sendcmd();
    void imuCalibration(const std_msgs::Bool &calFlag);
    void setStatusPtr(StatusPublisher &status);
    void updateMoveFlag(const std_msgs::Bool &moveFlag);
    void updateBarDetectFlag(const std_msgs::Bool &DetectFlag);
    void checkStop();
  private:
    double max_wheelspeed; //单位为转每秒,只能为正数
    std::string cmd_topic;
    StatusPublisher *xq_status;
    CallbackAsyncSerial* cmd_serial_left;
    CallbackAsyncSerial* cmd_serial_right;
    boost::mutex mMutex;
    bool MoveFlag;
    bool DetectFlag_;
    bool barFlag_;
    float cmd_x_;
    float cmd_y_;
    float cmd_theta_;
};

} // namespace xqserial_server
#endif // DIFFDRIVERCONTROLLER_H
