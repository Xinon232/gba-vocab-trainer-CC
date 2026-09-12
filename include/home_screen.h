#pragma once

class HomeScreen {
public:
    explicit HomeScreen(bool destination=false):page_(destination?Page::files:Page::home),destination_(destination){}
    enum class Page { home, files, new_list, confirm, controls, credits };
    enum class Key { a, b, up, down, left, right, select, start };
    enum class Request { none, resume, load, create, dictionary };
    Page page() const { return page_; }
    int selection() const { return selection_; }
    int file_index() const { return file_index_; }
    int help_page() const { return help_page_; }
    Request request() const { return request_; }
    bool save_first() const { return save_first_; }
    void press(Key key, bool active, bool dirty, int count);
    void finish(bool success);
private:
    Page page_ = Page::home;
    Page origin_ = Page::home;
    int selection_ = 0;
    int file_index_ = 0;
    int help_page_ = 0;
    Request request_ = Request::none;
    bool save_first_ = false;
    bool destination_ = false;
};

constexpr int HOME_HELP_PAGES = 30;
constexpr int HOME_CREDIT_PAGES = 5;
const char* home_help_heading(int page);
const char* home_help_line(int page, int line);
const char* home_credit_heading(int page);
const char* home_credit_line(int page, int line);

class Renderer;
struct VocabFile;
// Callbacks own storage and the learning State, including save index remapping.
struct HomeActions {
    void* context;
    bool (*save)(void*);
    bool (*load)(void*, const char*);
    bool (*create)(void*, const char*);
};
enum class HomeResult { opened, save_failed, open_failed };
HomeResult home_apply_request(const HomeScreen&, HomeActions, const char* filename);
// True after a successful switch; false when resuming the retained list.
bool run_home_screen(Renderer&, VocabFile&, HomeActions, bool* dictionary_requested=nullptr, bool destination=false);
