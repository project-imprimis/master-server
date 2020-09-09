#include <ctime>
#include "shims.h"

std::string tools::strtime(const std::time_t *time) {
    std::string _time = std::asctime(std::localtime(time));

    if(_time.back() == '\n') _time.pop_back();
    return _time;
}