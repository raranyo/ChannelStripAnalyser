#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include "spline.h"
#include "PluginProcessor.h"
#include <deque>
#include <numeric>

//==============================================================================
class SpectrumAnalyser  : public Component
{
    
public:
    typedef std::vector<std::vector<float>> floatMatrix;
    typedef AudioBufferManagement::audioBufferManagementType audioBufferManagementType;

    SpectrumAnalyser(int sampleRate, int fftSize, int numOfChannels, AudioBufferManagement& mainAudioBufferSystem, forwardFFT& fFFT);
    ~SpectrumAnalyser();
    
    void paint (Graphics& g) override;
    void createFrame();
    void resized() override;

    std::atomic<bool> axisNeedsUpdate;
    std::atomic<int> scaleModeAt;
    std::atomic<int> fftSizeAt;
    std::atomic<int> sampleRateAt;
    std::atomic<double> decayRatioAt;
    
private:
    void processAllFftData (audioBufferManagementType& buffer, std::vector<double>& magnitudeDBmono, std::vector<float>& maxValues);
    void peakHolder (int i, float& magLin, std::vector<float>& maxValues, double &decayRatio);
    void createPath (std::vector<double>& magnitudeDBmono, Path &drawPath);
    void createNewAxis ();
    
    std::vector<float> maxValuesPre;
    std::vector<float> maxValuesPost;
    std::vector<double> monoMagDataPre;
    std::vector<double> monoMagDataPost;
    
    floatMatrix conversionTable;
    Image spectrumFrame;
    Image axisImage;
    Image shadeWindow;
    
    AudioBufferManagement& mainAudioBufferSystem;
    forwardFFT& forwFFT;
    std::mutex m;
};

//==============================================================================
class SpectrumDifference  : public Component
{
public:
    typedef std::vector<std::vector<float>> floatMatrix;
    typedef AudioBufferManagement::audioBufferManagementType audioBufferManagementType;
    
    SpectrumDifference(int sampleRate, int fftSize, int nomOfChannels, AudioBufferManagement& mainAudioBufferSystem, forwardFFT& fFFT);
    ~SpectrumDifference();
    
    void paint (Graphics& g) override;
    void createFrame();
    void resized() override;

    std::atomic<bool> axisNeedsUpdate;
    std::atomic<int> analyseMode;
    std::atomic<int> scaleModeAt;
    std::atomic<int> fftSizeAt;
    std::atomic<int> sampleRateAt;
    std::atomic<int> numOfHistorySamplesAt;

private:
    void processAllFftData (audioBufferManagementType& binsStereoPre, audioBufferManagementType& binsStereoPost, std::vector<double>& linGain1, std::vector<double>& linGain2 );
    void peakHolder (int i, float& magLin, floatMatrix &historySamples);
    void createPath (std::vector<double>& magnitudeDBmono, Path &drawPath);
    void createNewAxis ();

    AudioBufferManagement& mainAudioBufferSystem;
    forwardFFT& forwFFT;
    std::mutex m;
    
    int numOfHistorySamples = 0;
    
    std::vector<double> linGainData1;
    std::vector<double> linGainData2;
    floatMatrix historyValues1;
    floatMatrix historyValues2;

    floatMatrix conversionTable;
    Image mainFrame;
    Image axisImage;
    Image shadeWindow;
};

//==============================================================================
class PhaseDifference  : public Component
{
    
public:
    typedef std::vector<std::vector<float>> floatMatrix;
    typedef AudioBufferManagement::audioBufferManagementType audioBufferManagementType;
    
    PhaseDifference (int sampleRate, int fftSize, int nomOfChannels, AudioBufferManagement& mainAudioBufferSystem, forwardFFT& fFFT);
    ~PhaseDifference();
    
    void paint (Graphics& g) override;
    void createFrame();
    void resized() override;
    
    std::atomic<int> analyseMode;    
    std::atomic<int> fftSizeAt;
    std::atomic<int> sampleRateAt;
    std::atomic<int> numOfHistorySamplesAt;
    
    
private:
    void processAllFftData (audioBufferManagementType& binsStereoPre, audioBufferManagementType& binsStereoPost, std::vector<double>& linGain1, std::vector<double>& linGain2 );
    
    void peakHolder (int i, float& magLin, floatMatrix &historySamples);
    void createPath (std::vector<double>& magnitudeDBmono, Path &drawPath);
    void createNewAxis ();
    
    AudioBufferManagement& mainAudioBufferSystem;
    forwardFFT& forwFFT;
    std::mutex m;
    
    std::vector<double> linGainData1;
    std::vector<double> linGainData2;
    
    floatMatrix historyValues1;
    floatMatrix historyValues2;
    int numOfHistorySamples = 0;
    
    floatMatrix conversionTable;
    Image mainFrame;
    Image axisImage;
    Image shadeWindow;
};

//==============================================================================
class StereoAnalyser  : public Component
{
    
public:
    typedef std::pair<float, float> pairOfXYpoints;
    typedef std::deque <std::vector <pairOfXYpoints>> pairFloatMatrix;
    typedef AudioBufferManagement::audioBufferManagementType audioBufferManagementType;

    struct setOfCorrPoints
    {
        float corrPre;
        float corrPost;
        float corrRest;
    };
    
    StereoAnalyser(int sR, int bS, AudioBufferManagement& mainAudioBufferSystem, forwardFFT& fFFT);
    ~StereoAnalyser();
    
    void paint (Graphics& g) override;
    void createFrame();
    void resized() override;
    
    std::atomic<int> blockSizeAt;
    std::atomic<int> sampleRateAt;
    std::atomic<int> numOfHistorySamplesAt;
    std::atomic<float> zoomAt;

    
private:
    void processAllFftData();
    void createCurbes ();
    void createNewAxis ();
    void createCorrelationBackground();

    AudioBufferManagement& mainAudioBufferSystem;
    forwardFFT& forwFFT;

    pairFloatMatrix setOfPointsHistDataPre;
    pairFloatMatrix setOfPointsHistDataPost;
    setOfCorrPoints setOfCorrelationHistData;
    
    int corrRMSWindowLength;
    float meanValuePre;
    float meanValuePost;
    
    std::pair<float, float> energyPre;
    std::pair<float, float> energyPost;
    
    Image mainFrame;
    Image axisImage;
    Image lissajouCurbe;
    Image correlationBackground;
    Image correlationLines;
    Image shadeWindow;
};

//==============================================================================
class WaveformAnalyser  : public Component
{
public:
    typedef std::pair<float, float> pairOfFloats;
    
    WaveformAnalyser(int sR, int bs, AudioBufferManagement& mainAudioBufferSystem);
    ~ WaveformAnalyser();
    
    void paint (Graphics& g) override;
    void resized() override;
    
    std::atomic<int> blockSizeAt;
    std::atomic<int> sampleRateAt;
    std::atomic<int> modeAt;
    std::atomic<int> rangeAt;
    std::atomic<float> gainAt;
    
    std::atomic<bool> axisNeedsUpdate;

private:
    void createFrame();
    void processData ();
    void createNewAxis ();
    
    AudioBufferManagement& mainAudioBufferSystem;
    
    int rmsWindowLength;
    
    std::vector<Range<float>> historyBufferMinMaxPre;
    std::vector<Range<float>> historyBufferMinMaxPost;

    std::vector<float> historyBufferPost;
    std::vector<float> historyBufferPre;
    std::vector<float> rmsGainBuffer;

    Path rmsGainPath;
    Image axisImage;
    Image waveformImage;
};

//==============================================================================
class LevelMeter  : public Component
{
public:
    
    struct SetOfValuesToPaint
    {
        std::pair<float, float> rmsLevelPost;
        std::pair<float, float> peakLevelPost;
        std::pair<float, float> oldPeakLevelPost;
        std::pair<float, float> rmsLevelDifference;
        std::pair<float, float> peakLevelDifference;
        std::pair<float, float> oldPeakLevelDifference;

    };
    
    LevelMeter(int sR, int bs, AudioBufferManagement& mainAudioBufferSystem);
    ~LevelMeter();
    
    void paint (Graphics& g) override;
    void resized() override;
    
    std::atomic<int> blockSizeAt;
    std::atomic<int> sampleRateAt;
    
private:
    void createFrame();
    void processData();
    void createNewAxis();
    void peakHolder();
    
    AudioBufferManagement& mainAudioBufferSystem;
    
    int rmsWindowLength;
    SetOfValuesToPaint setOfValuesToPaint;
    
    Path rmsGainPath;
    Image axisImage;
    Image waveformImage;
};


