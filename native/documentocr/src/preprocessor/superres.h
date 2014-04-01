#ifndef superres_processor_h
#define superres_processor_h

#include <opencv2/opencv.hpp>
#include <vector>
#include <boost/optional.hpp>
#include "../preprocessor.h"

namespace eigengo { namespace cm {

    
    /**
     * Implements the coin counter on both the CPU as well as the GPU [using OpenCL].
     */
    class SuperresProcessor : public DocumentPreprocessor {
    public:
        virtual cv::Mat preprocess(const std::vector<cv::Mat&> &sources) const;
    };
            
}
}


#endif