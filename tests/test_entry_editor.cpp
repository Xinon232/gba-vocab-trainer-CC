#include "entry_editor.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
using writer::Button;
constexpr unsigned bit(Button b){return 1u<<unsigned(b);}
constexpr unsigned START=bit(Button::START), A=bit(Button::A), B=bit(Button::B), UP=bit(Button::UP);
static int width(const char*){return 8;}
static void tap(EntryEditor& e,unsigned b){e.frame(b);e.frame(0);}
int main(){
 EntryEditor e(width);e.open(-1,nullptr);e.frame(0);
 assert(e.screen()==EntryEditor::Screen::menu);
 tap(e,A);assert(e.screen()==EntryEditor::Screen::front);
 tap(e,UP|B);assert(!std::strcmp(e.text().data(),"a"));
 tap(e,START|A);assert(e.screen()==EntryEditor::Screen::back);
 tap(e,UP|A);assert(!std::strcmp(e.text().data(),"b"));
 tap(e,START|B);assert(e.screen()==EntryEditor::Screen::front);
 assert(!std::strcmp(e.text().data(),"a"));
 tap(e,START|A);assert(!std::strcmp(e.text().data(),"b"));
 tap(e,START|A);assert(e.commit_requested());
 assert(e.operation()==EntryMutation::add && !std::strcmp(e.row(),"a\tb"));
 e.finish(false,"WRITE FAILED");e.frame(0);assert(e.screen()==EntryEditor::Screen::back);
 assert(!std::strcmp(e.text().data(),"b") && !e.commit_requested());
 tap(e,START|B);tap(e,START|B);assert(e.screen()==EntryEditor::Screen::menu && !e.commit_requested());
 e.open(7," café \t Übersetzung ");e.frame(0);
 tap(e,bit(Button::DOWN));tap(e,A);
 assert(e.screen()==EntryEditor::Screen::front && e.target()==7);
 assert(!std::strcmp(e.text().data()," café "));
 tap(e,START|A);assert(!std::strcmp(e.text().data()," Übersetzung "));
 tap(e,B);tap(e,START|A);assert(e.commit_requested() && e.operation()==EntryMutation::edit);
 assert(!std::strcmp(e.row()," café \t Übersetzung"));
 e.finish(true,"");assert(!e.active());
 e.open(2,"erase\tlöschen");e.frame(0);tap(e,bit(Button::DOWN));tap(e,bit(Button::DOWN));tap(e,A);
 assert(e.screen()==EntryEditor::Screen::confirm_delete && e.selection()==0);
 assert(!std::strcmp(e.captured(),"erase\tlöschen"));
 tap(e,A);assert(e.screen()==EntryEditor::Screen::menu && !e.commit_requested());
 tap(e,A);assert(e.screen()==EntryEditor::Screen::confirm_delete);
 tap(e,bit(Button::RIGHT));tap(e,A);
 assert(e.commit_requested() && e.operation()==EntryMutation::remove && e.target()==2);
 e.finish(false,"FAIL");e.frame(0);tap(e,B);
 assert(e.screen()==EntryEditor::Screen::menu && !e.commit_requested());
 e.open(-1,nullptr);e.frame(0);tap(e,A);
 assert(e.text().set_text(std::string(160,'a').c_str()));
 e.text().move_home();e.layout().reflow(e.text(),220,width);
 tap(e,START|bit(Button::R));assert(e.viewport()==e.view_rows());
 tap(e,START|bit(Button::L));assert(e.viewport()==0);
 tap(e,START|bit(Button::SELECT));assert(!e.status_visible()&&e.view_rows()==6);
 tap(e,START);assert(e.message()[0]&&e.view_rows()==5);
 puts("PASS Writer Add draft: real letter chords, two fields, retained back, atomic request, failed save, cancel");
}
