#include <shot_executer.h>


/* Shot Executer for MULTIDRONE project
/**
 * Class description:
*/

ShotExecuter::ShotExecuter(ros::NodeHandle &_nh){
    // publisher
    desired_pose_pub_ = _nh.advertise<nav_msgs::Odometry>("desired_pose",10);
    go_to_waypoint_client_ = _nh.serviceClient<uav_abstraction_layer::GoToWaypoint>("/drone_"+std::to_string(drone_id_)+"/ual/go_to_waypoint");
    take_off_srv_ = _nh.serviceClient<uav_abstraction_layer::TakeOff>("/drone_"+std::to_string(drone_id_)+"/ual/take_off");
    land_client_ = _nh.serviceClient<uav_abstraction_layer::Land>("/drone_"+std::to_string(drone_id_)+"/ual/take_off");
    
}

ShotExecuterMultidrone::ShotExecuterMultidrone(ros::NodeHandle &_nh) : ShotExecuter(_nh){
    server_ = new actionlib::SimpleActionServer<multidrone_msgs::ExecuteAction>(_nh, "action_server", false);
    server_->registerGoalCallback(boost::bind(&ShotExecuterMultidrone::actionCallback, this));
    // server_->registerPreemptCallback(boost::bind(&Executer::preemptCallback, this));
    server_->start();
}


/** \brief go to waypoint off the drone
 *  \param _height   take off's height
 *  \return success
 **/
bool ShotExecuterMultidrone::goToWaypoint(const multidrone_msgs::DroneAction &_goal){
    uav_abstraction_layer::GoToWaypoint go_to_waypoint_srv;
    geometry_msgs::PoseStamped setpoint_pose;
    multidrone_msgs::ExecuteResult result;
    int i=0;
    for(i; i<_goal.path.size();i++){
        setpoint_pose.header.stamp     = ros::Time::now();
        setpoint_pose.header.frame_id  = "map";
        setpoint_pose.pose.position.x  = _goal.path[i].point.x;
        setpoint_pose.pose.position.y  = _goal.path[i].point.y;
        setpoint_pose.pose.position.z  = _goal.path[i].point.z;
        setpoint_pose.pose.orientation = drone_pose_.pose.pose.orientation;  // the same orientation as the previous waypoint
        go_to_waypoint_srv.request.waypoint = setpoint_pose;
        go_to_waypoint_srv.request.blocking = true;
        if(go_to_waypoint_client_.call(go_to_waypoint_srv)){
            ROS_INFO("Executer %d: calling the go_to_waypoint service",drone_id_);
        }else
        {
            ROS_WARN("Executer %d: go_to_waypoint service is not available",drone_id_);
        } 
    }
    if (_goal.final_yaw_if_gotowaypoint.x==0.0 && _goal.final_yaw_if_gotowaypoint.y==0.0 && _goal.final_yaw_if_gotowaypoint.z==0.0 && _goal.final_yaw_if_gotowaypoint.w==0.0) {
        ROS_INFO("finish go to waypoint");
        result.goal_achieved=true;
        server_->setSucceeded(result);
        return true;
    }
    ROS_INFO("girando en yaw");
    setpoint_pose.pose.position.x  = _goal.path[_goal.path.size()-1].point.x;
    setpoint_pose.pose.position.y  = _goal.path[_goal.path.size()-1].point.y;
    setpoint_pose.pose.position.z  = _goal.path[_goal.path.size()-1].point.z;
    setpoint_pose.pose.orientation.x  = _goal.final_yaw_if_gotowaypoint.x;
    setpoint_pose.pose.orientation.y  = _goal.final_yaw_if_gotowaypoint.y;
    setpoint_pose.pose.orientation.z  = _goal.final_yaw_if_gotowaypoint.z;
    setpoint_pose.pose.orientation.w = _goal.final_yaw_if_gotowaypoint.w;        
    go_to_waypoint_srv.request.waypoint = setpoint_pose;
    go_to_waypoint_srv.request.blocking = true;
    result.goal_achieved = go_to_waypoint_client_.call(go_to_waypoint_srv);
    if(result.goal_achieved){
        ROS_INFO("Executer %d: calling the go_to_waypoint service",drone_id_);
    }else
    {
        ROS_WARN("Executer %d: go_to_waypoint service is not available",drone_id_);
    }
    server_->setSucceeded(result);
}

/** \brief Taking off the drone
 *  \param _height   take off's height
 *  \return success
 **/
bool ShotExecuterMultidrone::takeOff(const double _height){
    uav_abstraction_layer::TakeOff srv;
    srv.request.blocking = true;
    srv.request.height = _height;
    if(!take_off_srv_.call(srv)){
        return false;
    }else{
        return true;
    }
}
        
/** \brief target trajectory prediction
 */
std::vector<nav_msgs::Odometry> ShotExecuterMultidrone::targetTrajectoryPrediction(){

    //double target_vel_module = sqrt(pow(target_vel[0],2)+pow(target_vel[1],2));
    std::vector<nav_msgs::Odometry> target_trajectory;
    nav_msgs::Odometry aux;
    for(int i=0; i<time_horizon;i++){
        aux.pose.pose.position.x = target_pose_.pose.pose.position.x+ step_size_*i*target_pose_.twist.twist.linear.x;
        aux.pose.pose.position.y = target_pose_.pose.pose.position.y + step_size_*i*target_pose_.twist.twist.linear.y;
        target_trajectory.push_back(aux);
    }
    return target_trajectory;
}
/** \brief Shooting action thread callback
 */
void ShotExecuterMultidrone::actionThread(const multidrone_msgs::DroneAction goal){
    //duration =  goal.shooting_action.duration;
    if(goal.action_type == multidrone_msgs::DroneAction::TYPE_SHOOTING){
        //TODO predict
        std::vector<nav_msgs::Odometry> target_trajectory = targetTrajectoryPrediction();
        // calculate pose
        std::map<std::string,float> shooting_parameters;
        /*try{
            shooting_action_type = goal.shooting_action.shooting_roles.at(0).shooting_type.type;
        }
        catch(std::out_of_range o){
            ROS_ERROR("trying to shooting roles");
        }*/
        for(int i = 0; i<goal.shooting_action.shooting_roles[0].shooting_parameters.size(); i++){
            std::cout<<goal.shooting_action.shooting_roles[0].shooting_parameters[i].param<<std::endl;
            shooting_parameters[goal.shooting_action.shooting_roles[0].shooting_parameters[i].param] = goal.shooting_action.shooting_roles[0].shooting_parameters[i].value;
        }
        nav_msgs::Odometry desired_pose = calculateDesiredPoint(goal.shooting_action.shooting_roles.at(0).shooting_type.type,shooting_parameters ,target_trajectory);
        // publish desired pose


        /** if(shooting_action_running) stop_current_shooting = true;   // If still validating, end the current validation to start the new one as soon as possible.
        if(shooting_action_thread.joinable()) shooting_action_thread.join();
        shooting_action_thread = std::thread(shootingActionThread);
        return;*/
    }
    else if(goal.action_type == multidrone_msgs::DroneAction::TYPE_TAKEOFF){ // TAKE OFF NAVIGATION ACTION
        if(takeOff(goal.path[0].point.z)){
            ROS_INFO("Drone %d: taking_off",drone_id_);
            multidrone_msgs::ExecuteResult result;
            result.goal_achieved = true;
            server_->setSucceeded(result);
        }else{
            ROS_WARN("Drone %d: the take off is not available",drone_id_);
        }
    }else if(goal.action_type == multidrone_msgs::DroneAction::TYPE_LAND){ // LAND NAVIGATION ACTION

    }else if(goal.action_type == multidrone_msgs::DroneAction::TYPE_GOTOWAYPOINT){ // GO TO WAYPOINT NAVIGATION ACTION
        goToWaypoint(goal);
    }
}
    
/** \brief action callback
 *  \TODO reduce this function
 */
void ShotExecuterMultidrone::actionCallback(){
    const multidrone_msgs::DroneAction goal =server_->acceptNewGoal()->action_goal;
    action_thread_ = std::thread(&ShotExecuterMultidrone::actionThread, this, goal);
    
}
/** \brief Calculate desired pose. If type flyby, calculate wrt last mission pose .If lateral, calculate wrt time horizon pose
 *  \TODO   z position and velocity, angle relative to target
 *  \TODO   calculate orientation by velocity
 **/
nav_msgs::Odometry ShotExecuterMultidrone::calculateDesiredPoint(const int shooting_type, std::map<std::string, float> shooting_parameters, const std::vector<nav_msgs::Odometry> &target_trajectory){
    //int dur = (int)(shooting_duration*10);
    nav_msgs::Odometry desired_point;
    switch(shooting_type){
        //TODO
        case multidrone_msgs::ShootingType::SHOOT_TYPE_FLYBY:
            desired_point.pose.pose.position.x  = target_trajectory.back().pose.pose.position.x+(cos(-0.9)*shooting_parameters["x_e"]-sin(-0.9)*shooting_parameters["y_0"]);
            desired_point.pose.pose.position.y = target_trajectory.back().pose.pose.position.y+(sin(-0.9)*shooting_parameters["x_e"]+cos(-0.9)*shooting_parameters["y_0"]);
            desired_point.pose.pose.position.z = drone_pose_.pose.pose.position.z;

            // desired vel
            desired_point.twist.twist.linear.x =target_trajectory.back().twist.twist.linear.x;
            desired_point.twist.twist.linear.y =target_trajectory.back().twist.twist.linear.y;
            desired_point.twist.twist.linear.z =0;
        break;
        case multidrone_msgs::ShootingType::SHOOT_TYPE_LATERAL:
            desired_point.pose.pose.position.x  = target_trajectory[time_horizon-1].pose.pose.position.x-sin(-0.9)*shooting_parameters["y_0"];
            desired_point.pose.pose.position.y = target_trajectory[time_horizon-1].pose.pose.position.y+cos(-0.9)*shooting_parameters["y_0"];
            desired_point.pose.pose.position.z  = drone_pose_.pose.pose.position.z;

            // desired
            desired_point.twist.twist.linear.x =target_trajectory[time_horizon-1].twist.twist.linear.x;
            desired_point.twist.twist.linear.y =target_trajectory[time_horizon-1].twist.twist.linear.y;
            desired_point.twist.twist.linear.z =0;
        break;
    }
}


/** \brief Shooting action thread 
 * **/
// void shootingActionThread(){



//     bool desired_point_reached = false;

//     /** \brief Utility function to calculate if the trajectory calculated by the solver finishes in the desired pose
//     */
//     bool desiredPoseReached(const double x_des, const double y_des, const double z_des, const double x_traj, const double y_traj, const double z_traj){
//         Eigen::Vector3f desired_pose = Eigen::Vector3f(x_des,y_des,z_des);
//         Eigen::Vector3f final_pose = Eigen::Vector3f(x_traj,y_traj,z_traj);
//         if((desired_pose-final_pose).norm()<2.0){
//             return true;
//         }else{
//             return false;
//         }
//     }

//     /** \brief callback for the pose of uavs
//     */

//     void uavPoseCallback(const geometry_msgs::PoseStamped::ConstPtr &msg, int id){
//         has_poses[id] = true;
//         if(!trajectory_solved_received[id]){
//             for(int i=0; i<time_horizon;i++){
//                 geometry_msgs::Point pose_aux;
//                 pose_aux.x = msg->pose.position.x;
//                 pose_aux.y = msg->pose.position.y;
//                 pose_aux.z = msg->pose.position.z;
//                 uavs_trajectory[id].push_back(pose_aux);
//             }
//         }
//         uavs_pose[id].pose = msg->pose;  
//     }

//     /** callback for ual state
//      */

//     void ualStateCallback(const uav_abstraction_layer::State::ConstPtr &msg){
//         ual_state = msg->state;
//     }
// }

// class ShotExecuterMultidrone : public ShotExecuter{
//     /** \brief thread for multidrone shooting action
//     */

//     void shootingActionThread(){
//         shooting_action_running = true;
//         ROS_INFO("Executer %d: Shooting action thread initilized",drone_id_);
//         bool duration_reached = false;
//         ros::Rate rate(solver_rate); //hz
//         /* main loop to call the solver. */
//         while(ros::ok && !duration_reached && !desired_point_reached){
//             ros::spinOnce();
//             // solver function
//             x.clear();
//             y.clear();
//             z.clear();
//             vx.clear();
//             vy.clear();
//             vz.clear();
//             solver_success = solverFunction(x,y,z,vx,vy,vz, desired_wp, desired_vel, obst,target_vel);
//             if(solver_success==1){
//                 if(shooting_action_type == multidrone_msgs::ShootingType::SHOOT_TYPE_LATERAL){
//                     desired_point_reached = desiredPoseReached(f_pose[0],f_pose[1], f_pose[2],desired_wp[0],desired_wp[1],desired_wp[2]);
//                 }else if(shooting_action_type == multidrone_msgs::ShootingType::SHOOT_TYPE_FLYBY){
//                     desired_point_reached = desiredPoseReached(f_pose[0],f_pose[1], f_pose[2],x[time_horizon-1],y[time_horizon-1],z[time_horizon-1]);
//                 }
//                 publishTrajectory(x,y,z,vx,vy,vz);

//                 // log solver output to csv file
//                 logToCsv(x,y,z,vx,vy,vz);
//                 // publish path to rviz visualizer

//                 // double point_1[2]= {-13.1,-35.55};
//                 // double point_2[2]= {-2.2,-20.8};
//                 // double point_3[2]= {10.77,-39.7};
//                 // double point_4[2]= {-2.5,-51.3};

//                 // publishNoFlyZone(point_1,point_2,point_3,point_4);

//             }
            
//             publishDesiredPoint(desired_wp[0], desired_wp[1], desired_wp[2]);

//             if(drone_id_==1){

//                 nav_msgs::Path msg;
//                 std::vector<geometry_msgs::PoseStamped> poses(target_trajectory.size());
//                 msg.header.frame_id = "map";
//                 for (int i = 0; i < target_trajectory.size(); i++) {
//                     poses.at(i).pose.position.x = target_trajectory[i].x;
//                     poses.at(i).pose.position.y = target_trajectory[i].y;
//                     poses.at(i).pose.position.z = target_trajectory[i].z;
//                     poses.at(i).pose.orientation.x = 0;
//                     poses.at(i).pose.orientation.y = 0;
//                     poses.at(i).pose.orientation.z = 0;
//                     poses.at(i).pose.orientation.w = 1;
//                 }
//                 msg.poses = poses;
//                 target_path_rviz_pub.publish(msg);
            
//             }
//             ros::spinOnce();
//             rate.sleep();
//         }

//     ROS_INFO("Executer %d: Finishing shooting actin thread",drone_id_);

//     }
// /** \brief callback for multidrone action client
//  */

//     void actionCallback(){

//         const multidrone_msgs::DroneAction goal =server_->acceptNewGoal()->action_goal;
        
//         //duration =  goal.shooting_action.duration;

//         if(goal.action_type == multidrone_msgs::DroneAction::TYPE_SHOOTING){
//             target_final_pose[0] = goal.shooting_action.rt_trajectory[goal.shooting_action.rt_trajectory.size()-1].point.x;
//             target_final_pose[1] = goal.shooting_action.rt_trajectory[goal.shooting_action.rt_trajectory.size()-1].point.y;

//             try{
//                 shooting_action_type = goal.shooting_action.shooting_roles.at(0).shooting_type.type;
//             }
//             catch(std::out_of_range o){
//                 ROS_ERROR("trying to shooting roles");
//             }
//             for(int i = 0; i<goal.shooting_action.shooting_roles[0].shooting_parameters.size(); i++){
//                 std::cout<<goal.shooting_action.shooting_roles[0].shooting_parameters[i].param<<std::endl;
//             shooting_parameters[goal.shooting_action.shooting_roles[0].shooting_parameters[i].param] = goal.shooting_action.shooting_roles[0].shooting_parameters[i].value;
//             }

//             if(shooting_action_running) stop_current_shooting = true;   // If still validating, end the current validation to start the new one as soon as possible.
//             if(shooting_action_thread.joinable()) shooting_action_thread.join();
//             shooting_action_thread = std::thread(shootingActionThread);
//             return;
//         }
//         /*else if(goal.action_type == multidrone_msgs::DroneAction::TYPE_TAKEOFF){ // TAKE OFF NAVIGATION ACTION
//             // taking off
//             uav_abstraction_layer::TakeOff srv;
//             srv.request.blocking = true;
//             srv.request.height = goal.path[0].point.z;
//             if(!take_off_srv.call(srv)){
//                 ROS_WARN("Drone %d: the take off is not available",drone_id_);
                
//             }else{
//                 ROS_INFO("Drone %d: taking_off",drone_id_);
//                 multidrone_msgs::ExecuteResult result;
//                 result.goal_achieved = true;
//                 server_->setSucceeded(result);
//             }
//         }else if(goal.action_type == multidrone_msgs::DroneAction::TYPE_LAND){ // LAND NAVIGATION ACTION

//         }else if(goal.action_type == multidrone_msgs::DroneAction::TYPE_GOTOWAYPOINT){ // GO TO WAYPOINT NAVIGATION ACTION
//             uav_abstraction_layer::GoToWaypoint go_to_waypoint_srv;
//             geometry_msgs::PoseStamped setpoint_pose;
//             multidrone_msgs::ExecuteResult result;
//             int i=0;
//             for(i; i<goal.path.size();i++){
//                 setpoint_pose.header.stamp     = ros::Time::now();
//                 setpoint_pose.header.frame_id  = "map";
//                 setpoint_pose.pose.position.x  = goal.path[i].point.x;
//                 setpoint_pose.pose.position.y  = goal.path[i].point.y;
//                 setpoint_pose.pose.position.z  = goal.path[i].point.z;
//                 setpoint_pose.pose.orientation = uavs_pose[drone_id_].pose.orientation;  // the same orientation as the previous waypoint
//                 go_to_waypoint_srv.request.waypoint = setpoint_pose;
//                 go_to_waypoint_srv.request.blocking = true;
//                 if(go_to_waypoint_client.call(go_to_waypoint_srv)){
//                     ROS_INFO("Executer %d: calling the go_to_waypoint service",drone_id_);
//                 }else
//                 {
//                     ROS_WARN("Executer %d: go_to_waypoint service is not available",drone_id_);
//                 } 
//             }

//             if (goal.final_yaw_if_gotowaypoint.x==0.0 && goal.final_yaw_if_gotowaypoint.y==0.0 && goal.final_yaw_if_gotowaypoint.z==0.0 && goal.final_yaw_if_gotowaypoint.w==0.0) {
//                 ROS_INFO("finish go to waypoint");
//                 result.goal_achieved=true;
//                 server_->setSucceeded(result);
//                 return;
//             }
//             ROS_INFO("girando en yaw");
//             setpoint_pose.pose.position.x  = goal.path[goal.path.size()-1].point.x;
//             setpoint_pose.pose.position.y  = goal.path[goal.path.size()-1].point.y;
//             setpoint_pose.pose.position.z  = goal.path[goal.path.size()-1].point.z;
//             setpoint_pose.pose.orientation.x  = goal.final_yaw_if_gotowaypoint.x;
//             setpoint_pose.pose.orientation.y  = goal.final_yaw_if_gotowaypoint.y;
//             setpoint_pose.pose.orientation.z  = goal.final_yaw_if_gotowaypoint.z;
//             setpoint_pose.pose.orientation.w = goal.final_yaw_if_gotowaypoint.w;        
//             go_to_waypoint_srv.request.waypoint = setpoint_pose;
//             go_to_waypoint_srv.request.blocking = true;
//             result.goal_achieved = go_to_waypoint_client.call(go_to_waypoint_srv);
//             if(result.goal_achieved){
//                 ROS_INFO("Executer %d: calling the go_to_waypoint service",drone_id_);
//             }else
//             {
//                 ROS_WARN("Executer %d: go_to_waypoint service is not available",drone_id_);
//             }
//             server_->setSucceeded(result);

//         }
//         */
//     }
// }

// /**
//  */




// /** Callback for the target pose
//  */

// void targetPoseCallback(const nav_msgs::Odometry::ConstPtr &msg)
// {   
//     if(has_poses[0] == false){
//         target_init.pose = msg->pose.pose;
//     }
//     has_poses[0] = true;
//     target_pose.pose = msg->pose.pose;
// }

