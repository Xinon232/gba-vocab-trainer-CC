#include "home_screen.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>
namespace {
struct FakeStorage {
    bool save_ok=false, open_ok=false;
    int saves=0, loads=0, creates=0;
    int active=42, navigation=17;
};
HomeActions actions(FakeStorage& io) {
    return {&io,
        [](void* p) { auto& f=*static_cast<FakeStorage*>(p); ++f.saves; return f.save_ok; },
        [](void* p,const char* name) { auto& f=*static_cast<FakeStorage*>(p); assert(!std::strcmp(name,"LIST001.TXT")); ++f.loads; if(f.open_ok)f.active=99;return f.open_ok; },
        [](void* p,const char* name) { auto& f=*static_cast<FakeStorage*>(p); assert(!std::strcmp(name,"LIST001.TXT")); ++f.creates; if(f.open_ok)f.active=100;return f.open_ok; }};
}
}
int main() {
    HomeScreen h;
    assert(h.page()==HomeScreen::Page::home && h.selection()==0);
    h.press(HomeScreen::Key::a,false,false,0);
    assert(h.page()==HomeScreen::Page::files);
    assert(h.request()==HomeScreen::Request::none);
    h.press(HomeScreen::Key::a,false,false,0);
    assert(h.request()==HomeScreen::Request::none);
    h.press(HomeScreen::Key::b,false,false,0);
    h.press(HomeScreen::Key::b,false,false,0);
    assert(h.request()==HomeScreen::Request::none);
    h.press(HomeScreen::Key::b,true,false,0);
    assert(h.request()==HomeScreen::Request::resume);
    for(bool create : {false,true}) {
        HomeScreen s;
        if(create) s.press(HomeScreen::Key::down,true,true,7);
        s.press(HomeScreen::Key::a,true,true,7);
        s.press(HomeScreen::Key::a,true,true,7);
        assert(s.page()==HomeScreen::Page::confirm);
        s.press(HomeScreen::Key::select,true,true,7);
        assert(s.request()==HomeScreen::Request::none);
        assert(s.page()==(create?HomeScreen::Page::new_list:HomeScreen::Page::files));
        s.press(HomeScreen::Key::a,true,true,7);
        s.press(HomeScreen::Key::a,true,true,7);
        assert(s.save_first());
        assert(s.request()==(create?HomeScreen::Request::create:HomeScreen::Request::load));
        FakeStorage io;
        assert(home_apply_request(s,actions(io),"LIST001.TXT")==HomeResult::save_failed);
        assert(io.saves==1 && io.loads==0 && io.creates==0);
        assert(io.active==42 && io.navigation==17);
        io.save_ok=true;
        assert(home_apply_request(s,actions(io),"LIST001.TXT")==HomeResult::open_failed);
        assert(io.active==42 && io.navigation==17);
        s.finish(false);
        assert(s.request()==HomeScreen::Request::none);
        assert(s.page()==(create?HomeScreen::Page::new_list:HomeScreen::Page::files));
        s.press(HomeScreen::Key::a,true,true,7);
        s.press(HomeScreen::Key::b,true,true,7);
        assert(!s.save_first());
        int saves=io.saves;
        assert(home_apply_request(s,actions(io),"LIST001.TXT")==HomeResult::open_failed);
        assert(io.saves==saves && io.active==42 && io.navigation==17);
        io.open_ok=true;
        assert(home_apply_request(s,actions(io),"LIST001.TXT")==HomeResult::opened);
        assert(io.saves==saves && io.active==(create?100:99));
        s.finish(false);
        s.press(HomeScreen::Key::a,true,false,7);
        assert(s.request()==(create?HomeScreen::Request::create:HomeScreen::Request::load));
    }
    HomeScreen pages;
    pages.press(HomeScreen::Key::select,false,false,0);
    assert(pages.page()==HomeScreen::Page::controls);
    for(int i=0;i<100;++i) pages.press(HomeScreen::Key::right,false,false,0);
    assert(pages.help_page()==HOME_HELP_PAGES-1);
    pages.press(HomeScreen::Key::b,false,false,0);
    pages.press(HomeScreen::Key::start,false,false,0);
    assert(pages.page()==HomeScreen::Page::credits);
    std::puts("PASS home default, empty, resume, dirty save/discard/cancel, failed operations, help");
}
