/** DistroFamily 枚举和字符串映射测试 */
#include <doctest/doctest.h>
#include "core/config.h"

using tmoe::DistroFamily;

TEST_CASE("DistroFamily: enum values are distinct") {
    // 验证所有枚举值互不相同
    CHECK(DistroFamily::Debian != DistroFamily::Arch);
    CHECK(DistroFamily::Arch != DistroFamily::RedHat);
    CHECK(DistroFamily::RedHat != DistroFamily::Alpine);
    CHECK(DistroFamily::Alpine != DistroFamily::Gentoo);
    CHECK(DistroFamily::Gentoo != DistroFamily::Suse);
    CHECK(DistroFamily::Suse != DistroFamily::Solus);
    CHECK(DistroFamily::Solus != DistroFamily::Void_);
    CHECK(DistroFamily::Void_ != DistroFamily::Slackware);
    CHECK(DistroFamily::Slackware != DistroFamily::OpenWrt);
    CHECK(DistroFamily::OpenWrt != DistroFamily::Unknown);
}

TEST_CASE("DistroFamily: Unknown is the default") {
    // 验证 Unknown 是默认值
    DistroFamily df{};
    CHECK(df == DistroFamily::Unknown);
}

TEST_CASE("DistroFamily: TmoeConfig default distro_family") {
    // TmoeConfig 默认 distro_family 应为 Unknown
    tmoe::TmoeConfig cfg;
    CHECK(cfg.distro_family == DistroFamily::Unknown);
}
