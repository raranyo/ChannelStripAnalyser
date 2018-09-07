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
    void resized() override;
    void sliderValueChanged (Slider* sliderThatWasMoved) override;
    void buttonClicked (Button* buttonThatWasClicked) override;

private:
    
    void addUiElements();
    void createParametersAttachments();
    
    PluginDescription* getChosenType(const int menuID) const;
    void deletePlugin(int i);
    void createNewPlugin(const PluginDescription* desc, int i);
    
    void canVisualizersPaint(bool state);
    void setBuffersForVisualizers();
    void repaintVisualizers();
    void createBackgroundUi();
    
    int fftSize;

    ChannelStripAnalyserAudioProcessor& processor;
    AudioProcessorValueTreeState& parameters;
    AudioBufferManagement& mainAudioBufferSystem;
    forwardFFT forwFFT;

    ScopedPointer <SpectrumAnalyser>   spectrumAnalyser;
    ScopedPointer <SpectrumDifference> spectrumDifference;
    ScopedPointer <PhaseDifference>    phaseDifference;
    ScopedPointer <StereoAnalyser>     stereoAnalyser;
    ScopedPointer <WaveformAnalyser>   waveformAnalyser;
    
    class PluginListWindow;
    ScopedPointer <PluginListWindow> pluginListWindow;
    bool pluginWinState;
    
    ReferenceCountedArray<SimplePluginWindow> editors;
    int chosenPlugins[6];
    
    // GUI to save parameters
    Image backgroundImage;

    Slider sliderRangeVectorscope;
    Slider sliderVanishTime;
    Slider sliderRangeSpectrumAnalyser;
    Slider sliderReturnTime;
    Slider sliderCurbeOffset;
    Slider sliderDifferenceGain;
    Slider sliderRMSWindow;
    Slider sliderZoomX;
    Slider sliderZoomY;
    Slider sliderTimeAverage;
    Slider sliderRangeSpectrumDifference;
    Slider sliderLevelMeter2;
    Slider sliderLevelMeter1;
    
    ToggleButton pluginBypassButton1;
    ToggleButton pluginBypassButton2;
    ToggleButton pluginBypassButton3;
    ToggleButton pluginBypassButton4;
    ToggleButton pluginBypassButton5;
    ToggleButton pluginBypassButton6;
    ToggleButton textButtonMonoMode;
    ToggleButton textButtonLRMode;
    ToggleButton textButtonMSMode;

    
    // GUI processor attachments
    typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
    typedef AudioProcessorValueTreeState::ButtonAttachment ButtonAttachment;

    std::unique_ptr <SliderAttachment> sliderRangeVectorscopeAttach;
    std::unique_ptr <SliderAttachment> sliderVanishTimeAttach;
    std::unique_ptr <SliderAttachment> sliderRangeSpectrumAnalyserAttach;
    std::unique_ptr <SliderAttachment> sliderReturnTimeAttach;
    std::unique_ptr <SliderAttachment> sliderCurbeOffsetAttach;
    std::unique_ptr <SliderAttachment> sliderDifferenceGainAttach;
    std::unique_ptr <SliderAttachment> sliderRMSWindowAttach;
    std::unique_ptr <SliderAttachment> sliderZoomXAttach;
    std::unique_ptr <SliderAttachment> sliderZoomYAttach;
    std::unique_ptr <SliderAttachment> sliderTimeAverageAttach;
    std::unique_ptr <SliderAttachment> sliderRangeSpectrumDifferenceAttach;
    std::unique_ptr <SliderAttachment> sliderLevelMeter2Attach;
    std::unique_ptr <SliderAttachment> sliderLevelMeter1Attach;
    
    std::unique_ptr <ButtonAttachment> pluginBypassButton1Attach;
    std::unique_ptr <ButtonAttachment> pluginBypassButton2Attach;
    std::unique_ptr <ButtonAttachment> pluginBypassButton3Attach;
    std::unique_ptr <ButtonAttachment> pluginBypassButton4Attach;
    std::unique_ptr <ButtonAttachment> pluginBypassButton5Attach;
    std::unique_ptr <ButtonAttachment> pluginBypassButton6Attach;
    
    std::unique_ptr <ButtonAttachment> monoModeButtonAttach;
    std::unique_ptr <ButtonAttachment> leftRightModeButtonAttach;
    std::unique_ptr <ButtonAttachment> midSideButtonAttach;

    // GUI Elements
    //==============================================================================

    ScopedPointer<Label> curveOffsetLabelText;
    ScopedPointer<Label> differenceGainLabelText;
    ScopedPointer<Label> rmsWindowLabelText;
    ScopedPointer<Label> zoomxLabelText;
    ScopedPointer<Label> zoomyLabelText;
    ScopedPointer<Label> waveformNameTitle;
    ScopedPointer<Label> spectrumAnalyserNameTitle;
    ScopedPointer<Label> spectrumDifferenceNameTitle;
    ScopedPointer<Label> levelMeterNameTitle;
    ScopedPointer<Label> vectorscopeNameTitle;
    ScopedPointer<Label> rangeVectorscopLabelText;
    ScopedPointer<Label> vanishTimeLabelText;
    ScopedPointer<Label> rangeVectorscopeLabelText;
    ScopedPointer<Label> returnTimeLabelText;
    ScopedPointer<Label> rangeSpectrumLabelText;
    ScopedPointer<Label> timeAveargeLabelText;
    ScopedPointer<Label> monoModeLabelText;
    ScopedPointer<Label> leftRightModeLabelText;
    ScopedPointer<Label> midSideModeLabelText;

    ScopedPointer<TextEditor> textEditorTotalDelay;
    ScopedPointer<Label> totalDelayLabelText;
    //ScopedPointer<TextButton> textButtonApplyDelay;
    
    ScopedPointer<TextButton> pluginsManagerButton;
    
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
    
    ScopedPointer<Label> channelStripNameTitle;
    ScopedPointer<Label> analyserNameTitle;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStripAnalyserAudioProcessorEditor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]
