#include "entry_render.h"
extern "C" {
#include "../references/gbawriter/src/fonts/font_render.h"
}
namespace {
// GBAWriter's bus-safe bitmap primitives and glyph loop, with a reserved
// heading/field-label region. Body x, width, 16px glyphs and 18px pitch retained.
void line(uint8_t* px,int x,int y,const char* s,int max=224) {
    draw_text_idx8_bus16_range(s,px+y*240+x,0,max,240,1);
}
void pixel(uint8_t* px,int x,int y) {
    volatile uint16_t* p=reinterpret_cast<volatile uint16_t*>(px+y*240+(x&~1));
    *p=(x&1)?uint16_t((*p&0x00ff)|0x0100):uint16_t((*p&0xff00)|1);
}
}
int entry_glyph_width(const char* text) {return int(font_width(text));}
void render_entry(EntryEditor& e,uint8_t* px,EntryUiLine ui,void* context) {
    // Halfword stores are mandatory for GBA indexed VRAM.
    auto* words=reinterpret_cast<volatile uint16_t*>(px);
    for(int i=0;i<240*160/2;++i) words[i]=0;
    ui(context,8,0,e.heading());
    using S=EntryEditor::Screen;
    if(e.screen()==S::menu) {
        const char* normal[]={"  Add entry","  Edit entry","  Delete entry"};
        const char* selected[]={"> Add entry","> Edit entry","> Delete entry"};
        for(int i=0;i<3;++i)ui(context,24,36+i*24,e.selection()==i?selected[i]:normal[i]);
        ui(context,8,144,"Up/Down  A: Select  B: Back");
    } else if(e.screen()==S::confirm_delete) {
        // Both captured fields are shown in full. Only the confirmation
        // preview adapts to half-size glyphs for a maximum-length entry.
        writer::TextModel text; writer::Layout layout;
        char row[VOCAB_RAW_LINE_MAX];std::strcpy(row,e.captured());
        char* tab=std::strchr(row,'\t');
        if(tab) {
            *tab=0;char* extra=std::strchr(tab+1,'\t');if(extra)*extra=0;
            const char* fields[]={row,tab+1};int rows=0;
            for(auto field:fields)if(text.set_text(field)) {
                layout.reflow(text,220,entry_glyph_width);rows+=layout.rows();
            }
            const int scale=rows>4?2:1;int visible_row=0;
            for(auto field:fields)if(text.set_text(field)) {
                layout.reflow(text,220*scale,entry_glyph_width);
                for(int r=0;r<layout.rows();++r,++visible_row) {
                    int x=0,y=22+visible_row*18/scale;
                    auto end=r+1<layout.rows()?layout.row_start(r+1):text.bytes();
                    for(auto p=layout.row_content_start(text,r);p<end;) {
                        char ch[5];p=writer::Layout::character(text.data(),p,ch);
                        int w=layout.width(ch);
                        if(w && scale==1)line(px,8+x,y,ch,220-x);
                        else if(w) {
                            alignas(2) uint8_t glyph[16*16]={};
                            draw_text_idx8_bus16_range(ch,glyph,0,16,16,1);
                            for(int gy=0;gy<8;++gy)for(int gx=0;gx<(w+1)/2;++gx)
                                if(glyph[gy*32+gx*2]||glyph[gy*32+gx*2+1]||
                                   glyph[gy*32+16+gx*2]||glyph[gy*32+16+gx*2+1])
                                    pixel(px,8+x/2+gx,y+gy);
                        }
                        x+=w;
                    }
                }
            }
        }
        ui(context,8,96,"Are you sure?");
        ui(context,24,114,e.selection()==0?"> No":"  No");
        ui(context,104,114,e.selection()==1?"> Yes":"  Yes");
        ui(context,8,144,"Up/Down  A: Confirm  B: Back");
    } else if(e.screen()==S::front || e.screen()==S::back) {
        ui(context,8,18,e.screen()==S::front?"Word / front":"Translation / back");
        auto& text=e.text();auto& layout=e.layout();const char* s=text.data();
        int last=e.viewport()+e.view_rows();if(last>layout.rows())last=layout.rows();
        for(int row=e.viewport();row<last;++row) {
            int x=8,y=36+(row-e.viewport())*writer::TEXT_PITCH;
            auto end=row+1<layout.rows()?layout.row_start(row+1):text.bytes();
            for(auto p=layout.row_content_start(text,row);p<end;) {
                char ch[5];p=writer::Layout::character(s,p,ch);if(ch[0]=='\n')break;
                int w=layout.width(ch);if(w&&ch[0]!='\t')line(px,x,y,ch,228-x);x+=w;
            }
        }
        auto caret=layout.position(text,text.caret_byte());
        if(e.caret_visible()&&caret.row>=e.viewport()&&caret.row<last) {
            int x=8+caret.x,y=36+(caret.row-e.viewport())*writer::TEXT_PITCH;
            for(int j=0;j<16;++j)pixel(px,x,y+j);
        }
        if(e.status_visible() && !e.message()[0]) {
            line(px,144,128,e.input().active_group(),32);
            if(e.input().caps())line(px,184,128,"Caps",48);
            else if(e.input().shift_armed())line(px,184,128,"Shift",48);
        }
        ui(context,8,144,e.screen()==S::front?"Start+A: Next   Start+B: Back":"Start+A: Save   Start+B: Back");
    }
    if(e.message()[0])ui(context,8,128,e.message());
}
