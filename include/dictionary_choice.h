#pragma once
#include "dictionary.h"
#include "vocab.h"
// Bounded screen-local chooser state; catalog IDs are never persisted.
struct DictionaryChoice {
 int eligible[DictionaryCatalog::CAPACITY]={},count=0,choice=0;
 bool chooser=false;
 DictionaryChoice(DictionaryCatalog& catalog,const VocabFile* target){
  int preferred=-1;
  for(int i=0;i<catalog.count();++i){
   if(!catalog.dictionary(i).valid()||(target&&target->languages.present()&&catalog.match(i,target->languages.front,target->languages.back)<0))continue;
   if(target&&target->languages.present()&&ListSettings::filename(target->settings.dictionary)&&target->settings.dictionary[0]&&!std::strcmp(target->settings.dictionary,catalog.filename(i)))preferred=count;
   eligible[count++]=i;
  }
  chooser=count>1;
  if(preferred>=0){choice=preferred;chooser=false;}
  else if(target&&target->settings.dictionary[0]&&count)chooser=true;
 }
 bool remember(DictionaryCatalog& catalog,VocabFile& target)const{
  if(choice<0||choice>=count||!target.languages.present()||catalog.match(eligible[choice],target.languages.front,target.languages.back)<0)return false;
  const char* name=catalog.filename(eligible[choice]);
  if(!std::strcmp(target.settings.dictionary,name))return true;
  if(!target.settings.prefer(name))return false;
  target.pair_dirty=true;return true;
 }
};
