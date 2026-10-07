// SPDX-License-Identifier: GPL-3.0-or-later
#define main base_reader_suite
#include "test_player_probe.c"
#undef main
#include "skills_probe.h"
#include <limits.h>
static void fill_skills(Fixture *f) {
 for(unsigned i=0;i<6;i++){put32(f,PLAYER+0x134C+16*i,1000+i);put32(f,PLAYER+0x1350+16*i,i-1);}
}
static EdenDsmodHostApi skills_setup(Fixture *f,PlayerProbeIdentity *identity) {
 EdenDsmodHostApi h=setup(f);fill_skills(f);PlayerProbeResult p;player_probe(&h,&p);CHECK(p.available);
 *identity=p.identity;f->calls=f->maps=f->skill_reads=0;return h;
}
static void all_skills_clear(const SkillsProbeResult *r) {
 CHECK(!r->available);for(unsigned i=0;i<6;i++)CHECK(!r->slots[i].power_sno&&!r->slots[i].rune);
}
static void skills_valid_and_bounds(void) {
 Fixture f;PlayerProbeIdentity id;EdenDsmodHostApi h=skills_setup(&f,&id);SkillsProbeResult r;
 skills_probe(&h,&id,&r);CHECK(r.available&&r.shared_identity_valid);
 CHECK(r.reads==51&&r.bytes==408&&f.maps==51&&f.calls==51&&f.skill_reads==12);
 for(unsigned i=0;i<6;i++) {
  CHECK(r.slots[i].power_sno==(int32_t)(1000+i)&&r.slots[i].rune==(int32_t)i-1);
  CHECK(f.addresses[13+i]==PLAYER+0x134C+16*i&&f.lengths[13+i]==8);
  CHECK(f.addresses[32+i]==PLAYER+0x134C+16*i&&f.lengths[32+i]==8);
 }
 CHECK(f.addresses[18]+f.lengths[18]==PLAYER+0x13A4&&f.writes==0);release(&f);
}
static void skills_failure_positions(void) {
 for(unsigned mapping=0;mapping<2;mapping++)for(unsigned n=1;n<=51;n++) {
  Fixture f;PlayerProbeIdentity id;EdenDsmodHostApi h=skills_setup(&f,&id);
  if(mapping)f.map_fail_on=n;else f.fail_on=n;
  SkillsProbeResult r={.available=1,.slots={{123,4}}};skills_probe(&h,&id,&r);all_skills_clear(&r);
  int pair_failure=(n>=14&&n<=19)||(n>=33&&n<=38);
  CHECK(r.shared_identity_valid==pair_failure);
  CHECK(r.reads<=51&&r.bytes<=408&&f.writes==0);
  if(pair_failure)CHECK(f.calls>n);release(&f);
 }
}
static void skills_semantics(void) {
 const struct{uint32_t power,rune;int available;} cases[]={
  {UINT32_MAX,UINT32_MAX,1},{UINT32_MAX,5,1},{UINT32_MAX,UINT32_C(0x80000000),1},{UINT32_MAX,INT32_MAX,1},
  {0,UINT32_MAX,1},{INT32_MAX,4,1},{1000,0,1},{1000,4,1},
  {UINT32_MAX-1,0,0},{UINT32_C(0x80000000),0,0},{1000,5,0},{1000,UINT32_MAX-1,0},{1000,UINT32_C(0x80000000),0},{1000,INT32_MAX,0}
 };
 for(unsigned slot=0;slot<6;slot++)for(unsigned c=0;c<sizeof cases/sizeof cases[0];c++) {
  Fixture f;PlayerProbeIdentity id;EdenDsmodHostApi h=skills_setup(&f,&id);
  put32(&f,PLAYER+0x134C+16*slot,cases[c].power);put32(&f,PLAYER+0x1350+16*slot,cases[c].rune);
  SkillsProbeResult r;skills_probe(&h,&id,&r);CHECK(r.available==cases[c].available&&r.shared_identity_valid);
  if(!r.available)all_skills_clear(&r);else if(cases[c].power==UINT32_MAX)CHECK(r.slots[slot].power_sno==-1&&r.slots[slot].rune==-1);
  CHECK(r.reads==51&&r.bytes==408);release(&f);
 }
}
static void skills_changed_pairs(void) {
 for(unsigned slot=0;slot<6;slot++)for(unsigned field=0;field<2;field++) {
  Fixture f;PlayerProbeIdentity id;EdenDsmodHostApi h=skills_setup(&f,&id);
  f.mutate_on=33;f.change_address=PLAYER+0x134C+16*slot+4*field;f.change_value=field?(slot==5?0:4):1234;f.change_size=4;
  SkillsProbeResult r;skills_probe(&h,&id,&r);all_skills_clear(&r);CHECK(r.shared_identity_valid&&strstr(r.reason,"changed"));release(&f);
 }
 // Empty slots ignore rune semantics, but the raw pair still must be stable.
 Fixture f;PlayerProbeIdentity id;EdenDsmodHostApi h=skills_setup(&f,&id);put32(&f,PLAYER+0x134C,UINT32_MAX);
 f.mutate_on=33;f.change_address=PLAYER+0x1350;f.change_value=5;f.change_size=4;
 SkillsProbeResult r;skills_probe(&h,&id,&r);all_skills_clear(&r);CHECK(r.shared_identity_valid);release(&f);
}
static void skills_changed_identity(void) {
 const struct{uint64_t address,value;size_t size;} changes[]={
  {MAIN+0x114A840,0,8},{C+0x84,0,4},{C+0x84,UINT32_MAX,4},{C+0x948,PLAYERS+4,8},
  {SEL+16,2,4},{SEL+4,0,4},{PLAYER+4,124,4},{PLAYER+8,UINT32_MAX,4},{POOL+0x100,5,4},{ACTOR,0,4}
 };
 for(unsigned start=1;start<=39;start+=19)for(unsigned c=0;c<sizeof changes/sizeof changes[0];c++) {
  Fixture f;PlayerProbeIdentity id;EdenDsmodHostApi h=skills_setup(&f,&id);
  f.mutate_on=start;f.change_address=changes[c].address;f.change_value=changes[c].value;f.change_size=changes[c].size;
  SkillsProbeResult r;skills_probe(&h,&id,&r);all_skills_clear(&r);CHECK(!r.shared_identity_valid);release(&f);
 }
 // A skill read failure must still recheck ownership before preserving level.
 Fixture f;PlayerProbeIdentity id;EdenDsmodHostApi h=skills_setup(&f,&id);
 f.fail_on=14;f.mutate_on=14;f.change_address=C+0x84;f.change_value=0;f.change_size=4;
 SkillsProbeResult r;skills_probe(&h,&id,&r);all_skills_clear(&r);CHECK(!r.shared_identity_valid);release(&f);
 h=skills_setup(&f,&id);f.fail_on=14;f.map_fail_on=15;
 skills_probe(&h,&id,&r);all_skills_clear(&r);CHECK(!r.shared_identity_valid&&f.calls==14);release(&f);
}
static void skills_address_overflow(void) {
 Fixture f;PlayerProbeIdentity id;EdenDsmodHostApi h=skills_setup(&f,&id);
 // Relocate only the synthetic Player region near UINT64_MAX. First pair fits;
 // the second address overflows. Route IDs remain readable for the final check.
 const uint64_t player=UINT64_MAX-UINT64_C(0x1357), players=player-0x60;
 f.regions[4].base=players;f.regions[4].size=0x13B8;put64(&f,C+0x948,players);
 id.players=players;id.player=player;
 SkillsProbeResult r;skills_probe(&h,&id,&r);all_skills_clear(&r);
 CHECK(r.shared_identity_valid&&r.reads==27&&r.bytes==216&&f.writes==0);release(&f);
}
static void skills_invalid_host(void) {
 Fixture f;PlayerProbeIdentity id;EdenDsmodHostApi h=skills_setup(&f,&id);SkillsProbeResult r;
 skills_probe(NULL,&id,&r);all_skills_clear(&r);skills_probe(&h,NULL,&r);all_skills_clear(&r);
 h.main_size=UINT64_MAX;skills_probe(&h,&id,&r);all_skills_clear(&r);CHECK(f.calls==0);release(&f);
}
static void skills_suite(void) {
 base_reader_suite();skills_valid_and_bounds();skills_failure_positions();skills_semantics();skills_changed_pairs();skills_changed_identity();skills_address_overflow();skills_invalid_host();
 puts("PASS synthetic skills: six stored pairs, exact bounds, all read/map failures, ID/rune guards, ownership changes; no writes");
}
