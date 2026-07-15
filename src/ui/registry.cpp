#include "registry.h"
#include <mutex>

namespace tmoe::ui {

std::vector<std::shared_ptr<IMenuItem>>& MenuRegistry::registry() {
    static std::vector<std::shared_ptr<IMenuItem>> reg;
    return reg;
}

std::mutex& MenuRegistry::mutex() {
    static std::mutex mtx;
    return mtx;
}

void MenuRegistry::register_item(std::shared_ptr<IMenuItem> item) {
    std::lock_guard<std::mutex> lock(mutex());
    registry().push_back(std::move(item));
}

std::vector<std::shared_ptr<IMenuItem>> MenuRegistry::items() {
    std::lock_guard<std::mutex> lock(mutex());
    return registry();
}

void MenuRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex());
    registry().clear();
}

} // namespace tmoe::ui
