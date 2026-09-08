// White-box fault boundary: production parser/cache with source quarantined.
#include "../src/vocab_file_io.cpp"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
 VocabFile v={};v.reset();v.loaded=true;v.line_count=1;v.field[0]=2;v.field_counts[1]=1;
 v.line_offsets[0]=PENDING_ROW_BASE;std::strcpy(s_pending_rows[0],"pending\ttranslation");
 s_loaded_from_sd=true;s_source_verified=false;
 LineBuf row;assert(vocab_file_show(v,nullptr,0,0,row));
 assert(!std::strcmp(row.a,"pending") && row.field==2);
 v.line_offsets[0]=0;invalidate_card_cache();
 assert(!vocab_file_show(v,nullptr,0,0,row)); // Never read unverified old offsets.
 puts("PASS quarantined SD preserves RAM card display without permitting stale source reads");
}
