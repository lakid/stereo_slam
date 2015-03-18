#ifndef TOOLS
#define TOOLS

#include <ros/ros.h>
#include <pcl/common/common.h>
#include <image_geometry/stereo_camera_model.h>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/Image.h>
#include <stereo_msgs/DisparityImage.h>
#include <cv_bridge/cv_bridge.h>
#include <nav_msgs/Odometry.h>
#include <boost/filesystem.hpp>
#include "../localization/vertex.h"

namespace enc = sensor_msgs::image_encodings;
namespace fs  = boost::filesystem;

using namespace std;

namespace tools
{

class Tools
{

public:

  // Definitions
  typedef pcl::PointXYZRGB        Point;
  typedef pcl::PointCloud<Point>  PointCloud;


  /** \brief convert a tf::transform to Eigen::Isometry3d
    * @return Eigen::Isometry3d matrix
    * \param in of type tf::transform
    */
  static Eigen::Isometry3d tfToIsometry(tf::Transform in)
  {
    tf::Vector3 t_in = in.getOrigin();
    tf::Quaternion q_in = in.getRotation();
    Eigen::Vector3d t_out(t_in.x(), t_in.y(), t_in.z());
    Eigen::Quaterniond q_out;
    q_out.setIdentity();
    q_out.x() = q_in.x();
    q_out.y() = q_in.y();
    q_out.z() = q_in.z();
    q_out.w() = q_in.w();
    Eigen::Isometry3d out = (Eigen::Isometry3d)q_out;
    out.translation() = t_out;
    return out;
  }

  /** \brief convert a tf::transform to Eigen::Matrix4f
    * @return Eigen::Matrix4f matrix
    * \param in of type tf::transform
    */
  static Eigen::Matrix4f tfToMatrix4f(tf::Transform in)
  {
    Eigen::Matrix4f out;

    tf::Vector3 t_in  = in.getOrigin();
    tf::Matrix3x3 rot = in.getBasis();
    tf::Vector3 r0    = rot.getRow(0);
    tf::Vector3 r1    = rot.getRow(1);
    tf::Vector3 r2    = rot.getRow(2);

    out << r0.x(), r0.y(), r0.z(), t_in.x(),
           r1.x(), r1.y(), r1.z(), t_in.y(),
           r2.x(), r2.y(), r2.z(), t_in.z(),
           0,     0,     0,     1;

    return out;
  }

  /** \brief convert a Eigen::Isometry3d to tf::transform
    * @return tf::transform matrix
    * \param in of type Eigen::Isometry3d
    */
  static tf::Transform isometryToTf(Eigen::Isometry3d in)
  {
    Eigen::Vector3d t_in = in.translation();
    Eigen::Quaterniond q_in = (Eigen::Quaterniond)in.rotation();
    tf::Vector3 t_out(t_in.x(), t_in.y(), t_in.z());
    tf::Quaternion q_out(q_in.x(), q_in.y(), q_in.z(), q_in.w());
    tf::Transform out(q_out, t_out);
    return out;
  }

  /** \brief convert a Eigen::Matrix4f to tf::transform
    * @return tf::transform matrix
    * \param in of type Eigen::Matrix4f
    */
  static tf::Transform matrix4fToTf(Eigen::Matrix4f in)
  {
    tf::Vector3 t_out;
    t_out.setValue(static_cast<double>(in(0,3)),static_cast<double>(in(1,3)),static_cast<double>(in(2,3)));

    tf::Matrix3x3 tf3d;
    tf3d.setValue(static_cast<double>(in(0,0)), static_cast<double>(in(0,1)), static_cast<double>(in(0,2)),
                  static_cast<double>(in(1,0)), static_cast<double>(in(1,1)), static_cast<double>(in(1,2)),
                  static_cast<double>(in(2,0)), static_cast<double>(in(2,1)), static_cast<double>(in(2,2)));

    tf::Quaternion q_out;
    tf3d.getRotation(q_out);
    tf::Transform out(q_out, t_out);
    return out;
  }

  /** \brief Convert odometry message to tf::Transform
    * @return the trasnformation matrix
    * \param rvec cv matrix with the rotation angles
    * \param tvec cv matrix with the transformation x y z
    */
  static tf::Transform odomTotf(nav_msgs::Odometry odom_msg)
  {
    // Get the data
    double tx = odom_msg.pose.pose.position.x;
    double ty = odom_msg.pose.pose.position.y;
    double tz = odom_msg.pose.pose.position.z;

    double qx = odom_msg.pose.pose.orientation.x;
    double qy = odom_msg.pose.pose.orientation.y;
    double qz = odom_msg.pose.pose.orientation.z;
    double qw = odom_msg.pose.pose.orientation.w;

    // Sanity check
    if(qx == 0.0 && qy == 0.0 && qz == 0.0 && qw == 0.0)
    {
      tf::Transform odom;
      odom.setIdentity();
      return odom;
    }
    else
    {
      tf::Vector3 tf_trans(tx, ty, tz);
      tf::Quaternion tf_q (qx, qy, qz, qw);
      tf::Transform odom(tf_q, tf_trans);
      return odom;
    }
  }

  /** \brief Convert image messages to cv Mat
    * \param left image message.
    * \param right image message.
    * \param will contain the output left cv::Mat.
    * \param will contain the output right cv::Mat.
    */
  static bool imgMsgToMat(sensor_msgs::Image l_img_msg,
                          sensor_msgs::Image r_img_msg,
                          cv::Mat &l_img, cv::Mat &r_img)
  {
    // Convert message to Mat
    cv_bridge::CvImagePtr l_img_ptr, r_img_ptr;
    try
    {
      l_img_ptr = cv_bridge::toCvCopy(l_img_msg, enc::BGR8);
      r_img_ptr = cv_bridge::toCvCopy(r_img_msg, enc::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
      ROS_ERROR("[StereoSlam:] cv_bridge exception: %s", e.what());
      return false;
    }

    // Set the images
    l_img = l_img_ptr->image;
    r_img = r_img_ptr->image;
    return true;
  }

  /** \brief Get the stereo camera model from info messages
    * \param left image info message.
    * \param right image info message.
    * \param will contain the output camera model.
    * \param will contain the output camera matrix.
    */
  static bool getCameraModel(sensor_msgs::CameraInfo l_info_msg,
                             sensor_msgs::CameraInfo r_info_msg,
                             image_geometry::StereoCameraModel &stereo_camera_model,
                             cv::Mat &camera_matrix)
  {
    // Get the binning factors
    int binning_x = l_info_msg.binning_x;
    int binning_y = l_info_msg.binning_y;

    // Get the stereo camera model
    stereo_camera_model.fromCameraInfo(l_info_msg, r_info_msg);

    // Get the projection/camera matrix
    const cv::Mat P(3,4, CV_64FC1, const_cast<double*>(l_info_msg.P.data()));
    camera_matrix = P.colRange(cv::Range(0,3)).clone();

    // Are the images scaled?
    if (binning_x > 1 || binning_y > 1)
    {
      camera_matrix.at<double>(0,0) = camera_matrix.at<double>(0,0) / binning_x;
      camera_matrix.at<double>(0,2) = camera_matrix.at<double>(0,2) / binning_x;
      camera_matrix.at<double>(1,1) = camera_matrix.at<double>(1,1) / binning_y;
      camera_matrix.at<double>(1,2) = camera_matrix.at<double>(1,2) / binning_y;
    }
  }

  /** \brief get the pose of vertex in format tf::Transform
    * @return tf::Transform pose matrix
    * \param vertex of type Vertex
    */
  static tf::Transform getVertexPose(slam::Vertex* v)
  {
    Eigen::Isometry3d pose_eigen = v->estimate();
    tf::Transform pose_tf = tools::Tools::isometryToTf(pose_eigen);
    return pose_tf;
  }

  /** \brief compute the absolute diference between 2 poses
    * @return the norm between two poses
    * \param pose_1 transformation matrix of pose 1
    * \param pose_2 transformation matrix of pose 2
    */
  static double poseDiff(tf::Transform pose_1, tf::Transform pose_2)
  {
    tf::Vector3 d = pose_1.getOrigin() - pose_2.getOrigin();
    return sqrt(d.x()*d.x() + d.y()*d.y() + d.z()*d.z());
  }

  /** \brief Sort 2 pairs by size
    * @return true if pair 1 is smaller than pair 2
    * \param pair 1
    * \param pair 2
    */
  static bool sortByDistance(const pair<int, double> d1, const pair<int, double> d2)
  {
    return (d1.second < d2.second);
  }

  /** \brief Convert isometry to string (useful for debugin purposes)
    * @return string containing the isometry
    * \param isometry matrix
    */
  static string isometryToString(Eigen::Isometry3d m)
  {
    char result[80];
    memset(result, 0, sizeof(result));
    Eigen::Vector3d xyz = m.translation();
    Eigen::Vector3d rpy = m.rotation().eulerAngles(0, 1, 2);
    snprintf(result, 79, "%6.2f %6.2f %6.2f %6.2f %6.2f %6.2f",
        xyz(0), xyz(1), xyz(2),
        rpy(0) * 180/M_PI, rpy(1) * 180/M_PI, rpy(2) * 180/M_PI);
    return string(result);
  }

  /** \brief Show a tf in the console (useful for debugin purposes)
    * @return
    * \param tf matrix
    */
  static void showTf(tf::Transform input)
  {
    tf::Vector3 tran = input.getOrigin();
    tf::Matrix3x3 rot = input.getBasis();
    tf::Vector3 r0 = rot.getRow(0);
    tf::Vector3 r1 = rot.getRow(1);
    tf::Vector3 r2 = rot.getRow(2);
    ROS_INFO_STREAM("[StereoSlam:]\n" << r0.x() << ", " << r0.y() << ", " << r0.z() << ", " << tran.x() <<
                    "\n" << r1.x() << ", " << r1.y() << ", " << r1.z() << ", " << tran.y() <<
                    "\n" << r2.x() << ", " << r2.y() << ", " << r2.z() << ", " << tran.z());
  }

  static void readPoses(string work_dir, vector< pair<string, tf::Transform> > &cloud_poses)
  {
    // Init
    cloud_poses.clear();

    string graph_file = work_dir + "graph_vertices.txt";
    string lock_file = work_dir + ".graph.lock";

    // Wait until poses file is unlocked
    while(fs::exists(lock_file));

    // Create a locking element
    fstream f_lock(lock_file.c_str(), ios::out | ios::trunc);

    // Get the pointcloud poses file
    ifstream poses_file(graph_file.c_str());
    string line;
    while (getline(poses_file, line))
    {
      int i = 0;
      string cloud_name, value;
      double x, y, z, qx, qy, qz, qw;
      istringstream ss(line);
      while(getline(ss, value, ','))
      {
        if (i == 1)
          cloud_name = value;
        else if (i == 5)
          x = boost::lexical_cast<double>(value);
        else if (i == 6)
          y = boost::lexical_cast<double>(value);
        else if (i == 7)
          z = boost::lexical_cast<double>(value);
        else if (i == 8)
          qx = boost::lexical_cast<double>(value);
        else if (i == 9)
          qy = boost::lexical_cast<double>(value);
        else if (i == 10)
          qz = boost::lexical_cast<double>(value);
        else if (i == 11)
          qw = boost::lexical_cast<double>(value);
        i++;
      }
      // Build the pair and save
      tf::Vector3 t(x, y, z);
      tf::Quaternion q(qx, qy, qz, qw);
      tf::Transform transf(q, t);
      cloud_poses.push_back(make_pair(cloud_name, transf));
    }

    // Un-lock
    f_lock.close();
    int ret_code = remove(lock_file.c_str());
    if (ret_code != 0)
    {
      ROS_ERROR("[StereoSlam:] Error deleting the locking file.");
    }
  }
};

} // namespace

#endif // TOOLS