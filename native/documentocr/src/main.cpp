/*
#include "main.h"
#include "im.h"
#include "jzon.h"

using namespace eigengo::cm;

Main::Main(const std::string queue, const std::string exchange, const std::string routingKey) :
RabbitRpcServer::RabbitRpcServer(queue, exchange, routingKey) {
    
}

std::string Main::handleMessage(const AmqpClient::BasicMessage::ptr_t message, const AmqpClient::Channel::ptr_t channel) {
    return "{}";
}

int main(int argc, char** argv) {
    Main main("cm.document.queue", "cm.exchange", "cm.document.*.key");
    main.runAndJoin(8);
    return 0;
}
*/

//#define WITH_CUDA

#include <iostream>
#include <iomanip>
#include <string>
#include <ctype.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/contrib/contrib.hpp>
#include <opencv2/superres/superres.hpp>
#include <opencv2/superres/optical_flow.hpp>
#include <opencv2/video/video.hpp>
#include <opencv2/opencv_modules.hpp>

using namespace std;
using namespace cv;
using namespace cv::superres;

#define FOURCC(a,b,c,d) ( (uint) (((d)<<24) | ((c)<<16) | ((b)<<8) | (a)) )

#define MEASURE_TIME(op) \
{ \
TickMeter tm; \
tm.start(); \
op; \
tm.stop(); \
cout << tm.getTimeSec() << " sec" << endl; \
}

static Ptr<DenseOpticalFlowExt> createOptFlow(const string& name)
{
    if (name == "farneback") {
#ifdef WITH_CUDA
        return createOptFlow_Farneback_CUDA();
#else
        return createOptFlow_Farneback();
#endif
    }
    else if (name == "simple") return createOptFlow_Simple();
    else if (name == "tvl1") {
#ifdef WITH_CUDA
        return cv::superres::createOptFlow_DualTVL1_CUDA();
#else
        return cv::superres::createOptFlow_DualTVL1();
#endif
    }
#ifdef WITH_CUDA 
    else if (name == "brox") return createOptFlow_Brox_CUDA();
    else if (name == "pyrlk") return createOptFlow_PyrLK_CUDA();
#endif
    else cerr << "Incorrect Optical Flow algorithm - " << name << endl;
    
    return Ptr<DenseOpticalFlowExt>();
}

int main(int argc, const char* argv[])
{
    CommandLineParser cmd(argc, argv,
                          "{ v | video      |           | Input video }"
                          "{ o | output     |           | Output video }"
                          "{ s | scale      | 4         | Scale factor }"
                          "{ i | iterations | 20        | Iteration count }"
                          "{ t | temporal   | 8         | Radius of the temporal search area }"
                          "{ f | flow       | farneback | Optical flow algorithm (farneback, simple, tvl1, brox, pyrlk) }"
                          "{ h | help       | false     | Print help message }"
                          );
    
    if (cmd.get<bool>("help")) {
        cout << "This sample demonstrates Super Resolution algorithms for video sequence" << endl;
        cmd.printParams();
        return EXIT_SUCCESS;
    }
    
    const string inputVideoName = cmd.get<string>("video");
    const string outputVideoName = cmd.get<string>("output");
    const int scale = cmd.get<int>("scale");
    const int iterations = cmd.get<int>("iterations");
    const int temporalAreaRadius = cmd.get<int>("temporal");
    const string optFlow = cmd.get<string>("flow");
    
    bool useCamera = inputVideoName.compare("*") == 0;
    Ptr<SuperResolution> superRes;
    
#ifdef WITH_CUDA
    superRes = createSuperResolution_BTVL1_CUDA();
#else
    superRes = createSuperResolution_BTVL1();
#endif
    
    Ptr<DenseOpticalFlowExt> of = createOptFlow(optFlow);
    
    if (of.empty()) return EXIT_FAILURE;
    
    superRes->set("opticalFlow", of);
    superRes->set("scale", scale);
    superRes->set("iterations", iterations);
    superRes->set("temporalAreaRadius", temporalAreaRadius);
    
    Ptr<FrameSource> frameSource;
    if (useCamera) frameSource = createFrameSource_Camera();
    else {
#ifdef WITH_CUDA
        // Try to use gpu Video Decoding
        try  {
            frameSource = createFrameSource_Video_CUDA(inputVideoName);
            Mat frame;
            frameSource->nextFrame(frame);
        } catch (const cv::Exception&) {
            frameSource.release();
        }
#endif
        if (!frameSource) frameSource = createFrameSource_Video(inputVideoName);
    }
    
    // skip first frame, it is usually corrupted
    {
        Mat frame;
        frameSource->nextFrame(frame);
        cout << "Input           : " << inputVideoName << " " << frame.size() << endl;
        cout << "Scale factor    : " << scale << endl;
        cout << "Iterations      : " << iterations << endl;
        cout << "Temporal radius : " << temporalAreaRadius << endl;
        cout << "Optical Flow    : " << optFlow << endl;
#ifdef WITH_CUDA
        cout << "CUDA            : Yes" << endl;
#endif
        cout << "Camera          : " << (useCamera ? "Yes" : "No") << endl;
    }
    
    superRes->setInput(frameSource);
    
    VideoWriter writer;
    
    for (int i = 0;; ++i) {
        cout << '[' << setw(3) << i << "] : ";
        Mat result;
        
        MEASURE_TIME(superRes->nextFrame(result));
        
        if (result.empty()) break;
        
        imshow("Super Resolution", result);
        if (waitKey(1000) > 0) break;
        
        if (!outputVideoName.empty()) {
            if (!writer.isOpened()) writer.open(outputVideoName, FOURCC('X', 'V', 'I', 'D'), 25.0, result.size());
            writer << result;
        }
    }
    
    return 0;
}