#include <chroma.hpp>

Chroma_processing::Chroma_processing()
: it_(nh_)
{
	//Getting the parameters specified by the launch file 
	ros::NodeHandle local_nh("~");
	local_nh.param("image_topic"		 , image_topic		   , string("/camera/rgb/image_raw"));
	local_nh.param("image_out_topic"	 , image_out_topic	   , string("/chroma_proc/image"));
	local_nh.param("image_out_dif_topic" , image_out_dif_topic , string("/chroma_proc/image_dif"));
	local_nh.param("project_path"		 , path_  			   , string(""));
	local_nh.param("playback_topics"	 , playback_topics	   , false);
	local_nh.param("display"			 , display	 		   , false);
	local_nh.param("run_on_start"		 , running			   , false);
	
	if(playback_topics && running)
	{
		ROS_INFO_STREAM_NAMED("Chroma_processing","Subscribing at compressed topics \n"); 
		
		image_sub = it_.subscribe(image_topic, 1, 
		  &Chroma_processing::imageCb, this, image_transport::TransportHints("compressed"));
	}
	else{
		if(running){
			image_sub = it_.subscribe(image_topic, 1, &Chroma_processing::imageCb, this);
		}
	}

	service = local_nh.advertiseService("/ros_visual/chroma/node_state_service", &Chroma_processing::nodeStateCallback, this);
	image_pub 	  = it_.advertise(image_out_topic, 1);
	image_pub_dif = it_.advertise(image_out_dif_topic, 1);
	if(!running){
		ROS_INFO("The chroma node is in \"pause\" state. Use the provided service to start it!");
	}
}

Chroma_processing::~Chroma_processing()
{
	//destroy GUI windows
	destroyAllWindows();
	
}

/* Callback function to handle ROS image messages
 * 
 * PARAMETERS:
 * 			- msg : ROS message that contains the image and its metadata
 * 
 * RETURN: --
 */
void Chroma_processing::imageCb(const sensor_msgs::ImageConstPtr& msg)
{
	cv_bridge::CvImagePtr cv_ptr;
	
	try
	{
	  cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::MONO8);	  
	}
	catch (cv_bridge::Exception& e)
	{
	  ROS_ERROR("cv_bridge exception: %s", e.what());
	  return;
	}

	Mat cur_rgb = (cv_ptr->image);
	
	//~ equalizeHist( cur_rgb, cur_rgb );
	//~ cur_rgb.convertTo(cur_rgb, -1, 1.2, 0);
	Ptr<CLAHE> clahe = createCLAHE();
	clahe->setClipLimit(1.5);
	clahe->setTilesGridSize(Size(10, 10));
	
	// gamma correction
	gammaCorrection(cur_rgb, 2.5);
	clahe->apply(cur_rgb, cur_rgb);

	// First run variable initialization 
	if(ref_rgb.rows == 0)
	{
		rows 	 = cur_rgb.rows;
		cols 	 = cur_rgb.cols;
		channels = cur_rgb.channels();
		size 	 = rows*cols*channels;
		ref_rgb  = cur_rgb.clone();
	}
	
	//Calculating image difference between the current and previous images
	frameDif(cur_rgb, ref_rgb, dif_rgb, 255*0.33);

	for(int y = 0; y < 1; ++y)
	{
		uchar* cur = cur_rgb.ptr<uchar>(y);
		uchar* ref = ref_rgb.ptr<uchar>(y);
		for(int x = 0; x < size; ++x)
			ref[x] = cur[x]*(1-backFactor)+ ref[x]*backFactor;
	}
	if(display)
	{
		//Blob detection
		//~ detectBlobs(dif_rgb, rgb_rects, 15, 1, false);
		
		//~ Mat temp = dif_rgb.clone();
	    //~ for(Rect rect: rgb_rects)
			//~ rectangle(temp, rect, 255, 1);
		//~ rgb_rects.clear();

		
		//Display
		imshow("dif_rgb", ref_rgb);
		moveWindow("dif_rgb", 0, 0);
		imshow("cur_rgb", cur_rgb);
		moveWindow("cur_rgb", 645, 0);
		waitKey(1);
	}
	
	///////////////////////////////
	///////////////////////////////

	/*  Image background estimation	
	 * 
	 * 
	cur_rgb = (cv_ptr->image).clone();
	
	// Edge detection and background estimation
	int kernel_size = 3;
	int scale = 1;
	int delta = 0;
	int ddepth = CV_16S;
	
	
	cur_rgb.convertTo(cur_rgb, -1, 2, 0);
    gammaCorrection(cur_rgb);
	//~ GaussianBlur( cur_rgb, cur_rgb, Size(3,3), 0, 0, BORDER_DEFAULT );

	medianBlur(cur_rgb, cur_rgb, 3);
	Laplacian( cur_rgb, cur_rgb, ddepth, kernel_size, scale, delta, BORDER_DEFAULT );
	convertScaleAbs( cur_rgb, cur_rgb );
	

    estimateBackground(cur_rgb, back_Mat, rgb_storage, 500, 0.04);
    Mat temp_Mat = cur_rgb.clone();
    estimateForeground(cur_rgb, back_Mat, temp_Mat);
    medianBlur(temp_Mat, temp_Mat, 7);
    adaptiveThreshold(temp_Mat, temp_Mat, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 9, -15);
    
    vector< Rect_<int> > back_rects;
    detectBlobs(temp_Mat, back_rects, 15);
	
    imshow("temp_Mat", temp_Mat);
	moveWindow("temp_Mat", 645, 0);
	*/
	
	has_image = true;
	
	//Publish processed image
	cv_ptr->image = cur_rgb;
	image_pub.publish(cv_ptr->toImageMsg());
	
	//Publish image difference
	cv_ptr->image = dif_rgb;
	image_pub_dif.publish(cv_ptr->toImageMsg());
}

/**
 * @brief      This function is called when the corresponding service is called
 *             and based on the parameter passed, either changes its state and
 *             returns the new state, or just returns the current state.
 *             0: Change the current node state to WAITING (false) and return the current node state.
 *             1: Change the current node state to RUNNING (true), return the current node state (and do not change the mode).
 *            -1: Return the current node state.
 *
 * @param[in]  req   The requested action
 * @param[in]  res   The response (the current state of the node)
 *
 * @return     true if everything was successful, false otherwise.
 */
bool Chroma_processing::nodeStateCallback(radio_services::InstructionWithAnswer::Request &req, radio_services::InstructionWithAnswer::Response &res){
	if(req.command == 0 && running){
		running = false;
		image_sub.shutdown();
		ROS_INFO("Stopped ros_visual/chroma!");
	}
	else if(req.command == 1 && !running){
		running = true;
		if(playback_topics){
			image_sub = it_.subscribe(image_topic, 10, &Chroma_processing::imageCb, this, image_transport::TransportHints("compressed"));
			ROS_INFO("Started ros_visual/chroma!");
		}
		else{
			image_sub = it_.subscribe(image_topic, 10, &Chroma_processing::imageCb, this);
			ROS_INFO("Started ros_visual/chroma!");
		}
	}
	res.answer = running;
	return true;
}

int main(int argc, char** argv)
{
	ros::init(argc, argv, "chroma");
	Chroma_processing ip;
	ros::spin();
	return 0;
}
