#include "state.h"
#include <cassert>
#include <cstdio>
int main(){
 VocabFile v;vocab_open(v,"a\tb\n\nc\td\n",9);State s;
 s.debug_set_field(1);s.debug_set_line(0);s.debug_set_direction(2);s.debug_set_undo(true);
 s.entry_committed(v,1);assert(s.current_field()==2&&s.current_line_idx()==1&&s.direction_mode()==2&&!s.undo_pending()&&s.scene()==0);
 v.reset();v.loaded=true;s.entry_committed(v,-1);
 assert(s.current_line_idx()==-1&&s.current_field_is_empty(v)&&s.scene()==0);
 State::InputState in;in.select_pressed=true;s.update(v,in);assert(s.scene()==1);
 State feedback;vocab_open(v,"a\tb\n",4);
 State::InputState correct;correct.a_pressed=true;correct.a_held=true;
 feedback.update(v,correct);assert(feedback.feedback_active()&&feedback.current_field_is_empty(v));
 assert(feedback.entry_target(v)==0); // card remains visibly displayed during feedback
 puts("PASS committed entry navigation: box, direction, undo invalidation, usable empty list");
}
