#pragma once
#include "test_support.h"
#include "BrakeSettings.h"
using namespace wired_brake;
struct CountingStore:ConfigStore {
 PersistRecord record{};LoadResult loadResult=LoadResult::Missing,readResult=LoadResult::Valid;
 StoreResult commitResult=StoreResult::Committed;unsigned commitCount=0,loads=0;bool writeOnUnknown=false;
 LoadResult load(PersistRecord& p)override{++loads;p=record;return loadResult;}
 StoreResult commit(const PersistRecord& p)override{++commitCount;if(commitResult==StoreResult::Committed||writeOnUnknown){record=p;loadResult=LoadResult::Valid;}return commitResult;}
 LoadResult readBack(uint32_t,PersistRecord& p)override{p=record;return readResult;}
};
inline BrakeCommand readCommand(uint32_t host=7){BrakeCommand c{};c.hostBootId=host;return c;}
inline BrakeCommand saveCommand(uint32_t id=1,uint32_t version=0){BrakeCommand c{};c.op=CommandOp::ApplySave;c.hostBootId=7;c.targetBrakeBootId=11;c.commandId=id;c.expectedConfigVersion=version;c.candidate={650,true};return c;}
