/**
 * @file
 * @brief 3D reconstruction using the output of the stereo_slam (presentation).
 */

#ifndef BASE_H
#define BASE_H

#define PCL_NO_PRECOMPILE
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <ros/ros.h>
#include <pcl_ros/point_cloud.h>
#include <tf/transform_broadcaster.h>
#include <pcl/kdtree/kdtree_flann.h>
#include "../../common/tools.h"

using namespace std;
using namespace tools;

// Custom point type XYZ with the weight property
struct PointXYZRGBW
{
  float x;
  float y;
  float z;
  float rgb;
  float w;
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW;   // make sure our new allocators are aligned

  // Default values
  PointXYZRGBW () {
    x = 0.0;
    y = 0.0;
    z = 0.0;
    rgb = 0.0;
    w = 0.0;
  }
} EIGEN_ALIGN16;                    // enforce SSE padding for correct memory alignment
POINT_CLOUD_REGISTER_POINT_STRUCT (PointXYZRGBW,
                                   (float, x, x)
                                   (float, y, y)
                                   (float, z, z)
                                   (float, rgb, rgb)
                                   (float, w, w)
);

// Defines
typedef pcl::PointXY                      PointXY;
typedef pcl::PointXYZ                     PointXYZ;
typedef pcl::PointXYZRGB                  PointRGB;
typedef pcl::PointCloud<PointXY>          PointCloudXY;
typedef pcl::PointCloud<PointXYZ>         PointCloudXYZ;
typedef pcl::PointCloud<PointRGB>         PointCloudRGB;
typedef pcl::PointCloud<PointXYZRGBW>     PointCloudXYZW;

namespace reconstruction
{

class ReconstructionBase
{

public:

	// Constructor
  ReconstructionBase(ros::NodeHandle nh, ros::NodeHandle nhp);

  struct Params
  {
    string work_dir;              //!> Working directory.
    string clouds_dir;            //!> Directory where stereo_slam pointclouds are saved.
    string graph_file;            //!> stereo_slam output graph file.
    string output_dir;            //!> Output directory where the files will be stored.

    // Default settings
    Params () {
      work_dir                    = "";
      clouds_dir                  = "";
      graph_file                  = "";
      output_dir                  = "";
    }
  };

  /**
   * @param params new parameters
   */
  inline void setParams(const Params& params)
  {
    params_ = params;
  }

  /**
   * @return current parameters
   */
  inline Params params() const { return params_; }

  // Set the parameters
  void setParameters(string work_dir);

  // 3D reconstruction
  void build3D();

  // Greedy projection
  pcl::PolygonMesh::Ptr greedyProjection(PointCloudRGB::Ptr cloud);

  // Filter
  PointCloudRGB::Ptr filter(PointCloudRGB::Ptr in_cloud, float voxel_size);

  // Get contour
  PointCloudXY::Ptr getContourXY(PointCloudXYZW::Ptr acc, float voxel_size);

  // Color blending
  float colorBlending(float color_a, float color_b, float alpha);

protected:

	// Node handlers
	ros::NodeHandle nh_;
  ros::NodeHandle nh_private_;

private:

  Params params_;                   //!> Stores parameters
};

} // namespace

#endif // BASE_H