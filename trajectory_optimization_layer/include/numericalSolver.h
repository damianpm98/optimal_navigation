#include <vector>
#include <acado_optimal_control.hpp>
#include <acado/acado_gnuplot.hpp>
#include <acado_toolkit.hpp>
#include <acado/acado_optimal_control.hpp>
#include <ACADO.h>
#include <chrono>
#include <acado/utils/acado_utils.hpp>

#define TIME_HORIZON 40

namespace NumericalSolver{

class NumericalSolver{

private:

    const float CAMERA_PITCH = 0.1;
    const float Z_RELATIVE_TARGET_DRONE = 1.5;
    float solving_rate_; // solving rate (s)
    const float t_start = 0.0;
    const int drone_id_ = 1;
    const float t_end = 7.8; 
    const int N = TIME_HORIZON;
    const bool no_fly_zone = false;
    const bool debug = true;
    std::vector<int> priority;
    ///////// solver params /////////
    const double step_size = 0.2; // seg
    const int n_states_variables = 9;
    const float hovering_distance = 0.5;
    const int npar = 10;
    const float height = 1.7;

public:

    NumericalSolver();
    virtual int solverFunction(std::map<std::string, std::array<double,TIME_HORIZON>> &_initial_guess,std::array<double,TIME_HORIZON> &_ax, std::array<double,TIME_HORIZON> &_ay, std::array<double,TIME_HORIZON> &_az,std::array<double,TIME_HORIZON> &_x, std::array<double,TIME_HORIZON> &_y, std::array<double,TIME_HORIZON> &_z, std::array<double,TIME_HORIZON> &_vx, std::array<double,TIME_HORIZON> &_vy, std::array<double,TIME_HORIZON> &_vz,nav_msgs::Odometry &_desired_odometry, const std::vector<float> &_obst, const std::vector<nav_msgs::Odometry> &_target_trajectory, std::map<int,nav_msgs::Odometry> &_uavs_pose, float time_initial_position = 0, bool first_time_solving = true, const int _drone_id = 1, const bool _target = true, const bool _multi = false);



};


class ACADOSolver : public NumericalSolver{

private:

public:
    ACADOSolver(const float solving_rate);

    /** \brief This function fill the solver inputs and call it
    *  \param x y z vx vy vz       These are the variables where the calculated path will place
    *  \param desired_pose         Desired position
    *  \param obst                 No fly zone
    *  \param target_vel           [target_vx target_vy targe_vz] We guess velocity constant target
    *  \TODO m                     manage priorities by drones (ID)
    */
    int solverFunction(std::map<std::string, std::array<double,TIME_HORIZON>> &_initial_guess,std::array<double,TIME_HORIZON> &_ax, std::array<double,TIME_HORIZON> &_ay, std::array<double,TIME_HORIZON> &_az,std::array<double,TIME_HORIZON> &_x, std::array<double,TIME_HORIZON> &_y, std::array<double,TIME_HORIZON> &_z, std::array<double,TIME_HORIZON> &_vx, std::array<double,TIME_HORIZON> &_vy, std::array<double,TIME_HORIZON> &_vz,nav_msgs::Odometry &_desired_odometry, const std::vector<float> &_obst, const std::vector<nav_msgs::Odometry> &_target_trajectory, std::map<int,nav_msgs::Odometry> &_uavs_pose, float time_initial_position = 0, bool first_time_solving = true, const int _drone_id = 1, const bool _target = true, const bool _multi = false);

};

class FORCESProSolver : public NumericalSolver{


};

}