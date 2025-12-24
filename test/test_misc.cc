#include "misc.h"

#include "test/test.h"

namespace ut {
suite<"MiscUtils"> misc = [] {
    using namespace utils;
    using Level = Diagnostic::Level;
    test("Diagnostics") = [] {
        {
            std::stringstream ss;
            auto foo = Diagnostic("aaa bbb ccc ddd eee", "foo bar baz", 0, 7);
            foo.display();
            foo.display(ss);
            std::cout << ss.str() << '\n';
        }
        {
            std::stringstream ss;
            auto foo = Diagnostic("aaa bbb ccc ddd eee", "foo bar baz", 7, 7, Level::Warning);
            foo.display();
            foo.display(ss);
            std::cout << ss.str() << '\n';
        }
        {
            std::stringstream ss;
            auto foo = Diagnostic("aaa bbb ccc ddd eee", "foo bar baz", 7, 10, Level::Error);
            foo.display();
            foo.display(ss);
            std::cout << ss.str() << '\n';
        }
        {
            std::stringstream ss;
            auto foo = Diagnostic("aaa bbb ccc ddd eee", "foo bar baz", 10, 14, Level::Fatal);
            foo.display();
            foo.display(ss);
            std::cout << ss.str() << '\n';
        }
    };

    test("StringLike") = [] {
        {
            expect(strlike("hello", "hello"));
            expect(strlike("hello", "_ello"));
            expect(strlike("hello", "h_llo"));
            expect(strlike("hello", "he_lo"));
            expect(strlike("hello", "hel_o"));
            expect(strlike("hello", "hell_"));
            expect(strlike("hello", "_ello"));
            expect(strlike("hello", "__llo"));
            expect(strlike("hello", "___lo"));
            expect(strlike("hello", "____o"));
            expect(strlike("hello", "_____"));
            expect(!strlike("hello", "world"));
            expect(!strlike("hello", "hell"));
            expect(!strlike("hello", "hell__"));
            expect(strlike("_iallo", "\\_iallo"));
            expect(strlike("%iall%", "\\%ial_\\%"));
            expect(strlike("%iall%", "\\%ia%\\%"));
        }

        {
            expect(strlike("hello", "_ello"));
            expect(strlike("hello", "h_llo"));
            expect(strlike("hello", "hell_"));
            expect(!strlike("hello", "h_o"));
            expect(!strlike("hi", "h__"));
        }

        {
            expect(strlike("hello", "%hello"));
            expect(strlike("hello", "hello%"));
            expect(strlike("hello", "h%o"));
            expect(strlike("hello", "%o"));
            expect(strlike("hello", "h%"));
            expect(strlike("database", "%data%"));
        }

        {
            expect(strlike("gpamgr", "g%m_r"));
            expect(strlike("gpamgr", "%_____%"));
            expect(!strlike("gpa", "%_____%"));
        }

        {
            expect(strlike("abc", "%%%%%"));
            expect(strlike("", "%"));
            expect(!strlike("", "_"));
            expect(strlike("a", "%_"));
            expect(strlike("a", "_%"));
        }

        {

            expect(strlike("ababac", "ab%ac"));
            expect(strlike("ababac", "%ab%c"));
            expect(!strlike("ababac", "ab%d"));
        }
    };
};
}  // namespace ut
