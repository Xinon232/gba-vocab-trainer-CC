#include "entry_shortcuts.h"
#include <cassert>
#include <cstdio>
int main() {
    using A = EntryShortcuts::Action;
    for (int first=1; first<=3; ++first) for(int tail=1;tail<=2;++tail) {
        EntryShortcuts g;
        assert(g.update(first&1, first&2)==(first==3?A::editor:A::none));
        for(int i=0;i<90;++i) assert(g.update(first&1, first&2)==A::none);
        assert(g.update(true,true)==(first==3?A::none:A::editor));
        for(int i=0;i<90;++i) assert(g.update(tail&1,tail&2)==A::none);
        assert(g.update(false,false)==A::none);
        assert(g.update(true,false)==A::none);
        assert(g.update(false,false)==A::save);
        assert(g.update(false,true)==A::none);
        assert(g.update(false,false)==A::menu);
    }
    EntryShortcuts g;
    g.suppress_until_release();
    assert(g.update(true,false)==A::none);
    assert(g.update(false,false)==A::none);
    assert(g.update(false,true)==A::none);
    assert(g.update(false,false)==A::menu);
    puts("PASS learning shortcuts: isolated release, both chord orders, holds and tails");
}
