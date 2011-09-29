/*
 * HistologicalEntities.h
 *
 *  Created on: Jul 1, 2011
 *      Author: tcpan
 */

#ifndef HistologicalEntities_H_
#define HistologicalEntities_H_

#include "utils.h"
#include "opencv2/opencv.hpp"
#include "opencv2/gpu/gpu.hpp"
#include <string.h>

namespace nscale {

class HistologicalEntities {

public:
	static cv::Mat getRBC(const cv::Mat& img);
	static cv::Mat getRBC(const std::vector<cv::Mat>& bgr);
	static cv::Mat getBackground(const cv::Mat& img);
	static cv::Mat getBackground(const std::vector<cv::Mat>& bgr);


	static int segmentNuclei(const cv::Mat& img, cv::Mat& output, cciutils::SimpleCSVLogger *logger = NULL, int stage=-1);
	static int segmentNuclei(const std::string& input, const std::string& output, cciutils::SimpleCSVLogger *logger=NULL, int stage=-1);
};


namespace gpu {

class HistologicalEntities {

public:
	static cv::gpu::GpuMat getRBC(const std::vector<cv::gpu::GpuMat>& bgr, cv::gpu::Stream& stream);
	static cv::gpu::GpuMat getBackground(const std::vector<cv::gpu::GpuMat>& bgr, cv::gpu::Stream& stream);

	static int segmentNuclei(const cv::Mat& img, cv::Mat& output, cciutils::SimpleCSVLogger *logger = NULL, int stage=-1);
	static int segmentNuclei(const std::string& input, const std::string& output, cciutils::SimpleCSVLogger *logger = NULL, int stage=-1);
};

}

}
#endif /* HistologicalEntities_H_ */
