#include <vision.hpp>
#include <exception>

/* Detects non-black rectangle areas in a black image. This 
 * to produce ROIS(Regions of interest) for further processing.
 *    
 *
 * PARAMETERS: 
 * 			- src		   : the Mat object that holds the image
 * 			- colour_areas : the rectangles produced 
 * 			- range		   : the starting dimension of each rectangle
 * 			- subsampling  : number of pixels to skip at each iteration
 * 			- detect_people: filter boxes(unfinished)
 * 
 * RETURN --
 */
void detectBlobs(const Mat& src, vector< Rect_<int> >& colour_areas, int range, int subsampling, bool detect_people)
{
	bool flag 	 		   = false;
	int cols     		   = src.cols;
	int rows 	 		   = src.rows;
	int channels 		   = src.channels();
	int size 			   = cols*rows*channels;
	
	//Starting from the 1st non-zero pixel it starts forming rectangles (range x range)
	//and fuses them if their intersection is above a certain threshold.
	for(int y = 0; y < 1; y++)
	{
		const uchar *dif = src.ptr<uchar>(y);
		for(int x = 0; x < size; x = x + subsampling*channels)
		{
			if(dif[x] != 0)
			{		
				int i = floor((x/channels)%(cols)); 
				int j = floor(x/(cols*channels));
				
				//If the rect is out of bounds skip
				if((i + range >= cols) || (j + range >= rows))
					continue;
				
				Rect_<int> removal = Rect(i, j, range , range);
					
				if(!colour_areas.empty())
				{
					for(int k = 0; k < colour_areas.size(); k++)
					{
						Rect_<int> rect   = colour_areas[k];
						Rect all 		  = removal | rect;
						Rect intersection = removal & rect;
						int threshold 	  = intersection.area();
						
						if(threshold > 0)
						{
							flag = true;
							colour_areas[k] = all;
							break;
						}
					
					}
					if(!flag)
						colour_areas.push_back(removal);
					else
						flag = false;
				}
				else
					colour_areas.push_back(removal);
			}
			
		}
	}
	
	//In this phase we loop through all the produced rectangles and again try to merge those whose
	//intersection is above a certain threshold	
	int end = colour_areas.size();	
	for(int a = 0; a < end; a++) 
	{
		for(int b = a + 1; b < end; b++) 
		{	
			Rect_<int> removal = colour_areas[a];
			Rect_<int> rect    = colour_areas[b];
			Rect all 		   = removal | rect;
			Rect intersection  = removal & rect;
			int threshold = intersection.area();
			if(threshold == 0)
			{
				int y_distance = 0;
				if (removal.y < rect.y)
					y_distance = rect.y - (removal.y + removal.height);
				else
					y_distance = removal.y - (rect.y + rect.height);
				if(y_distance < rows/20)
				{
					int y_temp 	 = removal.y;
					removal.y 	 = rect.y;
					intersection = removal & rect;
					threshold 	 = intersection.area();
					if(threshold == 0)
					{
						int x_distance = cols;
						if (removal.x < rect.x)
							x_distance = rect.x - (removal.x + removal.width);
						else
							x_distance = removal.x - (rect.x + rect.width);
						
						
						float area_thres = max(removal.area(), rect.area());
						if((x_distance < cols/50) && (all.area() < 2*area_thres))
						{
							threshold = 1;
						}
					}
					removal.y = y_temp;
				}
			}
				
			if(threshold > 0)
			{
				colour_areas[a] = all;
				colour_areas[b] = colour_areas.back();
				colour_areas.pop_back();
				a = -1;
				end--;
				break;
			}
			
		}
	}	
	vector< Rect_<int> >::iterator it;					
	//Filter out erroneous areas (dimensions < 0) that sometimes occur
	for(it = colour_areas.begin(); it < colour_areas.end();)
	{
		Rect_<int> rect = *it; 
		float x  	 = rect.x;
		float y 	 = rect.y;
		float width  = rect.width;
		float height = rect.height;
		if((x < 0) || (y < 0) || (height <= 0) || (width <= 0))
			it = colour_areas.erase(it);
		else
			it++;
	}
	
	//Filter out areas whose ratio cannot belong to a human
	if(detect_people)
	{
		
		vector< Rect_<int> >::iterator it;
		for(it = colour_areas.begin(); it < colour_areas.end();)
		{
			Rect_<int> rect = *it; 
			float width  = rect.width;
			float height = rect.height;
			float area   = rect.area();
			float ratio  = width/height;
			if((ratio < 0.25) || (ratio > 1.5) || (area < cols*rows*0.02))
				it = colour_areas.erase(it);
			else
				it++;
		}
	}
	
	//~ cout<<"Size: "<<colour_areas.size()<<endl;
	//~ for(Rect rect: colour_areas)
	//~ cout<<rect.x<<" "<<rect.y<<" "<<rect.width<<" "<<rect.height<<endl;
	//~ rectangle(src, rect, CV_RGB(255, 255, 255), 1);
}

/* Tracks current rectangle in the image and populates a
 * collection. Every tracked box has a rank(=3) that increases if the box
 * is redetected and decreases otherwise. The threshold that is used to compare
 * the detected boxes with the stored ones. 
 *
 * 
 * PARAMETERS: 
 * 			- cur_boxes  : current image rectangles
 * 			- collection : the collection to be populated
 * 			- rank  	 : the initial rank of a new box, 
 * 			- max_rank  	 : the initial rank of a new box, 
 * 			- threshold  : rectangle comparison threshold
 * 
 * RETURN --
 */
void track(vector< Rect_<int> >& cur_boxes, People& collection, int width, int height, int rank, int max_rank)
{
	
	float step  = 1.5;
	vector<bool> updates(collection.tracked_boxes.size(), false);
	if(!cur_boxes.empty())
	{	
		Rect_<int> all;
		//We reposition every tracked box with a union of
		//the boxes that fall in its area 
		for(int a = 0; a < collection.tracked_boxes.size(); ++a) 
		{	
			all = Rect(0,0,0,0);
			for(vector< Rect_<int> >::iterator it = cur_boxes.begin(); it < cur_boxes.end();) 
			{
				Rect_<int> cur_box = *it;
				Rect intersection = cur_box | collection.tracked_boxes[a];
				int threshold = intersection.area();	
				float area_thres = max(cur_box.area(), collection.tracked_boxes[a].area());
				if(threshold > 0 && threshold < 1.1*area_thres)
				{
					if (all.area() == 0)
						all = cur_box;
					else
						all = cur_box | all;
					it = cur_boxes.erase(it);
				}
				else
					++it;
			}
			
			//The reposition rules
			if(all.area() > 0)
			{
				float x_new;
				float y_new;
				float w_new;
				float h_new;
				if(all.area() > collection.tracked_boxes[a].area())
				{
					x_new = (all.x + collection.tracked_boxes[a].x)/2;
					y_new = (all.y  + collection.tracked_boxes[a].y)/2;
					w_new = (all.width + collection.tracked_boxes[a].width)/2;
					h_new = (all.height + collection.tracked_boxes[a].height)/2;
				}
				else
				{
						
					float thresh = 1;
					float factor = 160; 
					float power = 5*float(collection.tracked_boxes[a].area())/float(all.area());
					float x_dif = (all.x - collection.tracked_boxes[a].x);
					float y_dif = (all.y - collection.tracked_boxes[a].y);
					float w_dif = (all.width - collection.tracked_boxes[a].width);
					float h_dif = (all.height - collection.tracked_boxes[a].height);
					x_new = collection.tracked_boxes[a].x + x_dif/(power);
					y_new = collection.tracked_boxes[a].y + y_dif/(power);
					w_new = collection.tracked_boxes[a].width  + (abs((w_dif*(abs(x_dif) + abs(y_dif) + 1))/(factor*(power + 1)))  < thresh? 0 : w_dif* (abs(x_dif) + abs(y_dif) + 1)/(factor*(power + 1)));
					h_new = collection.tracked_boxes[a].height + (abs((h_dif*(abs(x_dif) + abs(y_dif) + 1))/(factor*(power + 1)))  < thresh? 0 : h_dif* (abs(x_dif) + abs(y_dif) + 1)/(factor*(power + 1)));
					
					if(w_new < 0)
						w_new = 0;
					if(h_new < 0)
						h_new = 0;
				}
				//Calculate features and diffs not normalized
				//****************************
				
				//Ratio feature
				float ratio = float(collection.tracked_boxes[a].height)/float(collection.tracked_boxes[a].width);
				collection.tracked_pos[a].ratio_diff = ratio - collection.tracked_pos[a].ratio;
				collection.tracked_pos[a].ratio      = ratio;
				
				//Area feature
				float area = w_new*h_new;
				collection.tracked_pos[a].area_diff = area - collection.tracked_pos[a].area;
				collection.tracked_pos[a].area      = area;
				
				//x_diff and y_diff
				float x_diff = (x_new - collection.tracked_boxes[a].x)/area;
				float y_diff = (y_new - collection.tracked_boxes[a].y)/area;
				collection.tracked_pos[a].x_delta = x_diff - collection.tracked_pos[a].x_diff;
				collection.tracked_pos[a].y_delta = y_diff - collection.tracked_pos[a].y_diff;
				collection.tracked_pos[a].x_diff  = x_diff;
				collection.tracked_pos[a].y_diff  = y_diff;
				
				//y_norm
				float y_norm = y_new/area;
				collection.tracked_pos[a].y_norm_diff = y_norm - collection.tracked_pos[a].y_norm;
				collection.tracked_pos[a].y_norm 	  = y_norm;
				
				//Distance feature
				int x1 = (x_new + w_new/2);
				int y1 = (y_new + h_new/2);
				int x2 = (collection.tracked_boxes[a].x + collection.tracked_boxes[a].width/2);
				int y2 = (collection.tracked_boxes[a].y + collection.tracked_boxes[a].height/2);
				float distance = sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2))/area;
				collection.tracked_pos[a].distance_diff = distance - collection.tracked_pos[a].distance;
				collection.tracked_pos[a].distance      = distance;
				
				
				
				//assign the new values
				collection.tracked_boxes[a].x 	   = x_new;
				collection.tracked_boxes[a].y  	   = y_new;
				collection.tracked_boxes[a].width  = w_new;
				collection.tracked_boxes[a].height = h_new;
				
				//***************************
				
				//check we did not exceed the limits 
				if(collection.tracked_boxes[a].x + collection.tracked_boxes[a].width > width)
					collection.tracked_boxes[a].width = width - collection.tracked_boxes[a].x;
				if(collection.tracked_boxes[a].y + collection.tracked_boxes[a].height > height)
					collection.tracked_boxes[a].height = height - collection.tracked_boxes[a].y;
					
				updates.at(a) = true;	
			}
			
			
		}
		for(int a = 0; a < cur_boxes.size(); ++a) 
		{
			Position pos;
			collection.tracked_pos.push_back(pos);
			collection.tracked_boxes.push_back(cur_boxes[a]);
			collection.tracked_rankings.push_back(rank + step);
		}
		
		
		//In this phase we loop through all the produced rectangles and again try to merge those whose
		//intersection is above a certain threshold	
		int end = collection.tracked_boxes.size();
		for(int a = 0; a < end; ++a) 
		{
			for(int b = a + 1; b < end; ++b) 
			{	
				Rect_<int> removal = collection.tracked_boxes[a];
				Rect_<int> rect    = collection.tracked_boxes[b];
				Rect all 		   = removal | rect;
				
				int y_distance = height;
				if (removal.y < rect.y)
					y_distance = rect.y - (removal.y + removal.height);
				else
					y_distance = removal.y - (rect.y + rect.height);
					
				Rect intersection;
				int threshold = 0;
				if(y_distance < height/20)
				{
					int y_temp 	 = removal.y;
					removal.y 	 = rect.y;
					intersection = removal & rect;
					threshold 	 = intersection.area();
					if (threshold == 0)
					{
						int x_distance = width;
						if (removal.x < rect.x)
							x_distance = rect.x - (removal.x + removal.width);
						else
							x_distance = removal.x - (rect.x + rect.width);
						
						
						//~ float area_thres = max(removal.area(), rect.area());
						//~ if((x_distance < width/50) && (all.area() < 2*area_thres))
						//~ {
							//~ threshold = 1;
						//~ }
					}
					removal.y 	 = y_temp;
				}
				if(threshold > 0)
				{
					float rank_dif = abs(collection.tracked_rankings[a] - collection.tracked_rankings[b])/(collection.tracked_rankings[a] + collection.tracked_rankings[b]);
					float rank_ratio = (collection.tracked_rankings[a] + collection.tracked_rankings[b])/(max_rank);
					if(rank_ratio < 1.1)
					{
						collection.tracked_boxes[a] = all;
						collection.tracked_boxes[b] = collection.tracked_boxes.back();
						collection.tracked_boxes.pop_back();
						
						collection.tracked_rankings[b] = collection.tracked_rankings.back();
						collection.tracked_rankings.pop_back();
						
						collection.tracked_pos[b] = collection.tracked_pos.back();
						collection.tracked_pos.pop_back();
						b=a;
						--end;
					}
				}
				
			}
		}	
	}	
	
	//Update the rankings
	for(int a = 0; a < collection.tracked_boxes.size(); ++a)
	{
		if (updates[a] == true)
		{
			if(collection.tracked_rankings[a] <= max_rank)
				collection.tracked_rankings[a] = collection.tracked_rankings[a] + step;
		}
		collection.tracked_rankings[a] = collection.tracked_rankings[a] - 1;
	}
	
	//Delete those that fall below 0 rank
	for(vector<float>::iterator rank_it = collection.tracked_rankings.begin(); rank_it < collection.tracked_rankings.end();)
	{
		int dist = distance(collection.tracked_rankings.begin(), rank_it);
		if(*rank_it < rank || collection.tracked_boxes[dist].area() < rank)
		{
			rank_it = collection.tracked_rankings.erase(rank_it);
			collection.tracked_pos.erase(collection.tracked_pos.begin() + dist);
			collection.tracked_boxes.erase(collection.tracked_boxes.begin() + dist);
		}
		else
			++rank_it;
		
	}
		
}

/* Calculates and stores the coordinates(x, y, z) in meters of a rectangle
 * in respect to the center of the camera.
 * 
 * PARAMETERS:
 * 			- rect   : rectangle to be processed
 * 			- pos    : object to save the measurements produced
 * 			- width  : image width
 * 			- height : image height
 * 			- Hfield : camera horizontal field of view in degrees
 * 			- Vfield : camera vertical field of view in degrees
 * 
 * RETURN: --
 * 
 */
void calculatePosition(Rect& rect, Position& pos, int width, int height, int Hfield, int Vfield)
{
	
	float hor_x 	= 0.0;
	float hor_y 	= 0.0;
	float ver_x 	= 0.0;
	float ver_y 	= 0.0;
	float hor_focal = 0.0;
	float ver_focal = 0.0;
	float distance  = 0.0;
	float depth = pos.z;
	
	if(depth != 0.0)
	{
		
		//Find the focal length 
		hor_focal = height / (2 * tan((Vfield/2) * M_PI / 180.0) );
		ver_focal = width / (2 * tan((Hfield/2) * M_PI / 180.0) );
		
		//Transform the pixel x, y in respect to 
		//the camera center
		ver_x = rect.x + rect.width/2;
		if(ver_x > width/2)
			ver_x = (ver_x - width/2);
		else
			ver_x = (-1)*(width/2 - ver_x);
		
		ver_y = rect.y + rect.height/2;
		if(ver_y > height/2)
			ver_y = (-1)*(ver_y - height/2);
		else
			ver_y = (height/2 - ver_y);
		
		//Calculate the real world coordinates of the box center
		hor_y = depth * ver_y / hor_focal;
		hor_x = depth * ver_x / ver_focal;
		
		
		//Update the position
		pos.x = hor_x;
		pos.y = hor_y;
		
	}
	
}

/* Estimates the foreground by combining static images, works for
 * images that contain edge information
 * 
 * PARAMETERS:
 * 			- cur_mat  : mat holding the current image frame
 * 			- back_Mat : mat to store the result
 * 			- dst_Mat  : the number of images to combine 
 * 
 * RETURN: --
 * 
 */
void estimateForeground(Mat& cur_Mat, Mat& back_Mat, Mat& dst_Mat)
{
	uchar *cur;
	uchar *dst;
	uchar *back;
	for(int y = 0; y < 1; y++)
	{
		int rows	 = cur_Mat.rows;
		int cols 	 = cur_Mat.cols;
		int channels = cur_Mat.channels();
		int size 	 = rows*cols*channels;
	
		back = back_Mat.ptr<uchar>(y);
		cur  = cur_Mat.ptr<uchar>(y);
		dst  = dst_Mat.ptr<uchar>(y);
		for(int x = 0; x < size; x = x + channels)
		{ 
			if(back[x] != 0)
				dst[x] = 0;
		}
	}
}
	
/* Estimates the background by combining static images recursively, works for
 * images that contain edge information. It recursively performs bitwise_and 
 * to remove noise and keep only the motionless part of the image and bitwise_or
 * at the produced images to fill parts of the images that were blocked from moving
 * objects (e.g. people) 
 * 
 * 
 * PARAMETERS:
 * 			- src 	    : mat holding the current image frame
 * 			- dst 	    : mat to store the result
 * 			- storage   : vector to store the intermediate images
 * 			- recursion : number of recursions to perform
 * 			- ratio 	: percent of the recursions that will bitwise_or operations
 * 
 * RETURN: --
 */
void estimateBackground(Mat& src, Mat& dst, vector<Mat>& storage, int recursion, float ratio, int index)
{
	Mat result;
	int size = storage.size();
	if (index > recursion)
	{
		dst = storage.at(index - 1);
		return;
	}
	if(size > 0)
	{
		if((index <= 0) || ((index >= recursion*ratio && (size >= recursion*ratio))))
		{
			bitwise_or(src, storage.at(index), result);
		}
		else
			bitwise_and(src, storage.at(index), result);
		if((size -1) == index)
		{
			storage.push_back(result);
			dst = result;
		}
		else
		{
			storage.at(index) = src;
			index++;
			estimateBackground(result, dst, storage, recursion, ratio, index);	
		}
	}
	else
	{
		storage.push_back(src);
		dst = src.clone();
	}
}

/* Calculates the absolute difference between the two mats and thresholds
 * the result according to the threshold given
 * 
 * PARAMETERS:
 * 			- scr1 		: Mat first
 * 			- scr2 		: Mat second
 * 			- dst 		: destination Mat 
 * 			- threshold : threshold to be used
 * 
 * RETURN: --
 */
void frameDif(const Mat& src1, const Mat& src2, Mat& dst, float thresh)
{
	absdiff(src1, src2, dst);
	threshold(dst, dst, thresh, 255, 0);
}

/* Image gamma correction 
 *
 * PARAMETERS:
 * 			- src: Mat to perform gamma correction
 * 
 * RETURN: --
 */
void gammaCorrection(const Mat& src, float factor)
{

	float inverse_gamma = 1.0/2.5;
	Mat lut_matrix(256, 1, CV_8UC1);
	uchar* ptr = lut_matrix.ptr<uchar>(0);
	for( int i = 0; i < 256; ++i)
		ptr[i] =  saturate_cast<uchar>(pow(i/255.0, inverse_gamma)*255.0);
	LUT(src, lut_matrix, src);
}


