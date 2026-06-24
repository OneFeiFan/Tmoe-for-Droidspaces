#ifndef TERMINAL_APP_H
#define TERMINAL_APP_H
#pragma once
#include <string>
#include <vector>
#include "core/config.h"
#include "domain/system/package_manager.h"

namespace tmoe::domain {

class TerminalAppManager {
public:
    explicit TerminalAppManager(const TmoeConfig& cfg);
    void run_terminal_menu();

private:
    const TmoeConfig& cfg_;
    DistroFamily family_;
};

} // namespace tmoe::domain
#endif
