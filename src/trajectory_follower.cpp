#include <ros/ros.h>
#include <optimal_control_interface/SolvedTrajectory.h>
#include <geometry_msgs/Point.h>
#include <Eigen/Eigen>
#include <geometry_msgs/TwistStamped.h>
#include <geometry_msgs/PoseStamped.h>
#include <uav_abstraction_layer/TakeOff.h>
#include <uav_abstraction_layer/GoToWaypoint.h>


std::vector<geometry_msgs::Point> velocities;
std::vector<geometry_msgs::Point> positions;
Eigen::Vector3f current_pose;
const double look_ahead = 1.0;
ros::ServiceClient go_to_waypoint_client;
int pose_on_path;
int target_pose;
int drone_id = 1;

/** \brief Calback for ual pose
 */
void ualPoseCallback(const geometry_msgs::PoseStamped::ConstPtr &msg){
    current_pose = Eigen::Vector3f(msg->pose.position.x,msg->pose.position.y,msg->pose.position.z);
}

/** \brief Callback for trayectory to follow
 */
void trajectoryCallback(const optimal_control_interface::SolvedTrajectory::ConstPtr &msg){
    ROS_INFO("Drone %d: trajectory received", drone_id);
    for(int i =0; i<msg->positions.size();i++){
        positions.push_back(msg->positions[i]);
        velocities.push_back(msg->velocities[i]);
    }
}

/** \brief Utility function to calculate the nearest pose on the path
 *  \param positions a path to follow
 *  \return index of the nearest pose on the path
 */
int cal_pose_on_path(const std::vector<geometry_msgs::Point> &positions){
    double min_distance = 10000000;
    int pose_on_path_id = 0;
    for(int i=0; i<positions.size();i++){
        Eigen::Vector3f pose_on_path = Eigen::Vector3f(positions[i].x, positions[i].y, positions[i].z);
        if((current_pose - pose_on_path).norm()<min_distance){
            min_distance = (current_pose - pose_on_path).norm();
            pose_on_path_id = i;
        }
    }
    return pose_on_path_id;
}

/** \brief Utility function to calculate the look ahead position
 *  \param positions a path to follow
 *  \param look_ahead
 *  \pose_on_path pose from which we apply look ahead
 *  \return look ahead position index
 */

int cal_pose_look_ahead(const std::vector<geometry_msgs::Point> &positions, const double look_ahead, int pose_on_path){
    for(int i = pose_on_path; i<positions.size();i++){
        Eigen::Vector3f aux = Eigen::Vector3f(positions[i].x-positions[pose_on_path].x, positions[i].y-positions[pose_on_path].y, positions[i].z-positions[pose_on_path].z);
        double distance = aux.norm();
        if(distance>look_ahead) return i;
    }
    return positions.size();
}

/** \brief utility function to calculate velocity commands. This function apply the direction to the next point of the trajectory and the velocity of the nearest point of the trajectory.
 *  \param pose desired position of the path
 *  \param vel desired velocity of the nearest pose on the path
 *  \return 3d vector velocity to command
 */
Eigen::Vector3f calculate_vel(Eigen::Vector3f pose, Eigen::Vector3f vel){
   Eigen::Vector3f vel_to_command = (pose - current_pose).normalized();
   double vel_module = vel.norm();
   return vel_to_command*vel_module;
}



int main(int _argc, char **_argv)
{

    ros::init(_argc, _argv, "trajectory_follower_node");
    ros::NodeHandle nh;
    ros::Subscriber trajectory_sub = nh.subscribe<optimal_control_interface::SolvedTrajectory>("solver", 1, trajectoryCallback);
    ros::Subscriber ual_pose_sub = nh.subscribe<geometry_msgs::PoseStamped>("ual/pose", 1, ualPoseCallback);
    ros::Publisher velocity_ual_pub = nh.advertise<geometry_msgs::TwistStamped>("ual/set_velocity",1);
    double control_rate = 10.0;
    double height_take_off = 3.0;
    //nh.param<float>("control_rate", control_rate, 1.0);
    //nh.param<float>("height_take_off", height_take_off, 4.0);

    const int hover_final_pose = 5;


    while(ros::ok){
        ROS_INFO("Drone %d: waiting for trajectory. Pose on path: %d",drone_id,pose_on_path);
        //wait for receiving trajectories
        while(!positions.empty() && !velocities.empty()){
            pose_on_path = cal_pose_on_path(positions);
            ROS_INFO("pose on path: %d", pose_on_path);
            target_pose = cal_pose_look_ahead(positions,look_ahead, pose_on_path);
            // if the point to go is out of the trajectory, the trajectory will be finished and cleared
            if(target_pose==positions.size()){
                ROS_INFO("Drone %d: end of the trajectory",drone_id);
                positions.clear();
                velocities.clear();
                pose_on_path = 0;
                break;
            }
            ROS_INFO("look ahead: %d",target_pose);
            Eigen::Vector3f pose_to_go =Eigen::Vector3f(positions[target_pose].x,positions[target_pose].y, positions[target_pose].z);
            Eigen::Vector3f vel_to_go= Eigen::Vector3f(velocities[pose_on_path].x,velocities[pose_on_path].y, velocities[pose_on_path].z);
            Eigen::Vector3f velocity_to_command = calculate_vel(pose_to_go, vel_to_go);
            // publish topic to ual
            geometry_msgs::TwistStamped vel;
            vel.twist.linear.x = velocity_to_command.x();
            vel.twist.linear.y = velocity_to_command.y();
            vel.twist.linear.z = velocity_to_command.z();
            velocity_ual_pub.publish(vel);
            ros::spinOnce();
        }
        ros::spinOnce();
        sleep(1);
    }

    return 0;
}

// cada vez que una trayectoria sea recibida, resetear