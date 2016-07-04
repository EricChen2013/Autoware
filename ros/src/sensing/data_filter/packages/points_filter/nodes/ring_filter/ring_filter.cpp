/*
 *  Copyright (c) 2015, Nagoya University
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of Autoware nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>

#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/filters/voxel_grid.h>

#include <velodyne_pointcloud/point_types.h>

#include <runtime_manager/ConfigRingFilter.h>

#include <points_filter/PointsFilterInfo.h>

ros::Publisher filtered_points_pub;

// Leaf size of VoxelGrid filter.
static double voxel_leaf_size = 2.0;

int ring_max = 0;
int ring_div = 3;

static ros::Publisher points_filter_info_pub;
static points_filter::PointsFilterInfo points_filter_info_msg;

static void config_callback(const runtime_manager::ConfigRingFilter::ConstPtr& input)
{
  ring_div = input->ring_div;
  voxel_leaf_size = input->voxel_leaf_size;
}

static void scan_callback(const sensor_msgs::PointCloud2::ConstPtr& input)
{
  pcl::PointXYZI p;
  pcl::PointCloud<pcl::PointXYZI> scan;
  pcl::PointCloud<velodyne_pointcloud::PointXYZIR> tmp;
  sensor_msgs::PointCloud2 filtered_msg;

  pcl::fromROSMsg(*input, scan);
  pcl::fromROSMsg(*input, tmp);

  scan.points.clear();

  for (pcl::PointCloud<velodyne_pointcloud::PointXYZIR>::const_iterator item = tmp.begin(); item != tmp.end(); item++)
  {
    p.x = (double)item->x;
    p.y = (double)item->y;
    p.z = (double)item->z;
    p.intensity = (double)item->intensity;
    if (item->ring % ring_div == 0)
    {
      scan.points.push_back(p);
    }
    if (item->ring > ring_max)
    {
      ring_max = item->ring;
    }
  }

  pcl::PointCloud<pcl::PointXYZI>::Ptr scan_ptr(new pcl::PointCloud<pcl::PointXYZI>(scan));
  pcl::PointCloud<pcl::PointXYZI>::Ptr filtered_scan_ptr(new pcl::PointCloud<pcl::PointXYZI>());

  // if voxel_leaf_size < 0.1 voxel_grid_filter cannot down sample (It is specification in PCL)
  if (voxel_leaf_size >= 0.1)
  {
    // Downsampling the velodyne scan using VoxelGrid filter
    pcl::VoxelGrid<pcl::PointXYZI> voxel_grid_filter;
    voxel_grid_filter.setLeafSize(voxel_leaf_size, voxel_leaf_size, voxel_leaf_size);
    voxel_grid_filter.setInputCloud(scan_ptr);
    voxel_grid_filter.filter(*filtered_scan_ptr);

    pcl::toROSMsg(*filtered_scan_ptr, filtered_msg);
  }
  else
  {
    pcl::toROSMsg(*scan_ptr, filtered_msg);
  }
  filtered_msg.header = input->header;
  filtered_points_pub.publish(filtered_msg);

  points_filter_info_msg.header = input->header;
  points_filter_info_msg.filter_name = "ring_filter";
  points_filter_info_msg.original_points_size = scan.size();
  if (voxel_leaf_size >= 0.1)
  {
    points_filter_info_msg.filtered_points_size = filtered_scan_ptr->size();
  }
  else
  {
    points_filter_info_msg.filtered_points_size = scan_ptr->size();
  }
  points_filter_info_msg.original_ring_size = ring_max;
  points_filter_info_msg.filtered_ring_size = ring_max / ring_div;
  points_filter_info_pub.publish(points_filter_info_msg);
}

int main(int argc, char** argv)
{
  ros::init(argc, argv, "ring_filter");

  ros::NodeHandle nh;
  ros::NodeHandle private_nh("~");

  // Publishers
  filtered_points_pub = nh.advertise<sensor_msgs::PointCloud2>("/filtered_points", 10);
  points_filter_info_pub = nh.advertise<points_filter::PointsFilterInfo>("/points_filter_info", 1000);

  // Subscribers
  ros::Subscriber config_sub = nh.subscribe("config/ring_filter", 10, config_callback);
  ros::Subscriber scan_sub = nh.subscribe("points_raw", 10, scan_callback);

  ros::spin();

  return 0;
}
