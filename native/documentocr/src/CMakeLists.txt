SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..)

FILE(GLOB MainSources *.cpp)
FILE(GLOB PreprocessorSources preprocessor/*.cpp)
FILE(GLOB SplitterSources splitter/*.cpp)

ADD_EXECUTABLE(documentocr ${MainSources} ${PreprocessorSources} ${SplitterSources} ../../../jzon.cpp ../../../rabbit.cpp ../../../im.cpp)
TARGET_LINK_LIBRARIES(documentocr ${Rabbitmqc_LIBRARY} ${Boost_LIBRARIES} ${SimpleAmqpClient_LIBRARY} ${OpenCV_LIBS})
