#ifndef main_h
#define main_h

#include <inttypes.h>
#include <memory>
#include "rabbit.h"
#include "preprocessor.h"

namespace eigengo { namespace cm {
    
    class Main : public RabbitRpcServer {
    private:
        std::unique_ptr<DocumentPreprocessor> preprocessor;
    protected:
        virtual std::string handleMessage(const AmqpClient::BasicMessage::ptr_t message, const AmqpClient::Channel::ptr_t channel);
    public:
        Main(const std::string queue, const std::string exchange, const std::string routingKey);
    };
  
} }
#endif
