// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
// Tencent is pleased to support the open source community by making WeChat QRCode available.
// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.

#include "decodermgr.hpp"
#include "opencv2/core.hpp"
#include "opencv2/core/utils/filesystem.hpp"
#include "opencv2/wechat_qrcode.hpp"
#include "detector/align.hpp"
#include "detector/ssd_detector.hpp"
#include "scale/super_scale.hpp"
#include "precomp.hpp"
#include "zxing/result.hpp"
using cv::InputArray;
namespace cv {
namespace wechat_qrcode {
class QRCodeDetector::Impl
{
public:
    Impl() {}
    ~Impl() {}
    /**
     * @brief detect QR codes from the given image
     *
     * @param img supports grayscale or color (BGR) image.
     * @return vector<Mat> detected QR code bounding boxes.
     */
    std::vector<Mat> detect(const Mat& img);
    /**
     * @brief decode QR codes from detected points
     *
     * @param img supports grayscale or color (BGR) image.
     * @param candidate_points detected points. we name it "candidate points" which means no
     * all the qrcode can be decoded.
     * @param points succussfully decoded qrcode with bounding box points.
     * @return vector<string>
     */
    std::vector<std::string> decode(const Mat& img, std::vector<Mat>& candidate_points, std::vector<Mat>& points);
    int applyDetector(const Mat& img, std::vector<Mat>& points);
    Mat cropObj(const Mat& img, const Mat& point, Align& aligner);
    std::vector<float> getScaleList(const int width, const int height);
    std::shared_ptr<SSDDetector> detector_;
    std::shared_ptr<SuperScale> super_resolution_model_;
    bool use_nn_detector_, use_nn_sr_;
};



QRCodeDetector::QRCodeDetector(const String& detector_prototxt_path,
                               const String& detector_caffe_model_path,
                               const String& super_resolution_prototxt_path,
                               const String& super_resolution_caffe_model_path) {
    p = makePtr<QRCodeDetector::Impl>();
    if (!detector_caffe_model_path.empty() && !detector_prototxt_path.empty()) {
        // initialize detector model (caffe)
        p->use_nn_detector_ = true;
        CV_CheckEQ(utils::fs::exists(detector_prototxt_path), true,
                   "fail to find detector caffe prototxt file");
        CV_CheckEQ(utils::fs::exists(detector_caffe_model_path), true,
                   "fail to find detector caffe model file");
        p->detector_ = make_shared<SSDDetector>();
        auto ret = p->detector_->init(detector_prototxt_path, detector_caffe_model_path);
        CV_CheckEQ(ret, 0, "fail to load the detector model.");
    } else {
        p->use_nn_detector_ = false;
        p->detector_ = NULL;
    }
    // initialize super_resolution_model
    // it could also support non model weights by cubic resizing
    // so, we initialize it first.
    p->super_resolution_model_ = make_shared<SuperScale>();
    if (!super_resolution_prototxt_path.empty() && !super_resolution_caffe_model_path.empty()) {
        p->use_nn_sr_ = true;
        // initialize dnn model (onnx format)
        CV_CheckEQ(utils::fs::exists(super_resolution_prototxt_path), true,
                   "fail to find super resolution prototxt model file");
        CV_CheckEQ(utils::fs::exists(super_resolution_caffe_model_path), true,
                   "fail to find super resolution caffe model file");
        auto ret = p->super_resolution_model_->init(super_resolution_prototxt_path,
                                                 super_resolution_caffe_model_path);
        CV_CheckEQ(ret, 0, "fail to load the super resolution model.");
    } else {
        p->use_nn_sr_ = false;
    }
}

vector<string> QRCodeDetector::detectAndDecode(InputArray img, OutputArrayOfArrays points) {
    CV_Assert(!img.empty());
    CV_CheckDepthEQ(img.depth(), CV_8U, "");

    if (img.cols() <= 20 || img.rows() <= 20) {
        return vector<string>();  // image data is not enough for providing reliable results
    }
    Mat input_img;
    int incn = img.channels();
    CV_Check(incn, incn == 1 || incn == 3 || incn == 4, "");
    if (incn == 3 || incn == 4) {
        cv::cvtColor(img, input_img, cv::COLOR_BGR2GRAY);
    } else {
        input_img = img.getMat();
    }
    auto candidate_points = p->detect(input_img);
    auto res_points = vector<Mat>();
    auto ret = p->decode(input_img, candidate_points, res_points);
    // opencv type convert
    vector<Mat> tmp_points;
    if (points.needed()) {
        for (size_t i = 0; i < res_points.size(); i++) {
            Mat tmp_point;
            tmp_points.push_back(tmp_point);
            res_points[i].convertTo(((OutputArray)tmp_points[i]),
                                    ((OutputArray)tmp_points[i]).fixedType()
                                        ? ((OutputArray)tmp_points[i]).type()
                                        : CV_32FC2);
        }
        points.createSameSize(tmp_points, CV_32FC2);
        points.assign(tmp_points);
    }
    return ret;
};

vector<string> QRCodeDetector::Impl::decode(const Mat& img, vector<Mat>& candidate_points,
                                      vector<Mat>& points) {
    if (candidate_points.size() == 0) {
        return vector<string>();
    }
    vector<string> decode_results;
    for (auto& point : candidate_points) {
        cv::Mat cropped_img;
        if (use_nn_detector_) {
            Align aligner;
            cropped_img = cropObj(img, point, aligner);
        } else {
            cropped_img = img;
        }
        // scale_list contains different scale ratios
        auto scale_list = getScaleList(cropped_img.cols, cropped_img.rows);
        for (auto cur_scale : scale_list) {
            cv::Mat scaled_img =
                super_resolution_model_->processImageScale(cropped_img, cur_scale, use_nn_sr_);
            string result;
            DecoderMgr decodemgr;
            auto ret = decodemgr.decodeImage(scaled_img, use_nn_detector_, result);

            if (ret == 0) {
                decode_results.push_back(result);
                points.push_back(point);
                break;
            }
        }
    }

    return decode_results;
}

vector<Mat> QRCodeDetector::Impl::detect(const Mat& img) {
    auto points = vector<Mat>();

    if (use_nn_detector_) {
        // use cnn detector
        auto ret = applyDetector(img, points);
        CV_CheckEQ(ret, 0, "fail to apply detector.");
    } else {
        auto width = img.cols, height = img.rows;
        // if there is no detector, use the full image as input
        auto point = Mat(4, 2, CV_32FC1);
        point.at<float>(0, 0) = 0;
        point.at<float>(0, 1) = 0;
        point.at<float>(1, 0) = width - 1;
        point.at<float>(1, 1) = 0;
        point.at<float>(2, 0) = width - 1;
        point.at<float>(2, 1) = height - 1;
        point.at<float>(3, 0) = 0;
        point.at<float>(3, 1) = height - 1;
        points.push_back(point);
    }
    return points;
}

int QRCodeDetector::Impl::applyDetector(const cv::Mat& img, vector<Mat>& points) {
    int img_w = img.cols;
    int img_h = img.rows;

    // hard code input size
    int minInputSize = 400;
    float resizeRatio = sqrt(img_w * img_h * 1.0 / (minInputSize * minInputSize));
    int detect_width = img_w / resizeRatio;
    int detect_height = img_h / resizeRatio;

    points = detector_->forward(img, detect_width, detect_height);

    return 0;
}

cv::Mat QRCodeDetector::Impl::cropObj(const cv::Mat& img, const Mat& point, Align& aligner) {
    // make some padding to boost the qrcode details recall.
    float padding_w = 0.1, padding_h = 0.1;
    auto min_padding = 15;
    auto cropped = aligner.crop(img, point, padding_w, padding_h, min_padding);
    return cropped;
}

// empirical rules
vector<float> QRCodeDetector::Impl::getScaleList(const int width, const int height) {
    if (width < 320 || height < 320) return {1.0, 2.0, 0.5};
    if (width < 640 && height < 640) return {1.0, 0.5};
    return {0.5, 1.0};
}
}  // namespace wechat_qrcode
}  // namespace cv