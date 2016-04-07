/********************************************************************************
Copyright (c) 2016, TRACLabs, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, 
       this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation 
       and/or other materials provided with the distribution.

    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software 
       without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************************/

#include <boost/date_time.hpp>
#include <trac_ik/trac_ik.hpp>
#include <ros/ros.h>
#include <kdl/chainiksolverpos_nr_jl.hpp>
#include <geometry_msgs/PoseStamped.h>
#include <kdl_conversions/kdl_msg.h>
#include <baxter_core_msgs/JointCommand.h>

std::string chain_start, chain_end, urdf_param;
double timeout = 0.005;
double eps = 1e-5;
ros::Publisher jointCommandPublisher;

void findIKSolution(const KDL::Frame &end_effector_pose, const std::string &limbName)
{

  // This constructor parses the URDF loaded in rosparm urdf_param into the
  // needed KDL structures.  We then pull these out to compare against the KDL
  // IK solver.
  TRAC_IK::TRAC_IK tracik_solver(chain_start, limbName + chain_end, urdf_param, timeout, eps);

  KDL::Chain chain;
  KDL::JntArray ll, ul; //lower joint limits, upper joint limits

  bool valid = tracik_solver.getKDLChain(chain);
  
  if (!valid) {
    ROS_ERROR("There was no valid KDL chain found");
    return;
  }

  valid = tracik_solver.getKDLLimits(ll,ul);

  if (!valid) {
    ROS_ERROR("There were no valid KDL joint limits found");
    return;
  }

  assert(chain.getNrOfJoints() == ll.data.size());
  assert(chain.getNrOfJoints() == ul.data.size());

  ROS_INFO ("Using %d joints",chain.getNrOfJoints());


  // Create Nominal chain configuration midway between all joint limits
  KDL::JntArray nominal(chain.getNrOfJoints());

  for (uint j=0; j<nominal.data.size(); j++) {
    nominal(j) = (ll(j)+ul(j))/2.0;
  }

  KDL::JntArray result;

  int rc;

  rc=tracik_solver.CartToJnt(nominal,end_effector_pose,result);
  ROS_INFO("Found %d solution", rc);

  if (rc > 0) {

      std::string jointNamesArray[] = {limbName + "_s0", limbName + "_s1", limbName + "_e0", limbName + "_e1",
          limbName + "_w0", limbName + "_w1", limbName + "_w2"};
      std::vector<std::string> jointNamesVector(chain.getNrOfJoints());
      for( std::size_t j = 0; j < chain.getNrOfJoints(); ++j)
          jointNamesVector[j] = jointNamesArray[j];
      baxter_core_msgs::JointCommand jointCommand;
      jointCommand.mode = 1;
      jointCommand.names = jointNamesVector;
      jointCommand.command.resize(chain.getNrOfJoints());
      for( std::size_t j = 0; j < chain.getNrOfJoints(); ++j)
          jointCommand.command[j] = result(j);
      jointCommandPublisher.publish(jointCommand);
  }
}

void callBack(const geometry_msgs::PoseStamped &callBackData) {
  ROS_INFO("Callbacking...");
  KDL::Frame end_effector_pose;
  tf::poseMsgToKDL(callBackData.pose, end_effector_pose);
  if (callBackData.header.frame_id.find("right") != std::string::npos) {
    findIKSolution(end_effector_pose, "right");
  } else {
    findIKSolution(end_effector_pose, "left");
  }
}

int main(int argc, char** argv)
{
  srand(1);
  ros::init(argc, argv, "trac_ik_test");
  ros::NodeHandle nodeHandle;

  //nodeHandle.param("chain_start", chain_start, std::string("left_arm_mount"));
  //nodeHandle.param("chain_end", chain_end, std::string("left_gripper_base"));
  //nodeHandle.param("urdf_param", urdf_param, std::string("/robot_description"));
  chain_start = "base";
  chain_end = "_gripper_base";
  urdf_param = "/robot_description";
  if (chain_start=="" || chain_end=="") {
    ROS_FATAL("Missing chain info in launch file");
    exit (-1);
  }



  ros::Subscriber subscriber = nodeHandle.subscribe("end_effector_command_position", 1, callBack);

  jointCommandPublisher = nodeHandle.advertise<baxter_core_msgs::JointCommand>("end_effector_command_solution", 1);

  ros::spin();
  return 0;
}
