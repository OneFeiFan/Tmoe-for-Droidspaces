/** TUI 插件化框架单元测试 */
#include <doctest/doctest.h>
#include "ui/menu_item.h"
#include "ui/action.h"
#include "ui/menu.h"
#include "ui/registry.h"
#include "ui/builtin_actions.h"

using namespace tmoe;
using namespace tmoe::ui;

// ── 测试用具体类 ──────────────────────────────

class TestAction : public IAction {
    std::string label_, tag_;
    bool* executed_;
public:
    TestAction(std::string label, std::string tag, bool* exec_flag = nullptr)
        : label_(std::move(label)), tag_(std::move(tag)), executed_(exec_flag) {}

    std::string get_label() const override { return label_; }
    std::string get_tag() const override { return tag_; }
    bool execute(MenuContext&) override {
        if (executed_) *executed_ = true;
        return true;
    }
    bool needs_root() const override { return false; }
};

class TestMenu : public IUIMenu {
    std::string title_, label_, tag_;
public:
    TestMenu(std::string title, std::string label, std::string tag)
        : title_(std::move(title)), label_(std::move(label)), tag_(std::move(tag)) {}

    std::string get_label() const override { return label_; }
    std::string get_tag() const override { return tag_; }
    std::string get_title() const override { return title_; }
};

// ── 测试用例 ─────────────────────────────────

TEST_CASE("IMenuItem: BackAction builtin") {
    BackAction back;
    CHECK(back.get_tag() == "back");
    CHECK(back.needs_root() == false);
}

TEST_CASE("IMenuItem: ExitAction builtin") {
    ExitAction exit;
    CHECK(exit.get_tag() == "exit");
    CHECK(exit.needs_root() == false);
}

TEST_CASE("IUIMenu: add_child and find_child") {
    auto menu = std::make_shared<TestMenu>("Test", "Test Menu", "test");
    auto action = std::make_shared<TestAction>("Hello", "hello");

    menu->add_child(action);
    CHECK(menu->children().size() == 1);

    auto found = menu->find_child("hello");
    CHECK(found != nullptr);
    CHECK(found->get_label() == "Hello");

    auto not_found = menu->find_child("nonexistent");
    CHECK(not_found == nullptr);
}

TEST_CASE("IUIMenu: batch add_children") {
    auto menu = std::make_shared<TestMenu>("Root", "Root", "root");
    menu->add_children({
        std::make_shared<TestAction>("A", "a"),
        std::make_shared<TestAction>("B", "b"),
        std::make_shared<TestAction>("C", "c"),
    });
    CHECK(menu->children().size() == 3);
    CHECK(menu->find_child("a")->get_label() == "A");
    CHECK(menu->find_child("b")->get_label() == "B");
    CHECK(menu->find_child("c")->get_label() == "C");
}

TEST_CASE("IUIMenu: nested sub-menus") {
    auto root = std::make_shared<TestMenu>("Root", "Root", "root");
    auto sub = std::make_shared<TestMenu>("Sub", "Sub Menu", "sub");
    auto action = std::make_shared<TestAction>("Do It", "do");

    sub->add_child(action);
    root->add_child(sub);

    // 查询嵌套
    auto found_sub = root->find_child("sub");
    CHECK(found_sub != nullptr);

    auto sub_menu = std::dynamic_pointer_cast<IUIMenu>(found_sub);
    CHECK(sub_menu != nullptr);
    CHECK(sub_menu->find_child("do") != nullptr);
}

TEST_CASE("IAction: execution callback") {
    bool flag = false;
    auto action = std::make_shared<TestAction>("Test", "test", &flag);

    TmoeConfig cfg;
    MenuContext ctx{cfg};

    CHECK(flag == false);
    action->execute(ctx);
    CHECK(flag == true);
}

TEST_CASE("MenuRegistry: register and retrieve") {
    MenuRegistry::clear();

    MenuRegistry::register_item(std::make_shared<TestAction>("Item1", "i1"));
    MenuRegistry::register_item(std::make_shared<TestAction>("Item2", "i2"));

    auto items = MenuRegistry::items();
    CHECK(items.size() == 2);
    CHECK(items[0]->get_tag() == "i1");
    CHECK(items[1]->get_tag() == "i2");

    MenuRegistry::clear();
    CHECK(MenuRegistry::items().empty());
}

TEST_CASE("add_navigation_items: adds back and exit") {
    auto menu = std::make_shared<TestMenu>("Nav", "Nav Test", "nav");
    add_navigation_items(menu);

    CHECK(menu->children().size() == 2);
    CHECK(menu->find_child("back") != nullptr);
    CHECK(menu->find_child("exit") != nullptr);
}
