#include "PluginEditor.h"
#include <stdexcept>

namespace {
class MobileContent final : public juce::Component, private juce::Timer {
public:
    MobileContent() : keyboard(processor.keyboardState,juce::MidiKeyboardComponent::horizontalKeyboard) {
        restore();
        editor.reset(processor.createEditor());addAndMakeVisible(*editor);
        title.setText("BASS AMBIENT  "+juce::String(JucePlugin_VersionString),juce::dontSendNotification);
        title.setColour(juce::Label::textColourId,juce::Colour(0xffeab35d));addAndMakeVisible(title);
        keyboardButton.onClick=[this]{showKeyboard=!showKeyboard;if(!showKeyboard)processor.keyboardState.allNotesOff(0);resized();};
        audioButton.onClick=[this]{showAudioSettings();};
        down.onClick=[this]{shiftKeyboard(-12);};up.onClick=[this]{shiftKeyboard(12);};
        for(auto* b:{&keyboardButton,&audioButton,&down,&up})addAndMakeVisible(*b);
        keyboard.setAvailableRange(24,96);keyboard.setLowestVisibleKey(36);keyboard.setKeyWidth(34);
        addAndMakeVisible(keyboard);
        const auto error=devices.initialise(0,2,nullptr,true);
        if(error.isNotEmpty())title.setText("Audio: "+error,juce::dontSendNotification);
        else {
            auto setup=devices.getAudioDeviceSetup();setup.bufferSize=256;
            // Request the existing DSP's normal rate; retain the device's valid setup if unavailable.
            setup.sampleRate=48000;devices.setAudioDeviceSetup(setup,true);
        }
        player.setProcessor(&processor);devices.addAudioCallback(&player);
        devices.addMidiInputDeviceCallback({},&player);updateMidi();
        setSize(844,390);startTimer(1000);
#if BASS_AMBIENT_SMOKE_TEST
        if(juce::SystemStats::getEnvironmentVariable("BASS_AMBIENT_SMOKE",{}).isNotEmpty())
            juce::Timer::callAfterDelay(1500,[safe=juce::Component::SafePointer<MobileContent>(this)]{if(safe)safe->smokeTest();});
#endif
    }
    ~MobileContent() override {
        stopTimer();save();devices.removeMidiInputDeviceCallback({},&player);
        devices.removeAudioCallback(&player);player.setProcessor(nullptr);devices.closeAudioDevice();
        editor.reset();
    }
    void pause() {processor.panicNow();save();devices.closeAudioDevice();}
    void resume() {devices.restartLastAudioDevice();updateMidi();}
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff10141b));}
    void resized() override {
        auto area=getLocalBounds();
#if JUCE_IOS
        if(auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds()))
            area=display->safeAreaInsets.subtractedFrom(area);
#endif
        auto top=area.removeFromTop(44);audioButton.setBounds(top.removeFromRight(94).reduced(3,0));
        keyboardButton.setBounds(top.removeFromRight(114).reduced(3,0));title.setBounds(top);
        keyboardButton.setToggleState(showKeyboard,juce::dontSendNotification);
        keyboard.setVisible(showKeyboard);down.setVisible(showKeyboard);up.setVisible(showKeyboard);
        if(showKeyboard) {
            auto keys=area.removeFromBottom(76);down.setBounds(keys.removeFromLeft(44));up.setBounds(keys.removeFromRight(44));keyboard.setBounds(keys);
        }
        editor->setBounds(area);
    }
private:
    BassAmbientProcessor processor;
    juce::AudioDeviceManager devices;
    juce::AudioProcessorPlayer player;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::MidiKeyboardComponent keyboard;
    juce::Label title;
    juce::TextButton keyboardButton{"KEYBOARD"},audioButton{"AUDIO"},down{"- OCT"},up{"+ OCT"};
    bool showKeyboard=false;int lowest=36,ticks=0;
    juce::MemoryBlock lastSaved;
    juce::StringArray midiDevices;
    juce::File stateFile() const {
        auto dir=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("BassAmbient");
        dir.createDirectory();return dir.getChildFile("session.bin");
    }
    void restore() {juce::MemoryBlock state;if(stateFile().loadFileAsData(state)&&state.getSize()>0)processor.setStateInformation(state.getData(),int(state.getSize()));}
    void save() {
        juce::MemoryBlock state;processor.getStateInformation(state);if(state==lastSaved)return;
        juce::TemporaryFile temp(stateFile());
        if(temp.getFile().replaceWithData(state.getData(),state.getSize())&&temp.overwriteTargetFileWithTemporary())lastSaved=state;
    }
    void shiftKeyboard(int step) {processor.keyboardState.allNotesOff(0);lowest=juce::jlimit(24,72,lowest+step);keyboard.setLowestVisibleKey(lowest);}
    void updateMidi() {
        juce::StringArray now;for(const auto& input:juce::MidiInput::getAvailableDevices()) {
            now.add(input.identifier);if(!midiDevices.contains(input.identifier))devices.setMidiInputDeviceEnabled(input.identifier,true);
        }
        if(now!=midiDevices)processor.panicNow();midiDevices=now;
    }
    void timerCallback() {updateMidi();if(++ticks%5==0)save();}
    void showAudioSettings() {
        auto* selector=new juce::AudioDeviceSelectorComponent(devices,0,0,2,2,true,false,true,false);
        selector->setSize(std::min(680,getWidth()-40),std::min(300,getHeight()-40));
        juce::DialogWindow::LaunchOptions options;options.content.setOwned(selector);
        options.dialogTitle="Audio / MIDI";options.dialogBackgroundColour=juce::Colour(0xff1c2430);
        options.escapeKeyTriggersCloseButton=true;options.useNativeTitleBar=false;options.resizable=false;
        options.componentToCentreAround=this;options.launchAsync();
    }
#if BASS_AMBIENT_SMOKE_TEST
    void smokeTest() {
        const auto documents=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        juce::String result="PASS";
        try {
            auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
            player.setProcessor(nullptr);processor.prepareToPlay(48000,128);processor.applyPreset(6);
            juce::AudioBuffer<float> audio(2,128);double energy=0;
            processor.keyboardState.noteOn(1,36,.9f);
            for(int block=0;block<1000;++block) {juce::MidiBuffer midi;processor.processBlock(audio,midi);energy+=audio.getMagnitude(0,128);}
            check(energy>.1,"onscreen MIDI produces audio");
            juce::String error;check(processor.exportCapture(documents.getChildFile("smoke.wav"),false,error),"WAV export");
            check(processor.exportCapture(documents.getChildFile("smoke.mid"),true,error),"MIDI export");
            processor.saveScene(0);save();auto expected=processor.readParameters();processor.applyPreset(0);restore();
            check(processor.readParameters().source==expected.source&&processor.hasScene(0),"session and scene persistence");
            processor.panicNow();{juce::MidiBuffer midi;processor.processBlock(audio,midi);}
            check(audio.getMagnitude(0,128)==0,"panic stops audio");
            std::function<juce::Component*(juce::Component&,const juce::String&)> find=[&](auto& root,const auto& id)->juce::Component* {
                if(root.getComponentID()==id)return &root;
                for(auto* child:root.getChildren())if(auto* found=find(*child,id))return found;
                return nullptr;
            };
            auto selectMenu=[&](const char* id,int count) {
                auto* control=find(*editor,id);check(control!=nullptr,"mobile selector exists");
                juce::ComboBox* combo=nullptr;
                for(auto* child:control->getChildren())if(auto* c=dynamic_cast<juce::ComboBox*>(child))combo=c;
                check(combo&&combo->getNumItems()==count,"all mobile choices present");
                for(int i=1;i<=count;++i){combo->setSelectedId(i,juce::sendNotificationSync);check(combo->getSelectedId()==i,"mobile selection sync");}
            };
            auto snapshot=[&](const char* name){auto stream=documents.getChildFile(name).createOutputStream();juce::PNGImageFormat format;check(stream&&format.writeImageToStream(createComponentSnapshot(getLocalBounds()),*stream),"screenshot");};
            selectMenu("source",4);snapshot("mobile-bass.png");
            std::function<void(juce::Component&,const juce::String&)> click=[&](auto& root,const auto& label) {
                for(auto* child:root.getChildren()) {
                    if(auto* b=dynamic_cast<juce::TextButton*>(child))if(b->getButtonText()==label&&b->isShowing()&&b->onClick){b->onClick();return;}
                    click(*child,label);
                }
            };
            click(*editor,"PHRASE");selectMenu("styleFamily",8);snapshot("mobile-styles.png");
            click(*editor,"AMBIENT");snapshot("mobile-ambient.png");click(*editor,"FX");snapshot("mobile-fx.png");
            click(*editor,"BASS");click(*editor,"SOUND");showKeyboard=true;resized();snapshot("mobile-keyboard.png");
            processor.applyPreset(6);player.setProcessor(&processor);
        } catch(const std::exception& error) {result="FAIL: "+juce::String(error.what());}
        documents.getChildFile("smoke-result.txt").replaceWithText(result);
    }
#endif
};
class MobileWindow final : public juce::DocumentWindow {
public:
    MobileWindow():DocumentWindow("Bass Ambient",juce::Colour(0xff10141b),DocumentWindow::closeButton) {
        setUsingNativeTitleBar(false);setTitleBarHeight(0);setContentOwned(new MobileContent(),true);
#if JUCE_IOS
        setFullScreen(true);
#else
        setResizable(true,false);centreWithSize(844,390);
#endif
        setVisible(true);
    }
    void closeButtonPressed() override {juce::JUCEApplication::getInstance()->systemRequestedQuit();}
    MobileContent* content() {return static_cast<MobileContent*>(getContentComponent());}
};
class BassAmbientApp final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override {return "Bass Ambient";}
    const juce::String getApplicationVersion() override {return JucePlugin_VersionString;}
    void initialise(const juce::String&) override {
        juce::Desktop::getInstance().setOrientationsEnabled(juce::Desktop::rotatedClockwise|juce::Desktop::rotatedAntiClockwise);
        window=std::make_unique<MobileWindow>();
    }
    void shutdown() override {window.reset();}
    void suspended() override {if(window)window->content()->pause();}
    void resumed() override {if(window)window->content()->resume();}
private:
    std::unique_ptr<MobileWindow> window;
};
}
START_JUCE_APPLICATION(BassAmbientApp)
