#ifndef EDUCATION_H
#define EDUCATION_H
#pragma once
#include "core/config.h"
#include <string>

namespace tmoe::domain {

class EducationManager {
public:
    explicit EducationManager(const TmoeConfig& cfg);
    void run_education_menu();

private:
    void run_math_menu();
    void run_chemistry_menu();
    void run_physics_menu();
    void run_english_menu();
    void run_exam_menu();
    void run_gaokao_menu();
    void run_kaoyan_menu();
    void download_exam_data(const std::string& desc, const std::string& gitee_url);

    const TmoeConfig& cfg_;
};

} // namespace tmoe::domain
#endif
