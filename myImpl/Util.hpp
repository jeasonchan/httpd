//
// Created by chenr on 2020/11/5.
//

#ifndef HTTPD_UTIL_HPP
#define HTTPD_UTIL_HPP

#include <iostream>
#include <string>
#include <cstring>

namespace jeason {
    class Util {
    public:
        static inline void exit_with_flag_errInfo(std::string &&customFlag) {
            std::cerr << "customFlag:" << customFlag << std::endl
                      << "err no:" << errno << std::endl
                      << "error info:" << strerror(errno) << std::endl;
            exit(errno);
        }

    };


}


#endif //HTTPD_UTIL_HPP
