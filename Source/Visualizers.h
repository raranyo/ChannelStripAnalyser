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
    void createShadeWindow();
    
    std::vector<float> maxValuesPre;
    std::vector<float> maxValuesPost;
    std::vector<double> monoMagDataPre;
    std::vector<double> monoMagDataPost;
    
    floatMatrix conversionTable;
    Image spectrumFrame;
    Image axisImage;
    Image shadeWindow;
    AudioBuffer<float> windowTable;
    
    //dsp::FFT forwardFFT;
    AudioBufferManagement& mainAudioBufferSystem;
    forwardFFT& forwFFT;
    std::mutex m;
    // LOG INTERPOLATION OBJS
    //tk::spline spline;
    //std::vector<double> indexes;
    //std::vector<double> conversionTable;
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
    void createShadeWindow();

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
    void createShadeWindow();
    
    AudioBufferManagement& mainAudioBufferSystem;
    forwardFFT& forwFFT;
    std::mutex m;
    
    std::vector<double> linGainData1;
    std::vector<double> linGainData2;
    
    floatMatrix historyValues1;
    floatMatrix historyValues2;
    
    floatMatrix conversionTable;
    Image mainFrame;
    Image axisImage;
    Image shadeWindow;
    
    int numOfHistorySamples = 0;
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
    
    StereoAnalyser(int sR, int bS, int nS, int z, AudioBufferManagement& mainAudioBufferSystem, forwardFFT& fFFT);
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
    std::mutex m;

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
};

//==============================================================================
class WaveformAnalyser  : public Component
{
    
public:
    WaveformAnalyser(int sR, int bs, int mode, float cP, float bpm, AudioBufferManagement& mainAudioBufferSystem);
    ~ WaveformAnalyser();
    
    void paint (Graphics& g) override;
    void createFrame();
    void resized() override;
    
    std::atomic<int> blockSizeAt;
    std::atomic<int> sampleRateAt;
    std::atomic<int> modeAt;
    std::atomic<int> hostBpmAt;
    std::atomic<double> currentPositionAt;
    
    int numOfNewPixelsToPaint = 0;

private:
    void processAllFftData ();
    void createWaveform ();
    void createCurbes ();
    void createNewAxis ();
    
    AudioBufferManagement& mainAudioBufferSystem;
    std::mutex m;
    
    int oldIndexPosition;
    
    std::deque<float> monoDataBufferPre;
//    std::deque<std::vector<float>> monoDataBufferPost;
    
    std::deque<float> maxValueInPixel;
    
    std::vector<float> newMaxValueInPixel;

    int rmsWindowLength;
    
    Image mainFrame;
    Image axisImage;
    Image waveformImage;
    Image curbesImage;
    
    int counter = 0;
};


//==============================================================================



