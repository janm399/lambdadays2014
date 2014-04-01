#ifndef preprocessor_h
#define preprocessor_h

#include <opencv2/opencv.hpp>
#include <vector>

namespace eigengo { namespace cm {
    
    class DocumentPreprocessor {
    public:
        virtual cv::Mat preprocess(const std::vector<cv::Mat&> &sources) const = 0;
    };
    
} };


#endif