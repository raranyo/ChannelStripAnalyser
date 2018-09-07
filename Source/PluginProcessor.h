/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include <deque>
#include <numeric>

// DEFINING PARAMETERS

#define SLIDER_RANGE_VECTORSCOPE_ID "sliderRangeVectorscope"
#define SLIDER_RANGE_VECTORSCOPE_NAME "SliderRangeVectorscope"
#define SLIDER_VANISH_TIME_ID "sliderVanishTime"
#define SLIDER_VANISH_TIME_NAME "SliderVanishTime"
#define SLIDER_RANGE_SPECTRUM_ANALYSER_ID "sliderRangeSpectrumAnalyser"
#define SLIDER_RANGE_SPECTRUM_ANALYSER_NAME "SliderRangeSpectrumAnalyser"
#define SLIDER_RETURN_TIME_ID "sliderReturnTime"
#define SLIDER_RETURN_TIME_NAME "SliderReturnTime"
#define SLIDER_CURBE_OFFSET_ID "sliderCurbeOffset"
#define SLIDER_CURBE_OFFSET_NAME "SliderCurbeOffset"
#define SLIDER_DIFFERENCE_GAIN_ID "sliderDifferenceGain"
#define SLIDER_DIFFERENCE_GAIN_NAME "SliderDifferenceGain"
#define SLIDER_RMS_WINDOW_ID "sliderRMSWindow"
#define SLIDER_RMS_WINDOW_NAME "SliderRMSWindow"
#define SLIDER_ZOOM_X_ID "sliderZommX"
#define SLIDER_ZOOM_X_NAME "SliderZommX"
#define SLIDER_ZOOM_Y_ID "sliderZommY"
#define SLIDER_ZOOM_Y_NAME "SliderZommY"
#define SLIDER_TIME_AVERAGE_ID "sliderTimeAverage"
#define SLIDER_TIME_AVERAGE_NAME "SliderTimeAverage"
#define SLIDER_RANGE_SPECTRUM_DIFFERENCE_ID "sliderRangeSpectrumDifference"
#define SLIDER_RANGE_SPECTRUM_DIFFERENCE_NAME "SliderRangeSpectrumDifference"
#define SLIDER_LEVEL_METER_1_ID "sliderLevelMeter1"
#define SLIDER_LEVEL_METER_1_NAME "SliderLevelMeter1"
#define SLIDER_LEVEL_METER_2_ID "sliderLevelMeter2"
#define SLIDER_LEVEL_METER_2_NAME "SliderLevelMeter2"

#define BUTTON_PLUGINBYPASS_1_ID "buttonPluginBypass1"
#define BUTTON_PLUGINBYPASS_1_NAME "ButtonPluginBypass1"
#define BUTTON_PLUGINBYPASS_2_ID "buttonPluginBypass2"
#define BUTTON_PLUGINBYPASS_2_NAME "ButtonPluginBypass2"
#define BUTTON_PLUGINBYPASS_3_ID "buttonPluginBypass3"
#define BUTTON_PLUGINBYPASS_3_NAME "ButtonPluginBypass3"
#define BUTTON_PLUGINBYPASS_4_ID "buttonPluginBypass4"
#define BUTTON_PLUGINBYPASS_4_NAME "ButtonPluginBypass4"
#define BUTTON_PLUGINBYPASS_5_ID "buttonPluginBypass5"
#define BUTTON_PLUGINBYPASS_5_NAME "ButtonPluginBypass5"
#define BUTTON_PLUGINBYPASS_6_ID "buttonPluginBypass6"
#define BUTTON_PLUGINBYPASS_6_NAME "ButtonPluginBypass6"

#define PLUGIN_SIZE_1_ID "pluginSize1"
#define PLUGIN_SIZE_1_NAME "pluginSize2"

#define SLIDER5_ID "sliderTEST"
#define SLIDER5_NAME "SliderTEST"
//==============================================================================
class AudioBufferManagement
{
private:
    std::mutex threadMutex;
public:
    struct audioBufferManagementType
    {
        AudioBuffer<float> historyBuffer;
        AbstractFifo abstractFifoHistory;
        AudioBuffer<float> audioBuffer;
        AbstractFifo abstractFifoAudio;
        
        std::atomic<int> lastSampleIndexAudioBuffer;
        std::atomic<int> lastSampleIndexHistoryBuffer;
        std::atomic<int> processorDelay;
        
        String name;
        std::mutex& threadMutex;
        
        audioBufferManagementType (int ch, int sAb, int sHb, String n, std::mutex& tM)
        : historyBuffer (ch, sHb), abstractFifoHistory (sHb),
        audioBuffer (ch, sAb), abstractFifoAudio (sAb),
        lastSampleIndexAudioBuffer(0),
        lastSampleIndexHistoryBuffer(0),
        processorDelay(0),
        name(n),
        threadMutex(tM)
        {}
        void reset (int ch, int sAb, int sHb);
        void writeBufferIntoAudioBuffer   ( AudioBuffer<float> &fromBuffer, int numOfSamples );
        void copySamplesFromHistoryBuffer ( AudioBuffer<float> &toBuffer, int numOfSamples );
        int getLastIndexPositionInHistoryBuffer();
    };
    
    std::atomic<int> visualizersSemaphore;
    audioBufferManagementType bufferPre;
    audioBufferManagementType bufferPost;

    AudioBufferManagement (int channels, int sizeAudioBuffer, int sizeHistoryBuffer );
    ~AudioBufferManagement(){};
    void pushAudioBufferIntoHistoryBuffer();
    void reset(int newChannel, int newSizeAudioBuffer, int newSizeHistoryBuffer);
};

//==============================================================================
class ReleasePool : private Timer
{
public:
    
    ReleasePool() { startTimer(1000); }
    
    template<typename T> void add (const std::shared_ptr<T>& object)
    {
        //if (object.empty())
        //    return;
        std::lock_guard<std::mutex> lock (m);
        pool.emplace_back(object);
    }
private:
    void timerCallback() override
    {
        std::lock_guard<std::mutex> lock (m);
        pool.erase(
                   std::remove_if (
                                   pool.begin(), pool.end(),
                                   [] (auto& object) {return object.use_count() <= 1; }),
                   pool.end());
    }
    std::vector<std::shared_ptr<void>> pool;
    std::mutex m;
};

//==============================================================================
struct forwardFFT
{
    dsp::FFT forwFFT;
    AudioBuffer<float> windowTable;
    int fftSize;
    
    forwardFFT (int fS, int nC)
    : forwFFT (std::log2(fS)),
    windowTable ( nC, fS * 2),
    fftSize(fS)
    {
        createWindowTable();
    }
    void createWindowTable();
    void performFFT( AudioBuffer<float>& audioBuffer);
};


//==============================================================================
class ChannelStripAnalyserAudioProcessor  : public AudioProcessor
{
public:
    //==============================================================================
    ChannelStripAnalyserAudioProcessor();
    ~ChannelStripAnalyserAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    //==============================================================================
    void AudioPluginChannelConfiguration();
    void createPluginProcessor (const PluginDescription* desc, int i);
    void deletePluginProcessor (int i);
    AudioBuffer<float>& getAudioBuffers (int bufferPreOrPost);
    
    void createParameters();
    void createWindowTable();

    // Objects for owned windows ===================================================
    AudioPluginFormatManager formatManager;
    KnownPluginList knownPluginList;
    ApplicationProperties applicationProperties;
    
    // Objects for owned Plugins ===================================================
    bool plugIns[6];
    bool bypassFlag[6];
    bool deletedPlugins[6];
    bool createPluginCue[6];
    bool shownPlugins[6];
    bool recalledPlugins[6];
    
    std::atomic <bool> audioThreadIsProcessing;
    std::atomic <bool> messageThreadIsCopying;
    
    std::atomic <bool> readedTheSameBlock;
    std::atomic <bool> copyiedTheSameBlock;
    std::atomic <bool> isAccessingTheBuffer;

    int fftSize = 2048;

    AudioProcessorGraph graph;
    AudioPlayHead::CurrentPositionInfo currentPosition;
    
    std::atomic<int> asampleRate;
    std::atomic<int> ablockSize;

private:
    
    int mainAudioBufferSize = 2048 * 8;
    int graphLatencySamples = 0;
    
    AudioProcessorValueTreeState parameters;
    
    AudioBuffer<float> auxBuffer;
    AudioBuffer<float> windowTable;
    
    std::mutex audioThreadMutex;
    
    AudioBufferManagement mainAudioBufferSystem;
    

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStripAnalyserAudioProcessor)
};
















