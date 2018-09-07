/*
  ==============================================================================

    Engine.h
    Created: 11 Aug 2018 3:42:14pm
    Author:  Raimon Arano 

  ==============================================================================
*/

#pragma once
#include "../JuceLibraryCode/JuceHeader.h"

class AudioBufferManagement: public HighResolutionTimer
{
private:
    
    AudioBuffer<float> audioBufferPre;
    AudioBuffer<float> audioBufferPost;
    
    AbstractFifo abstractFifoAudioBufferPre;
    AbstractFifo abstractFifoAudioBufferPost;
    
    AbstractFifo abstractFifoHistoryBufferPost;
    AbstractFifo abstractFifoHistoryBufferPre;
    
    std::atomic<int> lastWrittenIndexHistoryBuffer;
    std::atomic<int> lastWrittenSizeHistoryBuffer;
    
    int channels;
    
    std::mutex& m;
    ChannelStripAnalyserAudioProcessor& processor;
    
public:
    
    AudioBuffer<float> historyBufferPost;
    AudioBuffer<float> historyBufferPre;
    
    AudioBufferManagement (int channels, int sizeAudioBuffer, int sizeHistoryBuffer, std::mutex& mutex, ChannelStripAnalyserAudioProcessor& p )
    :
    abstractFifoHistoryBufferPre  (sizeHistoryBuffer),
    abstractFifoHistoryBufferPost (sizeHistoryBuffer),
    abstractFifoAudioBufferPre    (sizeAudioBuffer),
    abstractFifoAudioBufferPost   (sizeAudioBuffer),
    historyBufferPre  (channels, sizeHistoryBuffer),
    historyBufferPost (channels, sizeHistoryBuffer),
    audioBufferPre    (channels, sizeAudioBuffer),
    audioBufferPost   (channels, sizeAudioBuffer),
    m (mutex),
    processor(p)
    {}
    ~AudioBufferManagement(){};
    void writeBufferIntoAudioBuffer ( AudioBuffer<float> &fromBuffer, int numOfSamples, bool bufferPrePost  );
    void hiResTimerCallback();
    void reset(int newChannel, int newSizeAudioBuffer, int newSizeHistoryBuffer);
    void writeBufferIntoLifo ( AudioBuffer<float> &fromBuffer, int numOfSamples);
    void discardSamples (int numOfSamples );
    void readSamplesFromLifo (AudioBuffer<float> &toBuffer
                              //std::atomic<std::shared_ptr<AudioBuffer<float>>> toBuffer,
                              , int numOfSamples, int delay);
    int getNumOfReadySamples();
    void resizeBuffer(int newSize);
    
};
