/** 插件辅助工具单元测试 */
#include <doctest/doctest.h>
#include "ui/plugin_helpers.h"

using namespace tmoe;
using namespace tmoe::ui;

TEST_CASE("LambdaAction: basic execution") {
    int called = 0;
    auto action = std::make_shared<LambdaAction>(
        "Test", "test",
        [&called](MenuContext&) -> bool { called++; return true; });

    CHECK(action->get_label() == "Test");
    CHECK(action->get_tag() == "test");

    TmoeConfig cfg;
    MenuContext ctx{cfg};
    action->execute(ctx);
    CHECK(called == 1);
}

TEST_CASE("LambdaAction::make: simplified factory") {
    int called = 0;
    auto action = LambdaAction::make("Label", "tag", [&called]() { called++; });

    TmoeConfig cfg;
    MenuContext ctx{cfg};
    action->execute(ctx);
    CHECK(called == 1);
}

TEST_CASE("make_plugin_menu: creates SimpleMenu without navigation") {
    auto menu = make_plugin_menu("Title", "Label", "tag");
    CHECK(menu->get_title() == "Title");
    CHECK(menu->get_label() == "Label");
    CHECK(menu->get_tag() == "tag");
    // make_plugin_menu 只创建空容器，导航项由调用方显式添加
    CHECK(menu->children().empty());
}

TEST_CASE("add_sandwich_nav: inserts back at top, exit at bottom") {
    auto menu = make_plugin_menu("T", "L", "t");
    menu->add_child(std::make_shared<LambdaAction>("Item", "item",
        [](MenuContext&) -> bool { return true; }));
    add_sandwich_nav(menu);
    // 夹心饼：back 在索引 0，内容在中间，exit 在末尾
    CHECK(menu->children().size() == 3);
    CHECK(menu->children()[0]->get_tag() == "back");
    CHECK(menu->find_child("item") != nullptr);
    CHECK(menu->find_child("exit") != nullptr);
}

TEST_CASE("AutoRegister: registers item in MenuRegistry") {
    MenuRegistry::clear();
    CHECK(MenuRegistry::items().empty());

    {
        AutoRegister reg(std::make_shared<LambdaAction>("X", "x",
            [](MenuContext&) -> bool { return true; }));
        auto items = MenuRegistry::items();
        CHECK(items.size() == 1);
        CHECK(items[0]->get_tag() == "x");
    }

    // AutoRegister 析构不会取消注册（设计决策：菜单生命周期与程序一致）
    CHECK(MenuRegistry::items().size() == 1);
    MenuRegistry::clear();
}
