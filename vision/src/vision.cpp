#include <vision.hpp>

/* Detects non-black rectangle areas in a black image. This 
 * to produce ROIS(Regions of interest) for further processing.
 *    
 *
 * PARAMETERS: 
 * 			- src		   : the Mat object that holds the image
 * 			- colour_areas : the rectangles produced 
 * 			- range		   : the starting dimension of each rectangle
 * 			- detect_people: the starting dimension of each rectangle
 * 
 * RETURN --
 */
void detectBlobs(Mat& src, vector< Rect_<int> >& colour_areas, int range, bool detect_people)
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
		for(int x = 0; x < size; x = x + channels)
		{
			if(dif[x] != 0)
			{				
				int i = (x/channels)%(cols); 
				int j = floor(x/(cols*channels));
				
				Rect_<int> removal;
				//If the rect is out of bounds skip
				if((i + range >= cols) || (j + range >= rows))
					continue;
				
				removal = Rect(i, j, range , range);
				if(removal.width < 1 || removal.height < 1)
					continue;
					
				if(!colour_areas.empty())
				{
					for(int k = 0; k < colour_areas.size(); k++)
					{
						Rect_<int> rect   = colour_areas[k];
						Rect all 		  = removal | rect;
						Rect intersection = removal & rect;
						int threshold 	  = intersection.area();
						
						if(threshold < 1)
							continue;
						else if(threshold > 0)
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
			
			int y_distance;
			if (removal.y < rect.y)
				y_distance = abs(removal.y + removal.height - rect.y);
			else
				y_distance = abs(removal.y - rect.y + rect.height);
				
			Rect intersection;
			if(y_distance < rows/3)
			{
				int temp 	 = removal.y;
				removal.y 	 = rect.y;
				intersection = removal & rect;
				removal.y 	 = temp;
			}
			else
				intersection = removal & rect;
				
			int threshold 	 = intersection.area();
			if(threshold < 1)
				continue;
			else if(threshold > 0)
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
					
	end = colour_areas.size();
	
	//Filter out erroneous areas (dimensions < 0) that sometimes occur
	for(int k = 0; k < end; k++)
	{
		float x  	 = colour_areas[k].x;
		float y 	 = colour_areas[k].y;
		float width  = colour_areas[k].width;
		float height = colour_areas[k].height;
		float area = colour_areas[k].area();
		if((area <= pow(range,2)) || (x < 0) || (y < 0) || (height < 0) || (width < 0))
		{
			colour_areas[k] = colour_areas.back();
			colour_areas.pop_back();
			k--;
			end--;
				
		}
	}
	
	//Filter out areas whose ratio cannot belong to a human
	if(detect_people)
	{
		end = colour_areas.size();
		for(int k = 0; k < end; k++)
		{
			float width  = colour_areas[k].width;
			float height = colour_areas[k].height;
			float area   = colour_areas[k].area();
			float ratio  = width/height;
			if((ratio < 0.25) || (ratio > 1.5) || (area < cols*rows*0.02))
			{
				//~ rectangle(src, colour_areas[k], 0, CV_FILLED);
				colour_areas[k] = colour_areas.back();
				colour_areas.pop_back();
				k--;
				end--;
					
			}
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
 * 			- threshold  : rectangle comparison threshold
 * 
 * RETURN --
 */
void track(vector< Rect_<int> >& cur_boxes, People& collection, int rank)
{
	bool exists = false;
	float step  = 1.2;
	vector<float> distances;
	if(!cur_boxes.empty())
	{	
		
		for(int a = 0; a < cur_boxes.size(); a++) 
		{
			exists = false;
			for(int b = 0; b < collection.tracked_boxes.size(); b++) 
			{
				Rect intersection = cur_boxes[a] & collection.tracked_boxes[b];
				int threshold = intersection.area();	
				if(threshold > 0)
				{
					float dif = cur_boxes[a].area()/collection.tracked_boxes[b].area();
					if(cur_boxes[a].area() > collection.tracked_boxes[b].area())
					{
						if(collection.tracked_boxes[b].area() > threshold)
						{	
							collection.tracked_boxes[b] = cur_boxes[a];
							if(dif < 10)
							{
								collection.tracked_boxes[b].width = (collection.tracked_boxes[b].width + cur_boxes[a].width)/2;
								collection.tracked_boxes[b].height = (collection.tracked_boxes[b].height + cur_boxes[a].height)/2;
							}
						}
						else
						{
							collection.tracked_boxes[b] = cur_boxes[a];
						}
					}
					else
					{
						float dif = collection.tracked_boxes[b].area()/cur_boxes[a].area();
						if((cur_boxes[a].area()) > threshold)
						{	
							if(dif < 2)
							{
								collection.tracked_boxes[b].x += (cur_boxes[a].x - collection.tracked_boxes[b].x)/10;
								collection.tracked_boxes[b].y += (cur_boxes[a].y - collection.tracked_boxes[b].y)/10;
								collection.tracked_boxes[b].width += (cur_boxes[a].width - collection.tracked_boxes[b].width)/10;
								collection.tracked_boxes[b].height += (cur_boxes[a].height - collection.tracked_boxes[b].height)/10;
							}
							//~ else
							//~ {
								//~ collection.tracked_boxes[b].width = (collection.tracked_boxes[b].width + cur_boxes[a].width)/2;
								//~ collection.tracked_boxes[b].height = (collection.tracked_boxes[b].height + cur_boxes[a].height)/2;
							//~ }
						}
						else
						{
							collection.tracked_boxes[b] = collection.tracked_boxes[b] | cur_boxes[a];
						}
					}
						
					if(collection.tracked_rankings[b] <= 30)
						collection.tracked_rankings[b] = collection.tracked_rankings[b] + step;
					exists = true;
					break;
				}
				
			}
			if(!exists)
			{
				collection.tracked_boxes.push_back(cur_boxes[a]);
				collection.tracked_rankings.push_back(rank + step);
			}
		}
		for(int a = 0; a < collection.tracked_boxes.size(); a++)
		{
			collection.tracked_rankings[a] = collection.tracked_rankings[a] - 1;
		}
		for(int a = 0; a < collection.tracked_boxes.size(); a++)
		{
			if(collection.tracked_rankings[a] <= 0 && collection.tracked_boxes.size() > 0)
			{
				collection.tracked_boxes[a]    = collection.tracked_boxes.back();
				collection.tracked_boxes.pop_back();
				collection.tracked_rankings[a] = collection.tracked_rankings.back();
				collection.tracked_rankings.pop_back();
				a--;
			}
			
		}
		
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
	float top 		= 0.0;
	float bottom 	= 0.0;
	float depth 	= pos.z;
	
	if (depth != 0.0)
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
		
		if(pos.x != 0)
		{
			distance = abs(pos.x - hor_x);
			pos.distance = distance;
		}
		pos.x = hor_x;
		pos.y = hor_y;
		
		
		
		
		ver_y = rect.y;
		if(ver_y > height/2)
			ver_y = (-1)*(ver_y - height/2);
		else
			ver_y = (height/2 - ver_y);
		top = depth * ver_y / hor_focal;
		
		
		ver_y = rect.y + rect.height;
		if(ver_y > height/2)
			ver_y = (-1)*(ver_y - height/2);
		else
			ver_y = (height/2 - ver_y);
			
		bottom     = depth * ver_y / hor_focal;
		pos.top    = abs(top);
		pos.height = abs(top - bottom);
	
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
 * images the contain edge information. It recursively performs bitwise_and 
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
void frameDif(Mat& src1, Mat& src2, Mat& dst, float thresh)
{
	// Absolute dif between our current mat and the previous one 
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
void gammaCorrection(Mat& src)
{
	double inverse_gamma = 1.0 / 2.5;
	
	Mat lut_matrix(1, 256, CV_8UC1 );
	uchar * ptr = lut_matrix.ptr();
	for( int i = 0; i < 256; i++ )
		ptr[i] = (int)( pow( (double) i / 255.0, inverse_gamma ) * 255.0 );
	
	LUT( src, lut_matrix, src );
}


