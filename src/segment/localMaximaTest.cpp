/*
 * test.cpp
 *
 *  Created on: Jun 28, 2011
 *      Author: tcpan
 */
#include "cv.h"
#include "highgui.h"
#include <iostream>
#include <dirent.h>
#include <vector>
#include <string>
#include <errno.h>
#include <time.h>
#include "MorphologicOperations.h"
#include "utils.h"


using namespace cv;


int main (int argc, char **argv){
	// test perfromance of imreconstruct.
	Mat mask(Size(4096,4096), CV_8U);
	randn(mask, Scalar::all(128), Scalar::all(30));
	imwrite("test/in-mask2-for-localmax.ppm", mask);

	uint64_t t1 = cciutils::ClockGetTime();
	Mat recon = nscale::localMaxima<uchar>(mask, 8);
	uint64_t t2 = cciutils::ClockGetTime();
	std::cout << "localmax took " << t2-t1 << "ms" << std::endl;
	imwrite("test/out-localmax.ppm", recon);

	t1 = cciutils::ClockGetTime();
	recon = nscale::localMinima<uchar>(mask, 8);
	t2 = cciutils::ClockGetTime();
	std::cout << "localmin took " << t2-t1 << "ms" << std::endl;
	imwrite("test/out-localmin.ppm", recon);

	t1 = cciutils::ClockGetTime();
	recon = nscale::localMaxima2<uchar>(mask, 8);
	t2 = cciutils::ClockGetTime();
	std::cout << "localmax2 took " << t2-t1 << "ms" << std::endl;
	imwrite("test/out-localmax2.ppm", recon);

	t1 = cciutils::ClockGetTime();
	recon = nscale::localMinima2<uchar>(mask, 8);
	t2 = cciutils::ClockGetTime();
	std::cout << "localmin2 took " << t2-t1 << "ms" << std::endl;
	imwrite("test/out-localmin2.ppm", recon);


	// imhmin test
	t1 = cciutils::ClockGetTime();
	Mat hmin = nscale::imhmin<uchar>(mask, 1, 8);
	t2 = cciutils::ClockGetTime();
	std::cout << "hmin -1 took " << t2-t1 << "ms" << std::endl;
	imwrite("test/out-hmin-1.ppm", hmin);

	t1 = cciutils::ClockGetTime();
	hmin = nscale::imhmin<uchar>(mask, 2, 8);
	t2 = cciutils::ClockGetTime();
	std::cout << "hmin -2 took " << t2-t1 << "ms" << std::endl;
	imwrite("test/out-hmin-2.ppm", hmin);

	t1 = cciutils::ClockGetTime();
	hmin = nscale::imhmin<uchar>(mask, 3, 8);
	t2 = cciutils::ClockGetTime();
	std::cout << "hmin -3 took " << t2-t1 << "ms" << std::endl;
	imwrite("test/out-hmin-3.ppm", hmin);

	return 0;
}

