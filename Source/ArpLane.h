#pragma once
#include "SequencerClock.h"
#include <array>
#include <algorithm>
#include <random>
namespace jerzy {
// Fixed storage: MIDI updates and clock ticks allocate no memory in the audio thread.
class ArpLane {
public:
 struct Settings {bool enabled=false,sync=true,latch=false,retrigger=true;int division=4,pattern=0,rhythm=0,octaves=1;double gate=.72,swing=0,velocity=.9,probability=1;};
 struct Event {int off=-1,on=-1;float velocity=0;};
 Settings settings;
 int current=-1;
 void prepare(unsigned seed){rng.seed(seed);heldCount=latchedCount=0;resetClock();current=-1;}
 void resetClock(){step=0;lastHostStep=-1;next=0;lastRandom=-1;origin=0;}
 void restart(double ppq){step=0;lastHostStep=-1;next=0;lastRandom=-1;origin=ppq;}
 void keys(const int* notes,int count,bool noteOn,double ppq) {
  if(!settings.latch || (noteOn && heldCount==0))latchedCount=0;
  heldCount=count;for(int i=0;i<count;++i)held[i]=notes[i];
  for(int i=0;i<count;++i) {
   bool found=false;for(int j=0;j<latchedCount;++j)found|=latched[j]==notes[i];
   if(!found && latchedCount<128)latched[latchedCount++]=notes[i];
  }
  if(noteOn && settings.retrigger)restart(ppq);
 }
 void clearKeys(){heldCount=latchedCount=0;}
 int release(){const int n=current;current=-1;return n;}
 Event process(double sr,double bpm,double ppq,bool hasPpq,bool playing,int source1,int source2,int launch) {
  Event e;
  if(!settings.latch && wasLatch){latchedCount=heldCount;for(int i=0;i<heldCount;++i)latched[i]=held[i];}
  wasLatch=settings.latch;
  if(!settings.latch && heldCount==0)latchedCount=0;
  const bool preview=source1>=0 || source2>=0 || launch>=0;
  const bool hostClock=settings.sync && hasPpq && playing;
  const bool run=settings.enabled && (!settings.sync || hostClock || preview);
  const int count=latchedCount+(preview?1:0);
  if(!run || count==0){e.off=release();if(!run){next=0;lastHostStep=-1;}return e;}
  static constexpr double quarters[]={4,2,1,.5,.25,.125,2.0/3,1.0/3,1.0/6,1.5,.75,.375};
  const double quarter=quarters[std::clamp(settings.division,0,11)];
  bool tick=false;
  if(hostClock){const int s=swungStepAtPpq(ppq-origin,quarter,settings.swing);if(s!=lastHostStep){lastHostStep=s;step=std::max(0,s);tick=true;}}
  else tick=next<=0;
  if(tick) {
   e.off=release();
   duration=sr*60/std::max(1.0,bpm)*quarter*((step&1)?1-settings.swing:1+settings.swing);
   next=duration;
   if(rhythmGate(settings.rhythm,step) && std::generate_canonical<double,24>(rng)<settings.probability) {
    std::array<int,128> notes{};int n=0;
    auto add=[&](int value){if(value<0)return;for(int i=0;i<n;++i)if(notes[i]==value)return;if(n<128)notes[n++]=value;};
    for(int i=0;i<latchedCount;++i)add(latched[i]);add(source1);add(source2);add(launch);
    if(n>0){if(settings.pattern!=4)std::sort(notes.begin(),notes.begin()+n);
     const int base=choose(notes,n,settings.pattern,step);
     current=std::clamp(base+12*((step/std::max(1,n))%settings.octaves),0,127);
     e.on=current;e.velocity=(float)settings.velocity;
    }
   }
   ++step;
  }
  if(current>=0 && next<=duration*(1-settings.gate)){e.off=release();}
  next-=1;
  return e;
 }
 static bool rhythmGate(int r,int step) {
  static constexpr int masks[6][8]={{1,0,1,1,0,1,0,1},{1,1,0,1,0,1,1,0},{1,0,0,1,0,0,1,0},{1,0,1,0,0,1,0,0},{1,0,1,1,0,1,1,0},{0,1,0,1,0,1,0,1}};
  if(r==1)return step%2==0;
  if(r>=2 && r<=6)return masks[r-2][step&7]!=0;
  if(r==7)return masks[5][step&7]!=0;
  if(r==8)return ((step*3)%8)<3;
  if(r==9)return ((step*5)%8)<5;
  return true;
 }
private:
 int choose(const std::array<int,128>& notes,int n,int mode,int s) {
  const int span=std::max(1,2*n-2),p=s%span;
  switch(mode) {
   case 1:return notes[n-1-s%n];
   case 2:return notes[p<n?p:span-p];
   case 3:{int i=0;if(n>1){if(lastRandom<0||lastRandom>=n){std::uniform_int_distribution<int>d(0,n-1);i=d(rng);}else{std::uniform_int_distribution<int>d(0,n-2);i=d(rng);if(i>=lastRandom)++i;}}lastRandom=i;return notes[i];}
   case 4:return notes[s%n];
   case 5:return notes[(s/2)%n];
   case 6:return notes[n-1-(s/2)%n];
   case 7:{int t=s%(2*n);return notes[t<n?t:2*n-1-t];}
   case 8:{int low=(s/2)%((n+1)/2);return notes[(s&1)?n-1-low:low];}
   case 9:{int t=s%n,mid=(n-1)/2;int i=t%2?mid+(t+1)/2:mid-t/2;return notes[std::clamp(i,0,n-1)];}
   case 10:{if(lastRandom<0||lastRandom>=n)lastRandom=0;else {lastRandom+=(rng()&1)?1:-1;if(lastRandom<0)lastRandom=std::min(1,n-1);if(lastRandom>=n)lastRandom=std::max(0,n-2);}return notes[lastRandom];}
   case 11:return notes[n-1-(p<n?p:span-p)];
   default:return notes[s%n];
  }
 }
 std::array<int,128> held{},latched{};bool wasLatch=false;int heldCount=0,latchedCount=0,step=0,lastHostStep=-1,lastRandom=-1;
 double next=0,duration=0,origin=0;std::mt19937 rng{0x51a7};
};
}
