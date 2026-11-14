// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <grains/SamplePathResolver.h>

#include <string>
#include <vector>

using grains::PathAnchor;
using grains::SampleRef;
using Res = grains::SamplePathResolver;

// 2.1 shape and names ---------------------------------------------------------------------------

TEST_CASE("sample_path: Windows-shaped paths are detected from the text", "[sample_path]")
{
    CHECK(Res::isWindowsShaped(R"(C:\x\kick.wav)"));
    CHECK(Res::isWindowsShaped("c:/x/kick.wav"));
    CHECK(Res::isWindowsShaped(R"(\\srv\sh\kick.wav)"));
    CHECK_FALSE(Res::isWindowsShaped("/x/kick.wav"));
    CHECK_FALSE(Res::isWindowsShaped("C:kick.wav"));
    CHECK_FALSE(Res::isWindowsShaped("kick.wav"));
    CHECK_FALSE(Res::isWindowsShaped(""));
}

TEST_CASE("sample_path: isAbsolute follows the path shape", "[sample_path]")
{
    CHECK(Res::isAbsolute(R"(C:\x\kick.wav)"));
    CHECK(Res::isAbsolute("C:/x/kick.wav"));
    CHECK(Res::isAbsolute(R"(\\srv\sh\kick.wav)"));
    CHECK(Res::isAbsolute("/x/kick.wav"));
    CHECK_FALSE(Res::isAbsolute("C:kick.wav"));
    CHECK_FALSE(Res::isAbsolute("loops/kick.wav"));
    CHECK_FALSE(Res::isAbsolute("kick.wav"));
    CHECK_FALSE(Res::isAbsolute(""));
}

TEST_CASE("sample_path: fileName splits on the separators of the path shape", "[sample_path]")
{
    CHECK(Res::fileName(R"(C:\x\kick.wav)") == "kick.wav");
    CHECK(Res::fileName(R"(C:/x\y/kick.wav)") == "kick.wav");
    CHECK(Res::fileName(R"(\\srv\sh\kick.wav)") == "kick.wav");
    CHECK(Res::fileName("/x/kick.wav") == "kick.wav");
    CHECK(Res::fileName("kick.wav") == "kick.wav");
    CHECK(Res::fileName(R"(loops\kick.wav)") == "kick.wav");
    CHECK(Res::fileName(R"(C:\x\)").empty());
    CHECK(Res::fileName("/x/y/").empty());
    CHECK(Res::fileName(R"(/home/a/we\ird.wav)") == R"(we\ird.wav)");
}

// 2.2 samePath ----------------------------------------------------------------------------------

TEST_CASE("sample_path: samePath ignores case and separator style on Windows paths", "[sample_path]")
{
    CHECK(Res::samePath(R"(C:\Music\Kick.wav)", "c:/music/kick.WAV"));
    CHECK(Res::samePath(R"(C:\a\\b\k.wav)", R"(C:\a\b\k.wav)"));
    CHECK(Res::samePath(R"(\\Srv\Sh\k.wav)", R"(\\srv\sh\k.wav)"));
    CHECK_FALSE(Res::samePath(R"(C:\Music\a.wav)", R"(C:\Music\b.wav)"));
    CHECK_FALSE(Res::samePath(R"(C:\Music\a.wav)", R"(D:\Music\a.wav)"));
}

TEST_CASE("sample_path: samePath is case-sensitive on POSIX paths", "[sample_path]")
{
    CHECK(Res::samePath("/home/a/k.wav", "/home/a/k.wav"));
    CHECK(Res::samePath("/home//a/k.wav", "/home/a/k.wav"));
    CHECK_FALSE(Res::samePath("/home/a/K.wav", "/home/a/k.wav"));
    CHECK_FALSE(Res::samePath("/home/a/k.wav", R"(C:\home\a\k.wav)"));
}

TEST_CASE("sample_path: samePath drops one trailing separator but keeps the root", "[sample_path]")
{
    CHECK(Res::samePath(R"(E:\a\)", R"(E:\a)"));
    CHECK(Res::samePath("/a/b/", "/a/b"));
    CHECK_FALSE(Res::samePath(R"(C:\)", "C:"));
    CHECK(Res::samePath(R"(C:\)", "C:/"));
    CHECK(Res::samePath("/", "//"));
    CHECK_FALSE(Res::samePath("/", ""));
}

// 2.3 - 2.5 makeRef -----------------------------------------------------------------------------

namespace {
const std::vector<PathAnchor> kWinAnchors{{"music", R"(C:\Users\a\Music)"}, {"home", R"(C:\Users\a)"}};
} // namespace

TEST_CASE("sample_path: makeRef finds the anchor and stores the relative path with slashes", "[sample_path]")
{
    const SampleRef r = Res::makeRef(R"(C:\Users\a\Music\loops\kick.wav)", kWinAnchors);
    CHECK(r.absolute == R"(C:\Users\a\Music\loops\kick.wav)");
    CHECK(r.name == "kick.wav");
    CHECK(r.anchorKey == "music");
    CHECK(r.relative == "loops/kick.wav");

    const SampleRef u = Res::makeRef(R"(\\srv\sh\music\loops\kick.wav)", {{"music", R"(\\SRV\Sh\Music)"}});
    CHECK(u.anchorKey == "music");
    CHECK(u.relative == "loops/kick.wav");
}

TEST_CASE("sample_path: makeRef picks the longest anchor", "[sample_path]")
{
    const SampleRef r = Res::makeRef(R"(C:\Users\a\Music\kick.wav)", kWinAnchors);
    CHECK(r.anchorKey == "music");
    CHECK(r.relative == "kick.wav");

    const SampleRef h = Res::makeRef(R"(C:\Users\a\Other\kick.wav)", kWinAnchors);
    CHECK(h.anchorKey == "home");
    CHECK(h.relative == "Other/kick.wav");

    // Reversed list order does not change the winner.
    const SampleRef rev = Res::makeRef(R"(C:\Users\a\Music\kick.wav)", {kWinAnchors[1], kWinAnchors[0]});
    CHECK(rev.anchorKey == "music");

    // On equal length the first anchor wins.
    const SampleRef tie = Res::makeRef(R"(C:\M\k.wav)", {{"first", R"(C:\M)"}, {"second", R"(c:\m)"}});
    CHECK(tie.anchorKey == "first");
}

TEST_CASE("sample_path: makeRef matches whole components only", "[sample_path]")
{
    const SampleRef r = Res::makeRef(R"(C:\Users\ab\kick.wav)", {{"home", R"(C:\Users\a)"}});
    CHECK(r.anchorKey.empty());
    CHECK(r.relative.empty());
    CHECK(r.name == "kick.wav");
    CHECK(Res::makeRef(R"(C:\Music2\k.wav)", {{"music", R"(C:\Music)"}}).anchorKey.empty());
    CHECK(Res::makeRef(R"(C:\Music\k.wav)", {{"music", R"(C:\Music)"}}).anchorKey == "music");
}

TEST_CASE("sample_path: makeRef without a containing anchor gives name only", "[sample_path]")
{
    const SampleRef r = Res::makeRef(R"(D:\samples\kick.wav)", kWinAnchors);
    CHECK(r.absolute == R"(D:\samples\kick.wav)");
    CHECK(r.name == "kick.wav");
    CHECK(r.anchorKey.empty());
    CHECK(r.relative.empty());
    CHECK(Res::makeRef(R"(D:\samples\kick.wav)", {}).anchorKey.empty());
}

TEST_CASE("sample_path: makeRef ignores case on Windows paths but keeps the original case", "[sample_path]")
{
    const SampleRef r = Res::makeRef(R"(c:\users\A\MUSIC\Kick.wav)", {{"music", R"(C:\Users\a\Music)"}});
    CHECK(r.anchorKey == "music");
    CHECK(r.relative == "Kick.wav");
}

TEST_CASE("sample_path: makeRef accepts forward slashes on Windows paths", "[sample_path]")
{
    const SampleRef r = Res::makeRef("C:/Users/a/Music/loops/kick.wav", {{"music", R"(C:\Users\a\Music)"}});
    CHECK(r.anchorKey == "music");
    CHECK(r.relative == "loops/kick.wav");
    CHECK(r.name == "kick.wav");
}

TEST_CASE("sample_path: makeRef is case-sensitive on POSIX paths", "[sample_path]")
{
    const std::vector<PathAnchor> anchors{{"music", "/home/a/Music"}};
    const SampleRef hit = Res::makeRef("/home/a/Music/kick.wav", anchors);
    CHECK(hit.anchorKey == "music");
    CHECK(hit.relative == "kick.wav");
    CHECK(Res::makeRef("/home/a/music/kick.wav", anchors).anchorKey.empty());
}

TEST_CASE("sample_path: a backslash is a filename character on POSIX", "[sample_path]")
{
    const SampleRef r = Res::makeRef(R"(/home/a/Music/we\ird.wav)", {{"music", "/home/a/Music"}});
    CHECK(r.name == R"(we\ird.wav)");
    CHECK(r.anchorKey == "music");
    CHECK(r.relative == R"(we\ird.wav)");
}

TEST_CASE("sample_path: an anchor equal to the whole path does not match", "[sample_path]")
{
    CHECK(Res::makeRef(R"(C:\Users\a\Music)", kWinAnchors).anchorKey == "home");
    CHECK(Res::makeRef(R"(C:\Users\a)", kWinAnchors).anchorKey.empty());
    CHECK(Res::makeRef(R"(C:\Users\a\)", kWinAnchors).anchorKey.empty());
}

TEST_CASE("sample_path: a POSIX anchor never matches a Windows path", "[sample_path]")
{
    CHECK(Res::makeRef(R"(C:\home\a\k.wav)", {{"home", "/home/a"}}).anchorKey.empty());
    CHECK(Res::makeRef("/home/a/k.wav", {{"home", R"(C:\home\a)"}}).anchorKey.empty());
}

TEST_CASE("sample_path: name extraction across path shapes", "[sample_path]")
{
    CHECK(Res::makeRef(R"(C:\x\kick.wav)", {}).name == "kick.wav");
    CHECK(Res::makeRef(R"(\\srv\sh\kick.wav)", {}).name == "kick.wav");
    CHECK(Res::makeRef("/x/kick.wav", {}).name == "kick.wav");
    CHECK(Res::makeRef("kick.wav", {}).name == "kick.wav");
}

TEST_CASE("sample_path: a dot-dot path gets no anchor", "[sample_path]")
{
    const SampleRef r = Res::makeRef(R"(C:\Users\a\Music\..\Other\kick.wav)", kWinAnchors);
    CHECK(r.anchorKey.empty());
    CHECK(r.relative.empty());
    CHECK(r.name == "kick.wav");
    CHECK(Res::makeRef("/home/a/Music/../k.wav", {{"music", "/home/a/Music"}}).anchorKey.empty());
    // A dotted name that is not a ".." component still matches.
    CHECK(Res::makeRef(R"(C:\Users\a\Music\..k\x.wav)", kWinAnchors).anchorKey == "music");
}

// 2.6 - 2.7 restoreRef --------------------------------------------------------------------------

TEST_CASE("sample_path: restoreRef derives the name for legacy state", "[sample_path]")
{
    const SampleRef r = Res::restoreRef(R"(C:\old\kick.wav)", "", "", "");
    CHECK(r.absolute == R"(C:\old\kick.wav)");
    CHECK(r.name == "kick.wav");
    CHECK(r.anchorKey.empty());
    CHECK(r.relative.empty());

    const SampleRef kept = Res::restoreRef(R"(C:\old\kick.wav)", "Stored.wav", "", "");
    CHECK(kept.name == "Stored.wav");
}

TEST_CASE("sample_path: restoreRef keeps a valid anchor pair", "[sample_path]")
{
    const SampleRef r = Res::restoreRef(R"(C:\Users\a\Music\loops\kick.wav)", "kick.wav", "music", "loops/kick.wav");
    CHECK(r.anchorKey == "music");
    CHECK(r.relative == "loops/kick.wav");
}

TEST_CASE("sample_path: restoreRef clears an invalid anchor pair", "[sample_path]")
{
    const auto cleared = [](const std::string& key, const std::string& rel) {
        const SampleRef r = Res::restoreRef(R"(C:\x\kick.wav)", "kick.wav", key, rel);
        return r.anchorKey.empty() && r.relative.empty();
    };
    CHECK(cleared("music", "../kick.wav"));
    CHECK(cleared("music", "loops/../../kick.wav"));
    CHECK(cleared("music", "/kick.wav"));
    CHECK(cleared("music", R"(C:\kick.wav)"));
    CHECK(cleared("music", ""));
    CHECK(cleared("", "loops/kick.wav"));
    CHECK(cleared("music", "loops//kick.wav"));
    CHECK_FALSE(cleared("music", "loops/kick.wav"));
}

TEST_CASE("sample_path: restoreRef turns backslashes in relative into slashes unless POSIX", "[sample_path]")
{
    const SampleRef win = Res::restoreRef(R"(C:\Users\a\Music\loops\kick.wav)", "kick.wav", "music", R"(loops\kick.wav)");
    CHECK(win.relative == "loops/kick.wav");

    const SampleRef posix = Res::restoreRef(R"(/home/a/Music/we\ird.wav)", R"(we\ird.wav)", "music", R"(we\ird.wav)");
    CHECK(posix.relative == R"(we\ird.wav)");
}

TEST_CASE("sample_path: an empty samplePath clears every other field", "[sample_path]")
{
    const SampleRef r = Res::restoreRef("", "kick.wav", "music", "kick.wav");
    CHECK(r.absolute.empty());
    CHECK(r.name.empty());
    CHECK(r.anchorKey.empty());
    CHECK(r.relative.empty());

    SampleRef stray;
    stray.name = "kick.wav";
    stray.anchorKey = "music";
    stray.relative = "kick.wav";
    CHECK(Res::candidates(stray, {{"music", R"(D:\Music)"}}, {R"(E:\a)"}).empty());
}

// 2.8 candidates --------------------------------------------------------------------------------

namespace {
SampleRef anchoredRef()
{
    SampleRef r;
    r.absolute = R"(C:\old\kick.wav)";
    r.name = "kick.wav";
    r.anchorKey = "music";
    r.relative = "loops/kick.wav";
    return r;
}
using Paths = std::vector<std::string>;
} // namespace

TEST_CASE("sample_path: candidates are ordered anchor, absolute, search directories", "[sample_path]")
{
    const Paths got = Res::candidates(anchoredRef(), {{"music", R"(D:\Music)"}}, {R"(E:\a)", R"(E:\b)"});
    const Paths want{R"(D:\Music\loops\kick.wav)", R"(C:\old\kick.wav)", R"(E:\a\kick.wav)", R"(E:\b\kick.wav)"};
    CHECK(got == want);
}

TEST_CASE("sample_path: candidates join the anchor in each path style", "[sample_path]")
{
    SampleRef posixRef = anchoredRef();
    posixRef.absolute = "/old/kick.wav";
    CHECK(Res::candidates(posixRef, {{"music", "/home/b/Music/"}}, {}) ==
          Paths{"/home/b/Music/loops/kick.wav", "/old/kick.wav"});
    // The stored relative path is portable: a Windows anchor gets backslashes.
    CHECK(Res::candidates(posixRef, {{"music", R"(D:\Music\)"}}, {}).front() == R"(D:\Music\loops\kick.wav)");
}

TEST_CASE("sample_path: candidates skip an anchor this machine lacks", "[sample_path]")
{
    CHECK(Res::candidates(anchoredRef(), {{"documents", R"(D:\Docs)"}}, {}) == Paths{R"(C:\old\kick.wav)"});
    CHECK(Res::candidates(anchoredRef(), {}, {R"(E:\a)"}) == Paths{R"(C:\old\kick.wav)", R"(E:\a\kick.wav)"});
}

TEST_CASE("sample_path: candidates skip a non-absolute stored path but keep the name search", "[sample_path]")
{
    SampleRef r;
    r.absolute = "kick.wav";
    r.name = "kick.wav";
    CHECK(Res::candidates(r, {}, {R"(E:\a)"}) == Paths{R"(E:\a\kick.wav)"});
}

TEST_CASE("sample_path: candidates drop duplicates keeping the first position", "[sample_path]")
{
    const SampleRef r = Res::makeRef(R"(D:\Music\loops\kick.wav)", {{"music", R"(D:\Music)"}});
    const Paths got = Res::candidates(r, {{"music", R"(d:/music)"}}, {R"(D:\Music\loops)", R"(E:\a)"});
    CHECK(got == Paths{R"(d:\music\loops\kick.wav)", R"(E:\a\kick.wav)"});
}

TEST_CASE("sample_path: candidates dedup is case-insensitive only for Windows paths", "[sample_path]")
{
    SampleRef win;
    win.absolute = R"(C:\Music\Kick.wav)";
    win.name = "Kick.wav";
    CHECK(Res::candidates(win, {}, {R"(c:\music)"}) == Paths{R"(C:\Music\Kick.wav)"});

    SampleRef posix;
    posix.absolute = "/m/Kick.wav";
    posix.name = "Kick.wav";
    CHECK(Res::candidates(posix, {}, {"/M"}) == Paths{"/m/Kick.wav", "/M/Kick.wav"});
}

TEST_CASE("sample_path: candidates treat search directories with and without trailing separator alike", "[sample_path]")
{
    SampleRef r;
    r.absolute = R"(C:\old\kick.wav)";
    r.name = "kick.wav";
    CHECK(Res::candidates(r, {}, {R"(E:\a\)", R"(E:\a)"}) == Paths{R"(C:\old\kick.wav)", R"(E:\a\kick.wav)"});
}

TEST_CASE("sample_path: candidates are capped at ten entries keeping the first ten", "[sample_path]")
{
    Paths dirs;
    for (int i = 0; i < 20; ++i) dirs.push_back(R"(E:\d)" + std::to_string(i));
    const Paths got = Res::candidates(anchoredRef(), {{"music", R"(D:\Music)"}}, dirs);
    REQUIRE(got.size() == Res::kMaxCandidates);
    CHECK(got[0] == R"(D:\Music\loops\kick.wav)");
    CHECK(got[1] == R"(C:\old\kick.wav)");
    CHECK(got[2] == R"(E:\d0\kick.wav)");
    CHECK(got[9] == R"(E:\d7\kick.wav)");
}

TEST_CASE("sample_path: a legacy reference lists the absolute path then the name in search directories", "[sample_path]")
{
    const SampleRef r = Res::restoreRef(R"(C:\old\kick.wav)", "", "", "");
    CHECK(Res::candidates(r, {}, {R"(E:\a)"}) == Paths{R"(C:\old\kick.wav)", R"(E:\a\kick.wav)"});
}

TEST_CASE("sample_path: candidates ignore an unsafe name or relative path", "[sample_path]")
{
    SampleRef r = anchoredRef();
    r.relative = "../../kick.wav";
    r.name = "..";
    CHECK(Res::candidates(r, {{"music", R"(D:\Music)"}}, {R"(E:\a)"}) == Paths{R"(C:\old\kick.wav)", R"(E:\a\kick.wav)"});
    r.name = R"(x\kick.wav)";
    CHECK(Res::candidates(r, {}, {R"(E:\a)"}) == Paths{R"(C:\old\kick.wav)", R"(E:\a\kick.wav)"});
}

// 3.0 cross-shape separators --------------------------------------------------------------------

TEST_CASE("sample_path: a POSIX name with backslashes never escapes a Windows search directory", "[sample_path]")
{
    SampleRef r;
    r.absolute = "/old/x.wav";
    r.name = R"(..\..\x.wav)"; // one legal POSIX file name
    const Paths got = Res::candidates(r, {}, {R"(E:\a)", "/m"});
    CHECK(got == Paths{"/old/x.wav", R"(/m/..\..\x.wav)"});
}

TEST_CASE("sample_path: a POSIX relative path with backslashes never escapes a Windows anchor", "[sample_path]")
{
    SampleRef r;
    r.absolute = "/old/x.wav";
    r.name = "x.wav";
    r.anchorKey = "music";
    r.relative = R"(a\..\..\x)";
    CHECK(Res::candidates(r, {{"music", R"(D:\Music)"}}, {}) == Paths{"/old/x.wav"});
    CHECK(Res::candidates(r, {{"music", R"(\\srv\share)"}}, {}) == Paths{"/old/x.wav"});
    CHECK(Res::candidates(r, {{"music", "/home/b"}}, {}) == Paths{R"(/home/b/a\..\..\x)", "/old/x.wav"});
}

TEST_CASE("sample_path: a colon in a name or relative part is skipped on Windows targets only", "[sample_path]")
{
    SampleRef posix;
    posix.absolute = "/old/a:b.wav";
    posix.name = "a:b.wav";
    CHECK(Res::candidates(posix, {}, {R"(E:\a)", "/m"}) == Paths{"/old/a:b.wav", "/m/a:b.wav"});

    SampleRef win;
    win.absolute = R"(C:\old\kick:stream.wav)";
    win.name = "kick:stream.wav";
    CHECK(Res::candidates(win, {}, {R"(E:\a)"}) == Paths{R"(C:\old\kick:stream.wav)"});

    SampleRef rel = anchoredRef();
    rel.relative = "a:b/kick.wav";
    CHECK(Res::candidates(rel, {{"music", R"(D:\Music)"}}, {}) == Paths{R"(C:\old\kick.wav)"});
}

// 2.9 resolve -----------------------------------------------------------------------------------

TEST_CASE("sample_path: resolve returns the first existing candidate and stops there", "[sample_path]")
{
    const Paths list{"A", "B", "C"};
    Paths called;
    const auto hit = Res::resolve(list, [&called](const std::string& p) {
        called.push_back(p);
        return p == "B" || p == "C";
    });
    REQUIRE(hit.has_value());
    CHECK(*hit == 1);
    CHECK(called == Paths{"A", "B"});

    const auto first = Res::resolve(list, [](const std::string& p) { return p == "A"; });
    REQUIRE(first.has_value());
    CHECK(*first == 0);
}

TEST_CASE("sample_path: resolve reports not found after trying every candidate in order", "[sample_path]")
{
    Paths called;
    const auto miss = Res::resolve({"A", "B"}, [&called](const std::string& p) {
        called.push_back(p);
        return false;
    });
    CHECK_FALSE(miss.has_value());
    CHECK(called == Paths{"A", "B"});
}

TEST_CASE("sample_path: resolve on an empty list never calls the predicate", "[sample_path]")
{
    int calls = 0;
    const auto miss = Res::resolve({}, [&calls](const std::string&) {
        ++calls;
        return true;
    });
    CHECK_FALSE(miss.has_value());
    CHECK(calls == 0);
}

// 2.10 garbage input ----------------------------------------------------------------------------

TEST_CASE("sample_path: empty and separator-only input is harmless", "[sample_path]")
{
    const std::vector<PathAnchor> anchors{{"music", R"(C:\Music)"}, {"posix", "/m"}};
    for (const std::string& s : {std::string(), std::string("/"), std::string("////"), std::string(R"(\\)"),
                                 std::string(R"(\\\\\)"), std::string(R"(C:\)")}) {
        const SampleRef r = Res::makeRef(s, anchors);
        CHECK(r.absolute == s);
        CHECK(r.name.empty());
        CHECK(r.anchorKey.empty());
        CHECK(r.relative.empty());
    }
    const SampleRef empty = Res::makeRef("", anchors);
    CHECK(Res::candidates(empty, anchors, {R"(E:\a)"}).empty());
    // A separator-only stored path has no name, so there is no name search (no E:\a entry).
    const SampleRef slashes = Res::restoreRef("////", "", "music", "x");
    CHECK(Res::candidates(slashes, anchors, {R"(E:\a)"}) == std::vector<std::string>{R"(C:\Music\x)", "////"});
}

TEST_CASE("sample_path: embedded NUL and control characters do not throw or lose bytes", "[sample_path]")
{
    const std::string nul = std::string("C:\\Music\\a") + '\0' + "b.wav";
    const SampleRef r = Res::makeRef(nul, {{"music", R"(C:\Music)"}});
    CHECK(r.name == std::string("a") + '\0' + "b.wav");
    CHECK(r.anchorKey == "music");
    CHECK(r.relative == r.name);

    const std::string ctl = std::string("/x/") + '\x01' + '\n' + "k.wav";
    CHECK(Res::makeRef(ctl, {}).name == std::string("\x01\nk.wav"));

    const auto list = Res::candidates(r, {{"music", R"(D:\M)"}}, {R"(E:\a)"});
    REQUIRE(list.size() == 3);
    CHECK(list[2] == R"(E:\a\)" + r.name);
    const auto hit = Res::resolve(list, [&nul](const std::string& p) { return p == nul; });
    REQUIRE(hit.has_value());
    CHECK(*hit == 1);
}

TEST_CASE("sample_path: a very long path goes through every operation", "[sample_path]")
{
    const std::string longName(100000, 'k');
    const std::string path = R"(C:\Music\)" + longName;
    const SampleRef r = Res::makeRef(path, {{"music", R"(C:\Music)"}});
    CHECK(r.name == longName);
    CHECK(r.relative == longName);
    const auto list = Res::candidates(r, {{"music", R"(D:\M)"}}, {R"(E:\a)"});
    REQUIRE(list.size() == 3);
    CHECK(list[0].size() == std::string(R"(D:\M\)").size() + longName.size());
    CHECK(Res::samePath(path, list[1]));
    const auto hit = Res::resolve(list, [](const std::string& p) { return p.front() == 'E'; });
    REQUIRE(hit.has_value());
    CHECK(*hit == 2);
    const std::string deep(100000, '/');
    CHECK(Res::makeRef(deep, {{"home", "/"}}).name.empty());
}
