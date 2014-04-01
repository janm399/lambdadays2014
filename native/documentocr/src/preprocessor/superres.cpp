#include "superres.h"
#include <opencv2/superres/superres.hpp>

using namespace eigengo::cm;

cv::Mat SuperresProcessor::preprocess(const std::vector<cv::Mat &> &sources) const {
    return cv::Mat();
}