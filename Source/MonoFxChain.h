#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>
#include <cmath>

class MonoFxChain
{
public:
    static constexpr int count = 6;
    MonoFxChain() = default;
    void prepare(double sr, int maxBlock)
    {
        sampleRate = juce::jmax(8000.0, sr);
        const int size = juce::jmax(8192, (int)(sampleRate * 4.1));
        delayL.assign((size_t)size, 0.0f); delayR.assign((size_t)size, 0.0f); chorusL.assign((size_t)size,0.0f); chorusR.assign((size_t)size,0.0f); widthLine.assign((size_t)juce::jmax(64,(int)(sampleRate*0.03)),0.0f);
        writePos = chorusWritePos = widthPos = 0; compEnvelope = 0.0f; chorusPhase = rotaryPhase = 0.0;
        juce::ignoreUnused(maxBlock);
        reverb.setSampleRate(sampleRate);
        reverb.reset();
        reverbParams.roomSize = 0.6f; reverbParams.damping = 0.35f;
        reverbParams.wetLevel = 0.25f; reverbParams.dryLevel = 0.75f;
        reverbParams.width = 1.0f; reverbParams.freezeMode = 0.0f;
        reverb.setParameters(reverbParams);
    }
    std::array<int, count> getOrder() const
    {
        const auto packed=orderPacked.load(std::memory_order_acquire);
        std::array<int,count> out{};
        for(int i=0;i<count;++i)out[(size_t)i]=(int)((packed>>(i*4))&0x0fu);
        return out;
    }
    void setOrder(const std::array<int, count>& value)
    {
        bool seen[count]{}; bool valid=true;
        for(int v:value){if(v<0||v>=count||seen[v]){valid=false;break;}seen[v]=true;}
        const std::array<int,count> fallback{0,1,2,3,4,5};
        const auto& src=valid?value:fallback;
        uint64_t packed=0;
        for(int i=0;i<count;++i)packed|=((uint64_t)src[(size_t)i]&0x0fu)<<(i*4);
        orderPacked.store(packed,std::memory_order_release);
    }
    void move(int from, int to)
    {
        from=juce::jlimit(0,count-1,from); to=juce::jlimit(0,count-1,to);
        auto a=getOrder(); const int x=a[(size_t)from];
        if(from<to) for(int i=from;i<to;++i) a[(size_t)i]=a[(size_t)i+1];
        else for(int i=from;i>to;--i) a[(size_t)i]=a[(size_t)i-1];
        a[(size_t)to]=x; setOrder(a);
    }
    void process(juce::AudioBuffer<float>& b, const juce::AudioProcessorValueTreeState& state, double bpm)
    {
        if (b.getNumChannels()<1 || b.getNumSamples()==0) return;
        const bool stereo=b.getNumChannels()>1;
        auto v=[&](const char* id){auto* p=state.getRawParameterValue(id); return p?p->load(std::memory_order_relaxed):0.0f;};
        const int n=b.getNumSamples(); float* l=b.getWritePointer(0); float* r=stereo?b.getWritePointer(1):l;
        const float threshold=v("fxCompThreshold"), ratio=juce::jmax(1.0f,v("fxCompRatio"));
        const float compAttack=(float)std::exp(-1.0/(sampleRate*juce::jmax(0.0005f,v("fxCompAttack"))));
        const float compRelease=(float)std::exp(-1.0/(sampleRate*juce::jmax(0.005f,v("fxCompRelease"))));
        const int delayDiv=(int)v("fxDelayDivision"), delayMode=(int)v("fxDelayMode");
        static constexpr double q[]={4.0,2.0,1.0,0.5,0.25,0.125,2.0/3.0,1.0/3.0,1.0/6.0,1.5,0.75,0.375};
        const int delaySamples=juce::jlimit(1,(int)delayL.size()-1,(int)(sampleRate*60.0/juce::jmax(20.0,bpm)*q[juce::jlimit(0,11,delayDiv)]));
        const float delayFb=juce::jlimit(0.0f,0.94f,v("fxDelayFeedback")), delayMix=juce::jlimit(0.0f,1.0f,v("fxDelayMix"));
        const float room=juce::jlimit(0.0f,1.0f,v("fxReverbSize")), damp=juce::jlimit(0.0f,1.0f,v("fxReverbDamping"));
        const float reverbMix=juce::jlimit(0.0f,1.0f,v("fxReverbMix"));
        const float width=juce::jlimit(0.0f,2.0f,v("fxWidth"));
        const int chorusMode=(int)v("fxChorusMode"); const float chorusRate=juce::jlimit(0.05f,8.0f,v("fxChorusRate"));
        const float chorusDepth=juce::jlimit(0.0f,1.0f,v("fxChorusDepth")), chorusMix=juce::jlimit(0.0f,1.0f,v("fxChorusMix"));
        const float chorusFb=juce::jlimit(-0.85f,0.85f,v("fxChorusFeedback"));
        const float rotaryDepth=juce::jlimit(0.0f,1.0f,v("fxRotaryDepth"));
        const bool rotarySync=v("fxRotarySync")>0.5f; const int rotaryDiv=(int)v("fxRotaryDivision");
        const double rotaryHz=rotarySync ? (bpm/60.0)/q[juce::jlimit(0,11,rotaryDiv)] : juce::jlimit(0.1f,8.0f,v("fxRotaryRate"));
        const auto chain=getOrder();
        for(int slot=0;slot<count;++slot)
        {
            const int effect=chain[(size_t)slot];
            if(v(enabledIds[effect])<0.5f) continue;
            // The linked compressor also works on a mono instrument bus.
            // Stereo-only spatial effects remain bypassed on mono layouts.
            if(!stereo && effect!=0) continue;
            switch(effect)
            {
                case 0:
                    for(int i=0;i<n;++i)
                    {
                        const float peak=juce::jmax(std::abs(l[i]),std::abs(r[i]));
                        const float c=peak>compEnvelope?compAttack:compRelease;
                        compEnvelope=c*compEnvelope+(1.0f-c)*peak;
                        const float db=juce::Decibels::gainToDecibels(juce::jmax(1.0e-7f,compEnvelope));
                        constexpr float knee=6.0f;
                        const float above=db-threshold;
                        float reductionDb=0.0f;
                        if(above>knee*0.5f) reductionDb=-above*(1.0f-1.0f/ratio);
                        else if(above>-knee*0.5f) reductionDb=-(above+knee*0.5f)*(above+knee*0.5f)/(2.0f*knee)*(1.0f-1.0f/ratio);
                        // Conservative automatic makeup restores body after compression.
                        const float makeupDb=juce::jlimit(0.0f,6.0f,-threshold*(1.0f-1.0f/ratio)*0.18f);
                        const float gain=juce::Decibels::decibelsToGain(reductionDb+makeupDb);
                        const float amount=juce::jlimit(0.0f,1.0f,v("fxCompDrive"));
                        const float pre=1.0f+amount*9.0f;
                        const float dryL=l[i],dryR=r[i];
                        const float colourL=std::tanh(dryL*pre),colourR=std::tanh(dryR*pre);
                        l[i]=(dryL+(colourL-dryL)*amount)*gain;
                        r[i]=(dryR+(colourR-dryR)*amount)*gain;
                    } break;
                case 1:
                    for(int i=0;i<n;++i)
                    {
                        const int rd=(writePos-delaySamples+(int)delayL.size())%(int)delayL.size();
                        const float dl=delayL[(size_t)rd], dr=delayR[(size_t)rd], inL=l[i], inR=r[i];
                        const float fbL=delayMode==2?dr:dl, fbR=delayMode==2?dl:dr;
                        if(delayMode==2)
                        {
                            delayL[(size_t)writePos]=0.5f*(inL+inR)+fbL*delayFb;
                            delayR[(size_t)writePos]=fbR*delayFb;
                        }
                        else
                        {
                            delayL[(size_t)writePos]=delayMode==0?0.5f*(inL+inR)+dl*delayFb:inL+dl*delayFb;
                            delayR[(size_t)writePos]=delayMode==0?0.0f:inR+dr*delayFb;
                        }
                        l[i]=inL*(1.0f-delayMix)+dl*delayMix;
                        r[i]=delayMode==0 ? (inR*(1.0f-delayMix)+dl*delayMix) : inR*(1.0f-delayMix)+dr*delayMix;
                        if(++writePos >= (int)delayL.size()) writePos=0;
                    } break;
                case 2:
                    reverbParams.roomSize=room; reverbParams.damping=damp;
                    reverbParams.wetLevel=reverbMix; reverbParams.dryLevel=1.0f-reverbMix;
                    reverb.setParameters(reverbParams);
                    reverb.processStereo(l,r,n); break;
                case 3:
                    for(int i=0;i<n;++i){const float mid=0.5f*(l[i]+r[i]);const int tap=(int)(sampleRate*(0.003+0.009*juce::jlimit(0.0f,1.0f,width*0.5f)));const float delayed=widthLine[(size_t)((widthPos-tap+(int)widthLine.size())%(int)widthLine.size())];widthLine[(size_t)widthPos]=mid;const float side=0.5f*(l[i]-r[i])+0.35f*(mid-delayed)*width;l[i]=mid+side;r[i]=mid-side;if(++widthPos>=(int)widthLine.size())widthPos=0;} break;
                case 4:
                    for(int i=0;i<n;++i)
                    {
                        const double phase=2.0*juce::MathConstants<double>::pi*chorusPhase;
                        const float base=chorusMode==2?3.5f:chorusMode==1?0.8f:9.0f;
                        const float excursion=chorusMode==1?1.2f:chorusDepth*(chorusMode==2?2.0f:8.0f);
                        const float dL=(base+excursion*(0.5f+0.5f*(float)std::sin(phase)))*0.001f*(float)sampleRate;
                        const float dR=(base+excursion*(0.5f+0.5f*(float)std::sin(phase+1.7)))*0.001f*(float)sampleRate;
                        const float inL=l[i],inR=r[i]; chorusL[(size_t)chorusWritePos]=inL+chorusFb*chorusL[(size_t)((chorusWritePos+(int)chorusL.size()-1)%(int)chorusL.size())];
                        chorusR[(size_t)chorusWritePos]=inR+chorusFb*chorusR[(size_t)((chorusWritePos+(int)chorusR.size()-1)%(int)chorusR.size())];
                        auto read=[&](const std::vector<float>& a,float d){int ix=(writePos-(int)d+(int)a.size())%(int)a.size();return a[(size_t)ix];};
                        l[i]=inL*(1.0f-chorusMix)+read(chorusL,dL)*chorusMix; r[i]=inR*(1.0f-chorusMix)+read(chorusR,dR)*chorusMix;
                        if(++chorusWritePos >= (int)chorusL.size()) chorusWritePos=0;
                        chorusPhase+=chorusRate/sampleRate; if(chorusPhase>=1.0)chorusPhase-=1.0;
                    } break;
                case 5:
                    for(int i=0;i<n;++i)
                    {
                        const float inL=l[i],inR=r[i]; const float trem=1.0f-rotaryDepth*(0.35f+0.25f*(float)std::sin(2.0*juce::MathConstants<double>::pi*rotaryPhase));
                        const float pan=rotaryDepth*0.45f*(float)std::sin(2.0*juce::MathConstants<double>::pi*rotaryPhase);
                        l[i]=inL*trem*(1.0f-pan); r[i]=inR*trem*(1.0f+pan);
                        rotaryPhase+=rotaryHz/sampleRate; if(rotaryPhase>=1.0)rotaryPhase-=1.0;
                    } break;
            }
        }
    }
private:
    static constexpr const char* enabledIds[count]={"fxCompOn","fxDelayOn","fxReverbOn","fxWidthOn","fxChorusOn","fxRotaryOn"};
    double sampleRate=44100.0, chorusPhase=0.0, rotaryPhase=0.0;
    std::vector<float> delayL,delayR,chorusL,chorusR,widthLine; int writePos=0,chorusWritePos=0,widthPos=0; float compEnvelope=0.0f;
    std::atomic<uint64_t> orderPacked{0x543210ULL};
    juce::Reverb reverb; juce::Reverb::Parameters reverbParams;
};
