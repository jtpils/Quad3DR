//==================================================
// video_source_zed.cpp
//
//  Copyright (c) 2016 Benjamin Hepp.
//  Author: Benjamin Hepp
//  Created on: Aug 29, 2016
//==================================================

#include "video_source_zed.h"
#include <algorithm>

namespace video
{

VideoSourceZED::VideoSourceZED(sl::zed::SENSING_MODE sensing_mode, bool compute_disparity, bool compute_measure)
: camera_(nullptr),
  sensing_mode_(sensing_mode), compute_disparity_(compute_disparity), compute_measure_(compute_measure),
  initialized_(false)
{
	init_params_.unit = sl::zed::UNIT::METER;
}

VideoSourceZED::~VideoSourceZED()
{
  close();
}

void VideoSourceZED::ensureInitialized() const
{
  if (!initialized_)
  {
    throw VideoSource::Error("Video source has not been initialized");
  }
}

const sl::zed::InitParams& VideoSourceZED::getInitParameters() const
{
  return init_params_;
}

sl::zed::InitParams& VideoSourceZED::getInitParameters()
{
  return init_params_;
}

void VideoSourceZED::init()
{
  sl::zed::ERRCODE err = camera_->init(init_params_);
  if (err != sl::zed::SUCCESS)
  {
    std::cerr << "ZED init error code : " << sl::zed::errcode2str(err) << std::endl;
    throw Error("Unable to initialize ZED camera");
  }
  initialized_ = true;
}

void VideoSourceZED::open(sl::zed::ZEDResolution_mode mode)
{
  close();
  camera_ = new sl::zed::Camera(mode);
  init();
}

void VideoSourceZED::open(const std::string &svo_filename)
{
  close();
  camera_ = new sl::zed::Camera(svo_filename);
  init();
}

void VideoSourceZED::close()
{
  if (camera_ != nullptr)
  {
    if (initialized_)
    {
      // Grab frame to prevent error on destruction (Bug in SDK)
      camera_->grab(sensing_mode_, compute_measure_, compute_disparity_, compute_pointcloud_);
    }
    delete camera_;
    camera_ = nullptr;
    initialized_ = false;
  }
}

sl::zed::Camera* VideoSourceZED::getNativeCamera()
{
  return camera_;
}

const sl::zed::Camera* VideoSourceZED::getNativeCamera()const
{
  return camera_;
}

bool VideoSourceZED::loadParameters(const std::string &filename)
{
  return init_params_.load(filename);
}

void VideoSourceZED::saveParameters(const std::string &filename)
{
  init_params_.save(filename);
}

double VideoSourceZED::getFPS() const
{
  ensureInitialized();
  return camera_->getCurrentFPS();
}

bool VideoSourceZED::setFPS(double fps)
{
  ensureInitialized();
  return camera_->setFPS(static_cast<int>(fps));
}

bool VideoSourceZED::has_depth() const
{
  return compute_measure_;
}

bool VideoSourceZED::has_stereo() const
{
  return true;
}

int VideoSourceZED::getWidth() const
{
  ensureInitialized();
  return camera_->getImageSize().width;
}

int VideoSourceZED::getHeight() const
{
  ensureInitialized();
  return camera_->getImageSize().height;
}

bool VideoSourceZED::grab(bool block)
{
  ensureInitialized();
  if (block)
  {
    bool old_image = true;
    while (old_image)
    {
      old_image = camera_->grab(sensing_mode_, compute_measure_, compute_disparity_, compute_pointcloud_);
    }
    return true;
  }
  else
  {
    return camera_->grab(sensing_mode_, compute_measure_, compute_disparity_, compute_pointcloud_);
  }
}

void VideoSourceZED::copyZedMatToCvMat(const sl::zed::Mat &zed_mat, cv::Mat *cv_mat) const
{
  assert(zed_mat.getDataSize() == cv_mat->elemSize1());
  int data_size = zed_mat.width * zed_mat.height * zed_mat.channels * zed_mat.getDataSize();
  std::copy(zed_mat.data, zed_mat.data + data_size, cv_mat->data);
  // For debugging: Copy manually
//  for (int row=0; row < zed_mat.height; ++row)
//  {
//    int row_offset = row * zed_mat.width * zed_mat.channels;
//    for (int col=0; col < zed_mat.width; ++col)
//    {
//      int row_col_offset = row_offset + col * zed_mat.channels;
//      for (int channel=0; channel < zed_mat.channels; ++channel)
//      {
//        cv_mat->data[row_col_offset + channel] = zed_mat.data[row_col_offset + channel];
//      }
//    }
//  }
}

bool VideoSourceZED::retrieveSide(cv::Mat *cv_mat, sl::zed::SIDE side)
{
  ensureInitialized();
  sl::zed::Mat zed_mat = camera_->retrieveImage(side);
  if (cv_mat->empty())
  {
    *cv_mat = cv::Mat(zed_mat.height, zed_mat.width, CV_8UC4);
  }
  copyZedMatToCvMat(zed_mat, cv_mat);
  // ZED SDK function for converting ZED image to CV
  //  *cv_mat = sl::zed::slMat2cvMat(zed_mat);
  return true;
}

bool VideoSourceZED::retrieveNormalizedMeasure(cv::Mat *cv_mat, sl::zed::MEASURE measure)
{
	ensureInitialized();
	if (measure == sl::zed::MEASURE::DISPARITY)
	{
		if (!compute_disparity_)
		{
			throw Error("Disparity map was not computed");
		}
	}
	else if (!compute_measure_)
	{
		throw Error("Depth map measures were not computed");
	}

	sl::zed::Mat zed_mat = camera_->normalizeMeasure(measure);
	if (cv_mat->empty())
	{
		(*cv_mat) = cv::Mat(zed_mat.height, zed_mat.width, CV_8UC4);
	}
	copyZedMatToCvMat(zed_mat, cv_mat);
	return true;
}

bool VideoSourceZED::retrieveMeasure(cv::Mat *cv_mat, sl::zed::MEASURE measure)
{
  ensureInitialized();
  if (measure == sl::zed::MEASURE::DISPARITY)
  {
    if (!compute_disparity_)
    {
      throw Error("Disparity map was not computed");
    }
  }
  else if (!compute_measure_)
  {
    throw Error("Depth map measures were not computed");
  }

  sl::zed::Mat zed_mat = camera_->retrieveMeasure(measure);
  if (cv_mat->empty())
  {
    (*cv_mat) = cv::Mat(zed_mat.height, zed_mat.width, CV_32FC1);
  }
  copyZedMatToCvMat(zed_mat, cv_mat);
  return true;
}

bool VideoSourceZED::retrieveMono(cv::Mat *mat)
{
  return retrieveLeft(mat);
}

bool VideoSourceZED::retrieveLeft(cv::Mat *mat)
{
  return retrieveSide(mat, sl::zed::SIDE::LEFT);
}

bool VideoSourceZED::retrieveRight(cv::Mat *mat)
{
  return retrieveSide(mat, sl::zed::SIDE::RIGHT);
}

bool VideoSourceZED::retrieveDepth(cv::Mat *mat)
{
  return retrieveNormalizedMeasure(mat, sl::zed::MEASURE::DEPTH);
}

bool VideoSourceZED::retrieveDepthFloat(cv::Mat *mat)
{
	return retrieveMeasure(mat, sl::zed::MEASURE::DEPTH);
}

bool VideoSourceZED::retrieveDisparity(cv::Mat *mat)
{
	return retrieveNormalizedMeasure(mat, sl::zed::MEASURE::DISPARITY);
}

bool VideoSourceZED::retrieveDisparityFloat(cv::Mat *mat)
{
	return retrieveMeasure(mat, sl::zed::MEASURE::DISPARITY);
}

bool VideoSourceZED::retrieveConfidence(cv::Mat *mat)
{
	return retrieveNormalizedMeasure(mat, sl::zed::MEASURE::CONFIDENCE);
}

bool VideoSourceZED::retrieveConfidenceFloat(cv::Mat *mat)
{
	return retrieveMeasure(mat, sl::zed::MEASURE::CONFIDENCE);
}

} /* namespace video */