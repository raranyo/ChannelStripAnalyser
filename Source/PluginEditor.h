#pragma once
//[Headers]     -- You can add your own extra header files here --
#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"
#include "SimplePluginWindow.h"
#include "Visualizers.h"
//[/Headers]

//==============================================================================
class ChannelStripAnalyserAudioProcessorEditor  : public AudioProcessorEditor,
                                                  public Timer,
                                                  public HighResolutionTimer,
                                                  public Slider::Listener,
                                                  public Button::Listener,
                                                  public AsyncUpdater
{
public:
    //==============================================================================
    ChannelStripAnalyserAudioProcessorEditor (ChannelStripAnalyserAudioProcessor& p, AudioProcessorValueTreeState& parameters, AudioBufferManagement& mainAudioBufferSystem);
    ~ChannelStripAnalyserAudioProcessorEditor();
    //==============================================================================
    void hiResTimerCallback() override;
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void paint (Graphics& g) override;
    void paintOverChildren (Graphics &g) override;
    void resized() override;
    void sliderValueChanged (Slider* sliderThatWasMoved) override;
    void buttonClicked (Button* buttonThatWasClicked) override;
    void fftSizeChanged ();

private:
    
    void addUiElements();
    void createParametersAttachments();
    PluginDescription* getChosenType(const int menuID) const;
    void deletePlugin(int i);
    void createNewPlugin(const PluginDescription* desc, int i);
    
    void createBackgroundUi();
    
    int fftSize;
    
    OpenGLContext myOpenGLContext;
    
    ChannelStripAnalyserAudioProcessor& processor;
    AudioProcessorValueTreeState& parameters;
    AudioBufferManagement& mainAudioBufferSystem;
    forwardFFT forwFFT;
    
    ScopedPointer <SpectrumAnalyser>   spectrumAnalyser;
    ScopedPointer <SpectrumDifference> spectrumDifference;
    ScopedPointer <PhaseDifference>    phaseDifference;
    ScopedPointer <StereoAnalyser>     stereoAnalyser;
    ScopedPointer <WaveformAnalyser>   waveformAnalyser;
    ScopedPointer <LevelMeter>         levelMeter;
    
    class PluginListWindow;
    ScopedPointer <PluginListWindow> pluginListWindow;
    bool pluginWinState;
    
    ReferenceCountedArray<SimplePluginWindow> editors;
    int chosenPlugins[6];
    bool freezeState = false;
    

    // GUI to save parameters
    Image backgroundImage;
    Image foregroundImage;
    

    // GUI Elements =================================================================
    typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
    typedef AudioProcessorValueTreeState::ButtonAttachment ButtonAttachment;
    
    // WAVEFORM
    ScopedPointer<Label> waveformNameTitle;
    ScopedPointer<Label> waveformRangeLabelText;
    ScopedPointer<Label> waveformGainLabelText;
    Slider sliderWaveformRange;
    Slider sliderWaveformGain;
    std::unique_ptr <SliderAttachment> sliderWaveformRangeAttach;
    std::unique_ptr <SliderAttachment> sliderWaveformGainAttach;

    // VECTORSCOPE
    ScopedPointer<Label> vectorscopeNameTitle;
    ScopedPointer<Label> vectorscopeRangeLabelText;
    ScopedPointer<Label> vectorscopeVanishTimeLabelText;
    Slider sliderVectorscopeRange;
    Slider sliderVectorscopeVanishTime;
    std::unique_ptr <SliderAttachment> sliderVectorscopeRangeAttach;
    std::unique_ptr <SliderAttachment> sliderVectorscopeVanishTimeAttach;
    
    // SPECTRUM ANALYSER
    ScopedPointer<Label> spectrumAnalyserNameTitle;
    ScopedPointer<Label> spectrumAnalyserRangeLabelText;
    ScopedPointer<Label> spectrumAnalyserReturnTimeLabelText;
    Slider sliderSpectrumAnalyserRange;
    Slider sliderSpectrumAnalyserReturnTime;
    std::unique_ptr <SliderAttachment> sliderSpectrumAnalyserRangeAttach;
    std::unique_ptr <SliderAttachment> sliderSpectrumAnalyserReturnTimeAttach;

    // SPECTRUM DIFFERENCE
    ScopedPointer<Label> spectrumDifferenceNameTitle;
    ScopedPointer<Label> spectrumDifferenceRangeLabelText;
    ScopedPointer<Label> spectrumDifferencetimeAverageLabelText;
    Slider sliderSpectrumDifferenceRange;
    Slider sliderSpectrumDifferenceTimeAverage;
    std::unique_ptr <SliderAttachment> sliderSpectrumDifferenceRangeAttach;
    std::unique_ptr <SliderAttachment> sliderSpectrumDifferenceTimeAverageAttach;

    // M ST M/S
    ScopedPointer<Label> monoModeLabelText;
    ScopedPointer<Label> leftRightModeLabelText;
    ScopedPointer<Label> midSideModeLabelText;
    ToggleButton textButtonMonoMode;
    ToggleButton textButtonLRMode;
    ToggleButton textButtonMSMode;
    std::unique_ptr <ButtonAttachment> monoModeButtonAttach;
    std::unique_ptr <ButtonAttachment> leftRightModeButtonAttach;
    std::unique_ptr <ButtonAttachment> midSideButtonAttach;

    // FFT
    ScopedPointer<Label> fftSizeLabelText;
    ScopedPointer<ComboBox> fftSizeButton;
    
    // DELAY
    ScopedPointer<Label> totalDelayLabelText;
    ScopedPointer<TextButton> compensateDelay;
    ScopedPointer<TextEditor> textEditorTotalDelay;

    // FREEZE
    ScopedPointer<TextButton> freezeButton;

    // PLUGIN SLOTS
    ToggleButton pluginBypassButton1;
    ToggleButton pluginBypassButton2;
    ToggleButton pluginBypassButton3;
    ToggleButton pluginBypassButton4;
    ToggleButton pluginBypassButton5;
    ToggleButton pluginBypassButton6;
    
    ScopedPointer<TextButton> pluginLoadButton1;
    ScopedPointer<TextEditor> pluginInfoButton1;
    ScopedPointer<TextButton> pluginOpenButton1;
    ScopedPointer<TextButton> pluginLoadButton2;
    ScopedPointer<TextEditor> pluginInfoButton2;
    ScopedPointer<TextButton> pluginOpenButton2;
    ScopedPointer<TextButton> pluginLoadButton3;
    ScopedPointer<TextEditor> pluginInfoButton3;
    ScopedPointer<TextButton> pluginOpenButton3;
    ScopedPointer<TextButton> pluginLoadButton4;
    ScopedPointer<TextEditor> pluginInfoButton4;
    ScopedPointer<TextButton> pluginOpenButton4;
    ScopedPointer<TextButton> pluginLoadButton5;
    ScopedPointer<TextEditor> pluginInfoButton5;
    ScopedPointer<TextButton> pluginOpenButton5;
    ScopedPointer<TextButton> pluginLoadButton6;
    ScopedPointer<TextEditor> pluginInfoButton6;
    ScopedPointer<TextButton> pluginOpenButton6;
    
    std::unique_ptr <ButtonAttachment> pluginBypassButton1Attach;
    std::unique_ptr <ButtonAttachment> pluginBypassButton2Attach;
    std::unique_ptr <ButtonAttachment> pluginBypassButton3Attach;
    std::unique_ptr <ButtonAttachment> pluginBypassButton4Attach;
    std::unique_ptr <ButtonAttachment> pluginBypassButton5Attach;
    std::unique_ptr <ButtonAttachment> pluginBypassButton6Attach;
    
    // OTHERS
    ScopedPointer<TextButton> pluginsManagerButton;
    ScopedPointer<Label> channelStripNameTitle;
    ScopedPointer<Label> analyserNameTitle;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStripAnalyserAudioProcessorEditor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]
