#ifndef DEPTH_HPP
#define DEPTH_HPP_HPP
#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <vector>
#include <math.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits>
#include <exception>
#include <vision.hpp>
#include "radio_services/InstructionWithAnswer.h"


using namespace std;
using namespace cv;

#define DEPTH_MAX 6000.0  /**< Default maximum distance. Only use this for initialization. */
#define DEPTH_MIN 0.0  /**< Default minimum distance. Only use this for initialization. */

class Depth_processing
{
	public:
				
		Depth_processing();
		~Depth_processing();
		
		//depth callback
		void depthCb(const sensor_msgs::ImageConstPtr& msg);
		bool nodeStateCallback(radio_services::InstructionWithAnswer::Request &req, radio_services::InstructionWithAnswer::Response &res);
				
	private:
	
		ros::NodeHandle nh_;
		cv_bridge::CvImagePtr cv_ptr;
		image_transport::ImageTransport it_;
		image_transport::Subscriber depth_sub;
		image_transport::Publisher  depth_pub;
		ros::ServiceServer service;
			
		string path_;
		string depth_topic;
		string depth_out_image_topic;
		
		People people;
		
		Mat ref_depth;
		Mat dif_depth;
		
		vector< Rect_<int> > depth_rects;
		
		
		bool playback_topics;
		bool display;
		
		int depth_width = 640;
		int depth_height = 480;
		int Hfield = 58;
		int Vfield = 45;
		int interval = 5;
		int frameCounter = -1, dFrameCounter = -1, myThreshold = 100;
		int depthCounter = -1;
		int range = 2; //in pixels
		int verRange = 7; //in pixels
		int recR = 2;
		
		float backFactor = 0.40;
		
		double max_depth;
		double min_depth;
		double horThreshold = 0.33;
		double vertThreshold = 0.5;
		double recThreshold = 0.3;
		double all, curAll, refAll;
		
		
		long curTime ;
		
		vector<Mat> depth_storage;
		
		Mat back_Mat;
	
		bool running = false;
};

int main(int argc, char** argv);
#endif
