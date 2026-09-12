
extern std::string fat_root;
extern std::function<FRESULT(const std::string&,const std::string&)> fat_hook;
static constexpr unsigned U=1,A=16,B=32,ST=256,SE=512,RIGHT=8,LEFT=4;
static void idle(unsigned n=4){while(n--)frames.push_back(0);}
static void tap(unsigned k){frames.push_back(k);idle();}
static std::string bytes(const std::string& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
 assert(argc==2);char dir[]="/tmp/screen-storage-XXXXXX";assert(mkdtemp(dir));fat_root=dir;std::filesystem::create_directory(fat_root+"/gbavocab");
 std::ofstream(fat_root+"/cards.txt")<<"outer\tdraft\textra\n";vocab_file_init();VocabFile list;char buf[2048];int used=0;
 assert(vocab_file_load("cards.txt",list,buf,sizeof buf,used));assert(list.languages.set("en","de"));auto txt=bytes(fat_root+"/cards.txt");
 Renderer renderer;
 for(bool entry:{false,true})for(unsigned operation:{SE,LEFT,RIGHT})for(int outcome=0;outcome<4;++outcome){
  std::filesystem::copy_file(argv[1],fat_root+"/gbavocab/fixture.dict",std::filesystem::copy_options::overwrite_existing);
  auto base=bytes(fat_root+"/gbavocab/fixture.dict");
  outer.open(0,"outer\tdraft\textra");outer.frame(0);outer.frame(2);outer.frame(0);outer.frame(A);outer.frame(0);outer.frame(ST|A);outer.frame(0);
  unsigned char snapshot[sizeof outer];std::memcpy(snapshot,&outer,sizeof outer);
  frames.clear();frame_index=0;seen_lines.clear();idle(8);if(operation!=SE)tap(ST|2);tap(ST|operation);
  if(outcome==0){if(operation==RIGHT)tap(A);else tap(ST|B);}
  else if(operation==RIGHT){tap(RIGHT);tap(A);}
  else {tap(U|B);tap(ST|A);if(operation==SE)tap(U|A);tap(ST|A);}
  bool fault=false;
  if(outcome>=2){
   fat_hook=[&](const std::string& op,const std::string& p){if(op=="write"&&p.find(".dict")!=std::string::npos&&!fault){fault=true;return FR_DISK_ERR;}return FR_OK;};
   if(outcome==2){if(operation==RIGHT)tap(A);else tap(ST|A);}
   else if(operation==RIGHT)tap(B);else {tap(ST|B);tap(ST|B);}
  }
  if(outcome==1||outcome==2)tap(B); // saved notice
  tap(ST|B);idle(8);DictionaryResult result;
  assert(!run_dictionary_screen(renderer,entry?&list:nullptr,result));fat_hook={};
  assert(!std::memcmp(snapshot,&outer,sizeof outer));assert(bytes(fat_root+"/cards.txt")==txt);
  auto after=bytes(fat_root+"/gbavocab/fixture.dict");assert(after.substr(0,base.size())==base);
  if(!outcome)assert(after==base);else {assert(after.size()>base.size());if(outcome>=2)assert(fault);}
  DictionaryCatalog catalog(true,vocab_file_dictionary_fil(),vocab_file_dictionary_opened());DictionarySearch verify(additions);verify.open(catalog.dictionary(0));verify.search(0,"");
  bool committed=outcome==1||outcome==2;
  assert(verify.count()==(committed?(operation==SE?3:operation==RIGHT?1:2):2));
  if(committed&&operation==LEFT){verify.search(0,"samea");assert(verify.count()==1);char front[192],back[192];assert(verify.read(0,front,back)&&!std::strcmp(back,"two"));}
  if(committed&&operation==SE){verify.search(0,"a");assert(verify.count()==1);}
 }
 // Explicit chooser cancellation preserves query, result selection and preference.
 std::filesystem::copy_file(argv[1],fat_root+"/gbavocab/fixture.dict",std::filesystem::copy_options::overwrite_existing);
 std::filesystem::copy_file(argv[1],fat_root+"/gbavocab/other.dict");list.settings.prefer("fixture.dict");list.pair_dirty=false;
 frames.clear();frame_index=0;idle(8);tap(64|2|128);tap(ST|128);tap(2);tap(B);tap(ST|A);idle(8);DictionaryResult chosen;
 assert(run_dictionary_screen(renderer,&list,chosen));assert(!std::strcmp(chosen.front,"same")&&!std::strcmp(chosen.back,"one"));assert(!std::strcmp(list.settings.dictionary,"fixture.dict")&&!list.pair_dirty);assert(bytes(fat_root+"/cards.txt")==txt);
 assert(!std::strcmp(query.text().data(),"s"));
 std::filesystem::remove(fat_root+"/gbavocab/other.dict");auto legacy=bytes(argv[1]);legacy[7]='1';dict_format::put(reinterpret_cast<unsigned char*>(&legacy[156]),dict_format::crc(legacy.data(),156));
 std::ofstream(fat_root+"/gbavocab/fixture.dict",std::ios::binary|std::ios::trunc).write(legacy.data(),legacy.size());
 for(bool entry:{false,true})for(unsigned op:{LEFT,RIGHT}){
  frames.clear();frame_index=0;seen_lines.clear();idle(8);tap(ST|op);tap(B);tap(ST|B);idle(8);
  assert(!run_dictionary_screen(renderer,entry?&list:nullptr,chosen));assert(bytes(fat_root+"/gbavocab/fixture.dict")==legacy);
  assert(std::find(seen_lines.begin(),seen_lines.end(),"Upgrade .dict with PC Save As")!=seen_lines.end());
 }
 std::filesystem::remove_all(fat_root);puts("PASS production screen loops: both routes New/Edit/Delete cancel/success/failure/retry, exact outer restoration, chooser cancellation, selected copy, immutable TXT/base bytes (UI doubles)");
}
