/*
 * RedBloodCell.cpp
 *
 *  Created on: Jul 1, 2011
 *      Author: tcpan
 */

#include "HistologicalEntities.h"
#include <iostream>
#include "MorphologicOperations.h"
#include "PixelOperations.h"
#include "highgui.h"
#include "float.h"
#include "utils.h"
#include "opencv2/gpu/gpu.hpp"
#include "precomp.hpp"


namespace nscale {

using namespace cv;

namespace gpu {

using namespace cv::gpu;


#if !defined (HAVE_CUDA)
GpuMat HistologicalEntities::getRBC(const std::vector<GpuMat>& bgr, Stream& stream) { throw_nogpu(); }
GpuMat HistologicalEntities::getBackground(const std::vector<GpuMat>& g_bgr, Stream& stream) { throw_nogpu(); }
int HistologicalEntities::segmentNuclei(const Mat& img, Mat& output, cciutils::SimpleCSVLogger& logger, int stage) { throw_nogpu(); }


#else

GpuMat HistologicalEntities::getRBC(const std::vector<GpuMat>& bgr, Stream& stream) {
	CV_Assert(bgr.size() >= 3);
	/*
	%T1=2.5; T2=2;
    T1=5; T2=4;

	imR2G = double(r)./(double(g)+eps);
    bw1 = imR2G > T1;
    bw2 = imR2G > T2;
    ind = find(bw1);
    if ~isempty(ind)
        [rows, cols]=ind2sub(size(imR2G),ind);
        rbc = bwselect(bw2,cols,rows,8) & (double(r)./(double(b)+eps)>1);
    else
        rbc = zeros(size(imR2G));
    end
	 */
	float T1 = 5.0;
	float T2 = 4.0;
	Size s = bgr[0].size();
	int newType = CV_32FC1;
	GpuMat bd, gd, rd;

	stream.enqueueConvert(bgr[0], bd, newType, 1.0, FLT_EPSILON);
	stream.enqueueConvert(bgr[1], gd, newType, 1.0, FLT_EPSILON);
	stream.enqueueConvert(bgr[2], rd, newType);
	stream.waitForCompletion();

	GpuMat imR2G, imR2B;
	divide(rd, gd, imR2G, stream);
	divide(rd, bd, imR2B, stream);
	stream.waitForCompletion();
	rd.release();
	gd.release();
	bd.release();
	GpuMat bw3 = PixelOperations::threshold<float>(imR2B, 1.0, std::numeric_limits<float>::max(), stream);
	GpuMat bw1 = PixelOperations::threshold<float>(imR2G, T1, std::numeric_limits<float>::max(), stream);
	GpuMat bw2 = PixelOperations::threshold<float>(imR2G, T2, std::numeric_limits<float>::max(), stream);
	stream.waitForCompletion();
	imR2G.release();
	imR2B.release();


	GpuMat rbc(s, CV_8UC1);
	stream.enqueueMemSet(rbc, Scalar(0));


	if (countNonZero(bw1) > 0) {
		GpuMat temp = nscale::gpu::bwselect<unsigned char>(bw2, bw1, 8, stream);

//	strange.  have to copy it else bitwise_and gets misaligned address error (see it in debugger)
		GpuMat temp2(temp.size(), temp.type());
		stream.enqueueCopy(temp, temp2);
		temp.release();

		bitwise_and(temp2, bw3, rbc, GpuMat(), stream);
	    stream.waitForCompletion();
		temp2.release();
	}
	bw1.release();
	bw2.release();
	bw3.release();

	return rbc;
}

GpuMat HistologicalEntities::getBackground(const std::vector<GpuMat>& g_bgr, Stream& stream) {

	GpuMat b1, g1, r1;
	GpuMat g_bg(g_bgr[0].size(), CV_8U);
	stream.enqueueMemSet(g_bg, Scalar(0));
	unsigned char max = std::numeric_limits<unsigned char>::max();
	threshold(g_bgr[0], b1, 220, max, THRESH_BINARY, stream);
	threshold(g_bgr[1], g1, 220, max, THRESH_BINARY, stream);
	threshold(g_bgr[2], r1, 220, max, THRESH_BINARY, stream);
	bitwise_and(b1, r1, g_bg, g1, stream);
	stream.waitForCompletion();

	b1.release();
	g1.release();
	r1.release();

	return g_bg;
}


int HistologicalEntities::segmentNuclei(const std::string& in, const std::string& out, cciutils::SimpleCSVLogger *logger, int stage) {
	Mat input = imread(in);
	if (!input.data) return -1;

	Mat output(input.size(), CV_8U, Scalar(0));

	int status = nscale::gpu::HistologicalEntities::segmentNuclei(input, output, logger, stage);

	imwrite(out, output);

	return status;
}



int HistologicalEntities::segmentNuclei(const Mat& img, Mat& output, cciutils::SimpleCSVLogger *logger, int stage) {
	// image in BGR format
	if (!img.data) return -1;


//	Mat img2(Size(1024,1024), img.type());
//	resize(img, img2, Size(1024,1024));
//	namedWindow("orig image", CV_WINDOW_AUTOSIZE);
//	imshow("orig image", img2);

	/*
	* this part to decide if the tile is background or foreground
	THR = 0.9;
    grayI = rgb2gray(I);
    area_bg = length(find(I(:,:,1)>=220&I(:,:,2)>=220&I(:,:,3)>=220));
    ratio = area_bg/numel(grayI);
    if ratio >= THR
        return;
    end
	 */
	if (logger) logger->logStart("start");
	if (stage == 0) {
			output = img;
			return 0;
		}

	GpuMat g_img;
	Stream stream;
	stream.enqueueUpload(img, g_img);
	stream.waitForCompletion();
	if (logger) logger->logTimeElapsedSinceLastLog("uploaded image");

	std::vector<GpuMat> g_bgr;
	split(g_img, g_bgr, stream);
	stream.waitForCompletion();
	if (logger) logger->logTimeElapsedSinceLastLog("toRGB");

	GpuMat g_bg = getBackground(g_bgr, stream);
	stream.waitForCompletion();
	if (stage == 1) {
		Mat temp(g_bg.size(), g_bg.type());
		stream.enqueueDownload(g_bg,temp);
		stream.waitForCompletion();
		output = temp;
		g_img.release();
		return 0;
	}

	int bgArea = countNonZero(g_bg);
	g_bg.release();

	float ratio = (float)bgArea / (float)(img.size().area());
	//std::cout << " background size: " << bgArea << " ratio: " << ratio << std::endl;
	if (logger) logger->log("backgroundRatio", ratio);
	if (ratio >= 0.9) {
		//std::cout << "background.  next." << std::endl;
		if (logger) logger->logTimeElapsedSinceLastLog("background");
		//if (logger) logger->endSession();
		return 0;
	}
	if (logger) logger->logTimeElapsedSinceLastLog("background");

	uint64_t t1 = cciutils::ClockGetTime();
	GpuMat g_rbc = nscale::gpu::HistologicalEntities::getRBC(g_bgr, stream);
	stream.waitForCompletion();
	if (stage == 2) {
		Mat temp(g_rbc.size(), g_rbc.type());
		stream.enqueueDownload(g_rbc, temp);
		stream.waitForCompletion();
		output = temp;
		g_img.release();
		return 0;
	}
	uint64_t t2 = cciutils::ClockGetTime();
	if (logger) logger->logTimeElapsedSinceLastLog("RBC");
	int rbcPixelCount = countNonZero(g_rbc);
	if (logger) logger->log("RBCPixCount", rbcPixelCount);
	//std::cout << "rbc took " << t2-t1 << "ms" << std::endl;

	GpuMat g_r(g_bgr[2]);
	g_bgr[0].release();
	g_bgr[1].release();
	g_bgr[2].release();

	Mat rbc(g_rbc.size(), g_rbc.type());
	stream.enqueueDownload(g_rbc, rbc);
	stream.waitForCompletion();
	g_rbc.release();
	if (logger) logger->logTimeElapsedSinceLastLog("cpuCopyRBC");
//	imwrite("test/out-rbc.pbm", rbc);

	/*
	rc = 255 - r;
    rc_open = imopen(rc, strel('disk',10));
    rc_recon = imreconstruct(rc_open,rc);
    diffIm = rc-rc_recon;
	 */

	GpuMat g_rc = nscale::gpu::PixelOperations::invert<unsigned char>(g_r, stream);
	stream.waitForCompletion();
	g_r.release();
	if (logger) logger->logTimeElapsedSinceLastLog("invert");

	GpuMat g_rc_open(g_rc.size(), g_rc.type());
	//Mat disk19 = getStructuringElement(MORPH_ELLIPSE, Size(19,19));
	// structuring element is not the same between matlab and opencv.  using the one from matlab explicitly....
	// (for 4, 6, and 8 connected, they are approximations).
	unsigned char disk19raw[361] = {
			0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
			0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
			0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
			0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
			0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
			0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
			0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0};
	std::vector<unsigned char> disk19vec(disk19raw, disk19raw+361);
	Mat disk19(disk19vec);
	disk19 = disk19.reshape(1, 19);
//	imwrite("test/out-rcopen-strel.pbm", disk19);
	// filter doesnot check borders.  so need to create border.
	GpuMat rc_border;
	copyMakeBorder(g_rc, rc_border, 9,9,9,9, Scalar(0), stream);
	stream.waitForCompletion();
	GpuMat rc_roi(rc_border, Range(9, 9+ g_rc.size().height), Range(9, 9+g_rc.size().width));
	morphologyEx(rc_roi, g_rc_open, MORPH_OPEN, disk19, Point(-1, -1), 1, stream);
	//Mat rc_open(g_rc_open.size(), g_rc_open.type());
	//stream.enqueueDownload(g_rc_open, rc_open);
	stream.waitForCompletion();
	if (stage == 3) {
		Mat temp(g_rc_open.size(), g_rc_open.type());
		stream.enqueueDownload(g_rc_open, temp);
		stream.waitForCompletion();
		output = temp;
		g_img.release();
		return 0;
	}

	if (logger) logger->logTimeElapsedSinceLastLog("open19");
	rc_roi.release();
	rc_border.release();
//	imwrite("test/out-rcopen.ppm", rc_open);


	unsigned int iter;
	GpuMat g_rc_recon = nscale::gpu::imreconstruct<unsigned char>(g_rc_open, g_rc, 8, stream, iter);
	stream.waitForCompletion();
//	std::cout << "\tIterations: " << iter << std::endl;
	if (stage == 4) {
		Mat temp(g_rc_recon.size(), g_rc_recon.type());
		stream.enqueueDownload(g_rc_recon, temp);
		stream.waitForCompletion();
		output = temp;
		g_img.release();
		return 0;
	}

	GpuMat g_diffIm;
	subtract(g_rc, g_rc_recon, g_diffIm, stream);
	stream.waitForCompletion();
	if (stage == 5) {
		Mat temp(g_diffIm.size(), g_diffIm.type());
		stream.enqueueDownload(g_diffIm, temp);
		stream.waitForCompletion();
		output = temp;
		g_img.release();
		return 0;
	}

	if (logger) logger->logTimeElapsedSinceLastLog("reconToNuclei");
	if (logger) logger->log("rc_reconIter", iter);
	int rc_openPixelCount = countNonZero(g_rc_open);
	if (logger) logger->log("rc_openPixCount", rc_openPixelCount);
	g_rc_open.release();
	g_rc.release();
	g_rc_recon.release();

	Mat diffIm(g_diffIm.size(), g_diffIm.type());
	stream.enqueueDownload(g_diffIm, diffIm);
	stream.waitForCompletion();
	if (logger) logger->logTimeElapsedSinceLastLog("downloadReconToNuclei");

//	imwrite("test/out-redchannelvalleys.ppm", diffIm);

/*
    G1=80; G2=45; % default settings
    %G1=80; G2=30;  % 2nd run

    bw1 = imfill(diffIm>G1,'holes');
 *
 */
	unsigned char G1 = 80;
	GpuMat g_diffIm2;
	threshold(g_diffIm, g_diffIm2, G1, std::numeric_limits<unsigned char>::max(), THRESH_BINARY, stream);
	stream.waitForCompletion();
	if (stage == 6) {
		Mat temp(g_diffIm2.size(), g_diffIm2.type());
		stream.enqueueDownload(g_diffIm2, temp);
		stream.waitForCompletion();
		output = temp;
		g_img.release();

		return 0;
	}

	if (logger) logger->logTimeElapsedSinceLastLog("threshold1");

	g_diffIm.release();

	GpuMat g_bw1 = nscale::gpu::imfillHoles<unsigned char>(g_diffIm2, true, 4, stream);
	stream.waitForCompletion();
	if (stage == 7) {
		Mat temp(g_bw1.size(), g_bw1.type());
		stream.enqueueDownload(g_bw1, temp);
		stream.waitForCompletion();
		output = temp;
		g_img.release();

		return 0;
	}

	if (logger) logger->logTimeElapsedSinceLastLog("fillHoles1");
	Mat bw1(g_bw1.size(), g_bw1.type());
	stream.enqueueDownload(g_bw1, bw1);
	stream.waitForCompletion();
	if (logger) logger->logTimeElapsedSinceLastLog("downloadHoleFilled1");
	g_diffIm2.release();
	g_bw1.release();
//	imwrite("test/out-rcvalleysfilledholes.ppm", bw1);

	g_img.release();
	stream.waitForCompletion();
	if (logger) logger->logTimeElapsedSinceLastLog("GPU done");
	
	// TODO: change back
//	return 0;

/*
 *     %CHANGE
    [L] = bwlabel(bw1, 8);
    stats = regionprops(L, 'Area');
    areas = [stats.Area];

    %CHANGE
    ind = find(areas>10 & areas<1000);
    bw1 = ismember(L,ind);
    bw2 = diffIm>G2;
    ind = find(bw1);

    if isempty(ind)
        return;
    end
 *
 */
	bw1 = nscale::bwareaopen<unsigned char>(bw1, 11, 1000, 8);
	if (stage == 8) {
		output = bw1;
		return 0;
	}
	if (countNonZero(bw1) == 0) {
		if (logger) logger->logTimeElapsedSinceLastLog("areaThreshold1");
		//if (logger) logger->endSession();
		return 0;
	}
	if (stage == 9) {
		output = bw1;
		return 0;
	}
//	imwrite("test/out-nucleicandidatessized.ppm", bw1);
	if (logger) logger->logTimeElapsedSinceLastLog("areaThreshold1");



	unsigned char G2 = 45;
	Mat bw2 = diffIm > G2;
	if (stage == 10) {
		output = bw2;
		return 0;
	}



	/*
	 *
    [rows,cols] = ind2sub(size(diffIm),ind);
    seg_norbc = bwselect(bw2,cols,rows,8) & ~rbc;
    seg_nohole = imfill(seg_norbc,'holes');
    seg_open = imopen(seg_nohole,strel('disk',1));
	 *
	 */

	Mat seg_norbc = nscale::bwselect<unsigned char>(bw2, bw1, 8);
	if (stage == 11) {
		output = seg_norbc;
		return 0;
	}
	seg_norbc = seg_norbc & (rbc == 0);
	if (stage == 12) {
		output = seg_norbc;
		return 0;
	}
//	imwrite("test/out-nucleicandidatesnorbc.ppm", seg_norbc);
	if (logger) logger->logTimeElapsedSinceLastLog("blobsGt45");

	Mat seg_nohole = nscale::imfillHoles<unsigned char>(seg_norbc, true, 4);
	if (logger) logger->logTimeElapsedSinceLastLog("fillHoles2");
	if (stage == 13) {
		output = seg_nohole;
		return 0;
	}

	Mat seg_open = Mat::zeros(seg_nohole.size(), seg_nohole.type());
	Mat disk3 = getStructuringElement(MORPH_ELLIPSE, Size(3,3));
	morphologyEx(seg_nohole, seg_open, CV_MOP_OPEN, disk3, Point(-1, -1), 1);
//	imwrite("test/out-nucleicandidatesopened.ppm", seg_open);
	if (logger) logger->logTimeElapsedSinceLastLog("openBlobs");
	if (stage == 14) {
		output = seg_open;
		return 0;
	}

	/*
	 *
	seg_big = imdilate(bwareaopen(seg_open,30),strel('disk',1));
	 */
	// bwareaopen is done as a area threshold.
	Mat seg_big_t = nscale::bwareaopen<unsigned char>(seg_open, 30, std::numeric_limits<int>::max(), 8);
	if (logger) logger->logTimeElapsedSinceLastLog("30To1000");
	if (stage == 15) {
		output = seg_big_t;
		return 0;
	}

	Mat seg_big = Mat::zeros(seg_big_t.size(), seg_big_t.type());
	dilate(seg_big_t, seg_big, disk3);
	if (stage == 16) {
		output = seg_big;
		return 0;
	}
//	imwrite("test/out-nucleicandidatesbig.ppm", seg_big);
	if (logger) logger->logTimeElapsedSinceLastLog("dilate");

	/*
	 *
		distance = -bwdist(~seg_big);
		distance(~seg_big) = -Inf;
		distance2 = imhmin(distance, 1);
		 *
	 *
	 */
	// distance transform:  matlab code is doing this:
	// invert the image so nuclei candidates are holes
	// compute the distance (distance of nuclei pixels to background)
	// negate the distance.  so now background is still 0, but nuclei pixels have negative distances
	// set background to -inf

	// really just want the distance map.  CV computes distance to 0.
	// background is 0 in output.
	// then invert to create basins
	Mat dist(seg_big.size(), CV_32FC1);

	distanceTransform(seg_big, dist, CV_DIST_L2, CV_DIST_MASK_PRECISE);
	if (stage == 17) {
		output = dist;
		return 0;
	}
	double mmin, mmax;
	minMaxLoc(dist, &mmin, &mmax);
	dist = (mmax + 1.0) - dist;
	if (stage == 18) {
			output = dist;
			return 0;
		}
//	cciutils::cv::imwriteRaw("test/out-dist", dist);

	// then set the background to -inf and do imhmin
	Mat distance = Mat::zeros(dist.size(), dist.type());
	dist.copyTo(distance, seg_big);
//	cciutils::cv::imwriteRaw("test/out-distance", distance);
	// then do imhmin.
	//Mat distance2 = nscale::imhmin<float>(distance, 1.0f, 8);
	//cciutils::cv::imwriteRaw("test/out-distanceimhmin", distance2);

	if (logger) logger->logTimeElapsedSinceLastLog("distTransform");
	if (stage == 19) {
			output = distance;
			return 0;
		}

	/*
	 *
		seg_big(watershed(distance2)==0) = 0;
		seg_nonoverlap = seg_big;
     *
	 */

	Mat nuclei = Mat::zeros(img.size(), img.type());
	img.copyTo(nuclei, seg_big);
	if (logger) logger->logTimeElapsedSinceLastLog("nucleiCopy");
	if (stage == 20) {
			output = nuclei;
			return 0;
		}


	// watershed in openCV requires labels.  input foreground > 0, 0 is background
	Mat watermask = nscale::watershed2(nuclei, distance, 8);
	if (stage == 21) {
			output = watermask;
			return 0;
		}


//	cciutils::cv::imwriteRaw("test/out-watershed", watermask);


	Mat seg_nonoverlap = Mat::zeros(seg_big.size(), seg_big.type());
	seg_big.copyTo(seg_nonoverlap, (watermask > 0));
	if (logger) logger->logTimeElapsedSinceLastLog("watershed");
	if (stage == 22) {
		output = seg_nonoverlap;
		return 0;
	}

//	imwrite("test/out-seg_nonoverlap.ppm", seg_nonoverlap);

	/*
     %CHANGE
    [L] = bwlabel(seg_nonoverlap, 4);
    stats = regionprops(L, 'Area');
    areas = [stats.Area];

    %CHANGE
    ind = find(areas>20 & areas<1000);

    if isempty(ind)
        return;
    end
    seg = ismember(L,ind);
	 *
	 */
	Mat seg = nscale::bwareaopen<unsigned char>(seg_nonoverlap, 21, 1000, 4);
	if (stage == 23) {
		output = seg;
		return 0;
	}
	if (countNonZero(seg) == 0) {
		if (logger) logger->logTimeElapsedSinceLastLog("20To1000");
		//if (logger) logger->endSession();
		return 0;
	}
//	imwrite("test/out-seg.ppm", seg);
	if (logger) logger->logTimeElapsedSinceLastLog("20To1000");
	if (stage == 24) {
		output = seg;
		return 0;
	}


	/*
	 *     %CHANGE
    %[L, num] = bwlabel(seg,8);

    %CHANGE
    [L,num] = bwlabel(imfill(seg, 'holes'),4);
	 *
	 */
	// don't worry about bwlabel.
	output = nscale::imfillHoles<unsigned char>(seg, true, 8);
	if (logger) logger->logTimeElapsedSinceLastLog("fillHolesLast");

//	imwrite("test/out-nuclei.ppm", seg);


	//if (logger) logger->endSession();

	return 0;
}
#endif
}}
