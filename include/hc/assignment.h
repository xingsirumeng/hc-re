#pragma once
#include "submission.h"
#include "time-defs.h"
#include <nlohmann/json.hpp>
#include <string>

// We use UTC time, so if not using them, convert them into UTC time.

struct Assignment {
  public:
    std::string name;
    TimePoint start_time;
    TimePoint end_time;

    // StudentID -> Submission
    std::unordered_map<std::string, Submission> submissions;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Assignment, name, start_time, end_time,
                                   submissions);
};
