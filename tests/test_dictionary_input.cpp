#include "entry_editor.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>
using writer::Button;
static int width(const char*){return 8;}
constexpr unsigned bit(Button b){return 1u<<unsigned(b);}
static void tap(EntryEditor& e,unsigned b){e.frame(b);e.frame(0);}
int main(){
 EntryEditor e(width);e.open(7,"old\talt");e.frame(0);
 tap(e,bit(Button::UP));tap(e,bit(Button::A));assert(e.take_dictionary_request());assert(!e.autosave());
 assert(e.prefill_add("café au lait","Milchkaffee"));e.frame(0);
 assert(e.operation()==EntryMutation::add);assert(!std::strcmp(e.text().data(),"café au lait"));
 tap(e,bit(Button::START)|bit(Button::A));assert(!std::strcmp(e.text().data(),"Milchkaffee"));
 tap(e,bit(Button::START)|bit(Button::B));assert(!std::strcmp(e.text().data(),"café au lait"));
 tap(e,bit(Button::START)|bit(Button::B));assert(!e.commit_requested());
 assert(!std::strcmp(e.captured(),"old\talt"));
 e.open_lookup();e.frame(0);tap(e,bit(Button::UP)|bit(Button::B));assert(!std::strcmp(e.text().data(),"a"));
 const Button buttons[]={Button::UP,Button::DOWN,Button::L,Button::R,Button::A,Button::B};
 const EntryEditor::LookupAction expected[]={EntryEditor::LookupAction::up,EntryEditor::LookupAction::down,EntryEditor::LookupAction::direction,EntryEditor::LookupAction::chooser,EntryEditor::LookupAction::select,EntryEditor::LookupAction::cancel};
 for(unsigned i=0;i<6;++i)for(int tail=0;tail<2;++tail){
   e.frame(bit(Button::START));e.frame(bit(Button::START)|bit(buttons[i]));
   assert(e.take_lookup_action()==expected[i]);
   e.frame(tail?bit(Button::START):bit(buttons[i]));e.frame(0);
   assert(e.take_lookup_action()==EntryEditor::LookupAction::none);
   assert(!std::strcmp(e.text().data(),"a"));assert(!e.commit_requested());
 }
 for(auto b:{Button::UP,Button::DOWN}) {
   e.frame(bit(Button::START));int events=0;
   for(int frame=0;frame<120;++frame){e.frame(bit(Button::START)|bit(b));if(e.take_lookup_action()!=EntryEditor::LookupAction::none)++events;}
   assert(events>10);e.frame(0);assert(!std::strcmp(e.text().data(),"a"));
 }
 for(auto b:{Button::L,Button::R}) {
   e.frame(bit(Button::START));e.frame(bit(Button::START)|bit(b));assert(e.take_lookup_action()!=EntryEditor::LookupAction::none);
   for(int frame=0;frame<120;++frame){e.frame(bit(Button::START)|bit(b));assert(e.take_lookup_action()==EntryEditor::LookupAction::none);}
   e.frame(0);
 }
 for(auto b:{Button::LEFT,Button::RIGHT}) {
   e.frame(bit(Button::START));e.frame(bit(Button::START)|bit(b));
   assert(e.take_lookup_action()==(b==Button::LEFT?EntryEditor::LookupAction::edit:EntryEditor::LookupAction::remove));
   for(int f=0;f<120;++f){e.frame(bit(Button::START)|bit(b));assert(e.take_lookup_action()==EntryEditor::LookupAction::none);}
   e.frame(bit(b));e.frame(0);assert(e.take_lookup_action()==EntryEditor::LookupAction::none);
   assert(e.text().caret_byte()==1);assert(!std::strcmp(e.text().data(),"a"));
 }
 tap(e,bit(Button::UP)|bit(Button::A));assert(!std::strcmp(e.text().data(),"ab"));
 for(bool editable:{false,true})for(int order=0;order<3;++order)for(int tail=0;tail<2;++tail){
   e.open_lookup(editable);e.frame(0);tap(e,bit(Button::UP)|bit(Button::B));
   if(order<2)e.frame(bit(order?Button::SELECT:Button::START));
   e.frame(bit(Button::START)|bit(Button::SELECT));
   assert(e.take_lookup_action()==EntryEditor::LookupAction::add);
   for(int frame=0;frame<120;++frame){e.frame(bit(Button::START)|bit(Button::SELECT));assert(e.take_lookup_action()==EntryEditor::LookupAction::none);}
   e.frame(bit(tail?Button::START:Button::SELECT));e.frame(0);
   assert(e.take_lookup_action()==EntryEditor::LookupAction::none);assert(!std::strcmp(e.text().data(),"a"));
 }
 // The production scoped loan restores every byte, including inactive drafts,
 // suffix, input session, layout, clock, captured identity and pending state.
 e.open(7,"old\talt\textra");e.frame(0);tap(e,bit(Button::DOWN));tap(e,bit(Button::A));
 e.frame(0);tap(e,bit(Button::START)|bit(Button::A));e.frame(0);
 unsigned char snapshot[sizeof e];std::memcpy(snapshot,&e,sizeof e);
 for(auto op:{EntryMutation::add,EntryMutation::edit,EntryMutation::remove})for(int outcome=0;outcome<3;++outcome){
   { EntryEditorLoan loan(e);
     assert(e.open_dictionary_mutation(op,"new","neu"));e.frame(0);
     if(op==EntryMutation::remove){
       assert(e.screen()==EntryEditor::Screen::confirm_delete);assert(!e.selection());
       if(outcome){tap(e,bit(Button::RIGHT));tap(e,bit(Button::A));assert(e.commit_requested());}
       else tap(e,bit(Button::A));
     }else{
       assert(!std::strcmp(e.text().data(),"new"));
       if(outcome){tap(e,bit(Button::START)|bit(Button::A));tap(e,bit(Button::START)|bit(Button::A));assert(e.commit_requested());assert(!std::strcmp(e.row(),"new\tneu"));}
       else tap(e,bit(Button::START)|bit(Button::B));
     }
     if(outcome)e.finish(outcome==1,"injected failure");
   }
   assert(!std::memcmp(snapshot,&e,sizeof e));
 }
 puts("PASS production lookup mutation actions, release tails, nested mutation drafts and exact restoration");
}
