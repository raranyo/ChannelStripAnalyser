#include "PluginEditor.h"

//[MiscUserDefs] You can add your own user definitions and misc code here...
//==============================================================================
class ChannelStripAnalyserAudioProcessorEditor::PluginListWindow   : public DocumentWindow
{
public:
    PluginListWindow(ChannelStripAnalyserAudioProcessorEditor& owner)
    : DocumentWindow("Available Plugins", Colour (0xff111111)
                     ,DocumentWindow::closeButton
                     ),
    owner(owner)
    {
        setResizable(false, false);
        
        std::unique_ptr <File> deadMansPedalFile(new File(owner.processor.applicationProperties.
                                                                          getUserSettings()->
                                                                          getFile().getSiblingFile("RecentlyCrashedPluginsList")));
        
        
        setTitleBarButtonsRequired(4, true);
        
        savedPluginList = owner.processor.applicationProperties.getUserSettings()->getXmlValue("pluginList");
        
        if (savedPluginList != nullptr){
            owner.processor.knownPluginList.recreateFromXml(*savedPluginList);
        }
        
        
        setContentOwned (new PluginListComponent(owner.processor.formatManager,
                                                 owner.processor.knownPluginList,
                                                 *deadMansPedalFile,
                                                 owner.processor.applicationProperties.getUserSettings()), true);
        
        auto x = owner.processor.applicationProperties.getUserSettings()->getIntValue("lastPositionX", 100);
        auto y = owner.processor.applicationProperties.getUserSettings()->getIntValue("lastPositionY", 100);
        
        setTopLeftPosition(x, y);
    }
    
    ~PluginListWindow()
    {
        owner.processor.applicationProperties.getUserSettings()->setValue("listWindowPos", getWindowStateAsString());
        clearContentComponent();
        
    }

    void open()
    {
        savedPluginList = owner.processor.knownPluginList.createXml();
        owner.processor.applicationProperties.getUserSettings()->setValue("pluginList", savedPluginList);
        owner.processor.applicationProperties.saveIfNeeded();
        
        
        auto x = owner.processor.applicationProperties.getUserSettings()->getIntValue("lastPositionX", 100);
        auto y = owner.processor.applicationProperties.getUserSettings()->getIntValue("lastPositionY", 100);
        
        setVisible(true);
        toFront(true);
        setTopLeftPosition(x, y);
        
    }
    
    void closeButtonPressed()
    {
        LastPositionPoint = this->getPosition();
        owner.processor.applicationProperties.getUserSettings()->setValue("lastPositionX", LastPositionPoint.x);
        owner.processor.applicationProperties.getUserSettings()->setValue("lastPositionY", LastPositionPoint.y);
        
        savedPluginList = owner.processor.knownPluginList.createXml();
        owner.processor.applicationProperties.getUserSettings()->setValue("pluginList", savedPluginList);
        owner.processor.applicationProperties.saveIfNeeded();
        
        setVisible(false);
        owner.pluginWinState = false;
        
    }
    
private:
    
    ChannelStripAnalyserAudioProcessorEditor& owner;
    ScopedPointer<XmlElement> savedPluginList;
    ScopedPointer<XmlElement> LastPosition;
    Point<int> LastPositionPoint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginListWindow)
};
//[/MiscUserDefs]
//==============================================================================
ChannelStripAnalyserAudioProcessorEditor::ChannelStripAnalyserAudioProcessorEditor
(ChannelStripAnalyserAudioProcessor& p, AudioProcessorValueTreeState& params, AudioBufferManagement& bufSys)
    :
        AudioProcessorEditor (&p),
        fftSize(2048),
        processor (p),
        parameters (params),
        mainAudioBufferSystem (bufSys),
        forwFFT (fftSize, processor. getTotalNumInputChannels())
{
    myOpenGLContext.attachTo(*this);
    addUiElements();
    setOpaque (true);
    setSize (1280, 791);
    createBackgroundUi();
    
    const int numOfChannels = processor.getTotalNumInputChannels();
    const int sampleRate = processor.getSampleRate();
    
    addAndMakeVisible
        (spectrumAnalyser   = new SpectrumAnalyser   (sampleRate, fftSize, numOfChannels, mainAudioBufferSystem, forwFFT));
    addAndMakeVisible
        (spectrumDifference = new SpectrumDifference (sampleRate, fftSize, numOfChannels, mainAudioBufferSystem, forwFFT));
    addAndMakeVisible
        ( phaseDifference   = new PhaseDifference    (sampleRate, fftSize, numOfChannels, mainAudioBufferSystem, forwFFT));
    addAndMakeVisible
        (stereoAnalyser     = new StereoAnalyser     (sampleRate, fftSize, mainAudioBufferSystem, forwFFT));
    addAndMakeVisible
        (waveformAnalyser   = new WaveformAnalyser   (sampleRate, fftSize, mainAudioBufferSystem));
    addAndMakeVisible
        (levelMeter         = new LevelMeter         (sampleRate, fftSize, mainAudioBufferSystem));
    
    createParametersAttachments();
    
    pluginWinState   = false;
    pluginListWindow = new PluginListWindow (*this);
    
    editors.ensureStorageAllocated(6);
    for (auto i=0; i<6; i++)
    {
        editors.add(nullptr);
    }
    
    triggerAsyncUpdate();
    Timer::startTimer(100);
}

ChannelStripAnalyserAudioProcessorEditor::~ChannelStripAnalyserAudioProcessorEditor()
{
    Timer::stopTimer();
    HighResolutionTimer::stopTimer();
}

void ChannelStripAnalyserAudioProcessorEditor::hiResTimerCallback()
{
    mainAudioBufferSystem.pushAudioBufferIntoHistoryBuffer();
    
    if (isUpdatePending()) cancelPendingUpdate();
    triggerAsyncUpdate();
}

void ChannelStripAnalyserAudioProcessorEditor::handleAsyncUpdate()
{
    if ( ! HighResolutionTimer::isTimerRunning())
        HighResolutionTimer::startTimer(43);
    
    spectrumAnalyser   -> repaint();
    spectrumDifference -> repaint();
    phaseDifference    -> repaint();
    stereoAnalyser     -> repaint();
    waveformAnalyser   -> repaint();
    levelMeter         -> repaint();
    
    String processorDelay   = (String) ( processor.graph.getLatencySamples() );
    textEditorTotalDelay -> setText(processorDelay);
}

void ChannelStripAnalyserAudioProcessorEditor::timerCallback()
{
    // checking flag states for plugin deletion, creation and re-creation
    for(auto i = 0; i < 6; i++)
    {
        if (processor.deletedPlugins[i]) // plugin is flagged for deletion
        {
            processor.deletePluginProcessor(i+1);
            processor.deletedPlugins[i] = false;
        }
        if (processor.createPluginCue[i]) // plugin is flagged for creation
        {
            createNewPlugin (getChosenType(chosenPlugins[i]), i+1);
            processor.createPluginCue[i] = false;
        }
        if (processor.recalledPlugins[i]) // plugin is flagged for re-creation
        {
            createNewPlugin(nullptr, i+1);
            if (editors[i] != nullptr) processor.recalledPlugins[i] = false;

        }
        if (processor.plugIns[i])
            if (editors[i] != nullptr) processor.shownPlugins[i] = editors[i]->isVisible();
    }
}

PluginDescription* ChannelStripAnalyserAudioProcessorEditor::getChosenType(const int menuID) const
{
    return processor.knownPluginList.getType(processor.knownPluginList.getIndexChosenByMenu(menuID));
}

void ChannelStripAnalyserAudioProcessorEditor::createNewPlugin(const PluginDescription* desc, int i)
{
    // the plugin is still flagged for deletion
    if (processor.deletedPlugins[i-1])
    {
        processor.createPluginCue[i-1] = true;
    }
    // the plugin slot is empty
    else
    {
        // the plugin is not flagged for re-creation,
        // if its flagged for re-creation the plugin
        // processor already exists (created in the setStateInformation function)
        if (!processor.recalledPlugins[i-1])
        {
            processor.createPluginProcessor(desc, i);
        }
        if (processor.graph.getNodeForId(i+2) != nullptr)
        {
            editors.set(i-1, SimplePluginWindow::getWindowFor (processor.graph.getNodeForId(i+2), false, editors[i-1]));
         
            if (editors[i-1] != nullptr)
            {
            editors[i-1]->toFront(true);
            editors[i-1]->setName((String)(processor.graph.getNodeForId(i+2)->getProcessor()->getName()));
            }
        }
         switch (i)
         {
         case 1: pluginInfoButton1->setText ((String) processor.graph.getNodeForId(i+2)->getProcessor()->getName(), NotificationType::dontSendNotification); break;
         case 2: pluginInfoButton2->setText ((String) processor.graph.getNodeForId(i+2)->getProcessor()->getName(), NotificationType::dontSendNotification); break;
         case 3: pluginInfoButton3->setText ((String) processor.graph.getNodeForId(i+2)->getProcessor()->getName(), NotificationType::dontSendNotification); break;
         case 4: pluginInfoButton4->setText ((String) processor.graph.getNodeForId(i+2)->getProcessor()->getName(), NotificationType::dontSendNotification); break;
         case 5: pluginInfoButton5->setText ((String) processor.graph.getNodeForId(i+2)->getProcessor()->getName(), NotificationType::dontSendNotification); break;
         case 6: pluginInfoButton6->setText ((String) processor.graph.getNodeForId(i+2)->getProcessor()->getName(), NotificationType::dontSendNotification); break;
         default: break;
         }
    }
}

void ChannelStripAnalyserAudioProcessorEditor::deletePlugin(int i)
{
    
    switch (i)
    {
        case 1: pluginInfoButton1->setText(""); break;
        case 2: pluginInfoButton2->setText(""); break;
        case 3: pluginInfoButton3->setText(""); break;
        case 4: pluginInfoButton4->setText(""); break;
        case 5: pluginInfoButton5->setText(""); break;
        case 6: pluginInfoButton6->setText(""); break;
        default: break;
    }
    
    // the function just deletes the editor side of the plugin and flags the processor for deletion afterwards.
    processor.plugIns[i-1] = false;
    processor.AudioPluginChannelConfiguration();
    
    editors.remove(i-1);
    editors.set(i-1, nullptr);
    processor.deletedPlugins[i-1] = true;
    
    
}

void ChannelStripAnalyserAudioProcessorEditor::fftSizeChanged() 
{
    cancelPendingUpdate();
    if (HighResolutionTimer::isTimerRunning())
        HighResolutionTimer::stopTimer();

    const int numOfChannels = processor.getTotalNumInputChannels();
    const int sampleRate = processor.getSampleRate();

    switch (fftSizeButton->getSelectedId())
    {
        case 1:
            forwFFT.changeFFTSize(1024);
            fftSize = 1024;
            spectrumAnalyser.reset   ( new SpectrumAnalyser   (sampleRate, fftSize, numOfChannels, mainAudioBufferSystem, forwFFT));
            spectrumDifference.reset ( new SpectrumDifference (sampleRate, fftSize, numOfChannels, mainAudioBufferSystem, forwFFT));
            phaseDifference.reset    ( new PhaseDifference    (sampleRate, fftSize, numOfChannels, mainAudioBufferSystem, forwFFT));
            break;
            
        case 2:
            forwFFT.changeFFTSize(2048);
            fftSize = 2048;
            spectrumAnalyser.reset   ( new SpectrumAnalyser   (sampleRate, fftSize, numOfChannels, mainAudioBufferSystem, forwFFT));
            spectrumDifference.reset ( new SpectrumDifference (sampleRate, fftSize, numOfChannels, mainAudioBufferSystem, forwFFT));
            phaseDifference.reset    ( new PhaseDifference    (sampleRate, fftSize, numOfChannels, mainAudioBufferSystem, forwFFT));
            break;
            
        case 3:
            forwFFT.changeFFTSize(4096);
            fftSize = 4096;
            spectrumAnalyser.reset   ( new SpectrumAnalyser   (sampleRate, fftSize, numOfChannels, mainAudioBufferSystem, forwFFT));
            spectrumDifference.reset ( new SpectrumDifference (sampleRate, fftSize, numOfChannels, mainAudioBufferSystem, forwFFT));
            phaseDifference.reset    ( new PhaseDifference    (sampleRate, fftSize, numOfChannels, mainAudioBufferSystem, forwFFT));
            break;
    }
    
    addAndMakeVisible(spectrumAnalyser);
    addAndMakeVisible(spectrumDifference);
    addAndMakeVisible(phaseDifference);
    
    sliderValueChanged(&sliderSpectrumAnalyserRange);
    sliderValueChanged(&sliderSpectrumAnalyserReturnTime);

    sliderValueChanged(&sliderSpectrumDifferenceRange);
    sliderValueChanged(&sliderSpectrumDifferenceTimeAverage);


    if ( ! HighResolutionTimer::isTimerRunning())
        HighResolutionTimer::startTimer(43);

}


//==============================================================================
void ChannelStripAnalyserAudioProcessorEditor::paint (Graphics& g)
{
    g.drawImage (backgroundImage, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
    //repaintVisualizers();
}

void ChannelStripAnalyserAudioProcessorEditor::paintOverChildren (Graphics& g)
{
    g.drawImage (foregroundImage, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
}

void ChannelStripAnalyserAudioProcessorEditor::createBackgroundUi()
{
    backgroundImage = Image(Image::PixelFormat::ARGB, getWidth(), getHeight(), true);
    Graphics g (backgroundImage);
    
    g.fillAll (Colour (0x11111111));
    {
        // BACKGROUND
        int x = 0, y = 0, width = 1280, height = 792;
        Colour fillColour = Colour (0xff111111);
        g.setColour (fillColour);
        g.fillRect (x, y, width, height);
    }
    {
        float x = 727.0f, y = 443.0f, width = 350.0f, height = 343.0f;
        Colour fillColour = Colour (0xff0f0f1c);
        g.setColour (fillColour);
        g.fillRoundedRectangle (x, y, width, height, 10.000f);
    }
    
    {
        float x = 727.0f, y = 443.0f, width = 350.0f, height = 343.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        g.setColour (strokeColour);
        g.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    
    {
        float x = 7.0f, y = 5.0f, width = 1267.0f, height = 76.0f;
        Colour fillColour = Colour (0xff0f0f1c);
        g.setColour (fillColour);
        g.fillRoundedRectangle (x, y, width, height, 10.000f);
    }
    
    {
        float x = 7.0f, y = 5.0f, width = 1267.0f, height = 76.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        g.setColour (strokeColour);
        g.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    {
        float x = 735.0f, y = 451.0f, width = 165.0f, height = 100.0f;
        Colour strokeColour = Colour (0xff42a2c8).withMultipliedSaturation(0.65);
        g.setColour (strokeColour);
        g.drawRoundedRectangle (x, y, width, height, 5.000f, 1.500f);
    }
    {
        float x = 905.0f, y = 451.0f, width = 164.0f, height = 100.0f;
        Colour strokeColour = Colour (0xff42a2c8).withMultipliedSaturation(0.65);
        g.setColour (strokeColour);
        g.drawRoundedRectangle (x, y, width, height, 5.000f, 1.500f);
    }
    {
        float x = 735.0f, y = 556.0f, width = 334.0f, height = 140.0f;
        Colour strokeColour = Colour (0xff42a2c8).withMultipliedSaturation(0.65);
        g.setColour (strokeColour);
        g.drawRoundedRectangle (x, y, width, height, 5.000f, 1.500f);
    }
    {
        float x = 735.0f, y = 701.0f, width = 334.0f, height = 78.0f;
        Colour strokeColour = Colour (0xff42a2c8).withMultipliedSaturation(0.65);
        g.setColour (strokeColour);
        g.drawRoundedRectangle (x, y, width, height, 5.000f, 1.500f);
    }

    {
        float x = 735.0f, y = 451.0f, width = 165.0f, height = 100.0f;
        Colour strokeColour = Colour (0xff42a2c8).withAlpha(0.03f);
        g.setColour (strokeColour);
        g.fillRoundedRectangle (x, y, width, height, 5.000f);
    }
    {
        float x = 905.0f, y = 451.0f, width = 164.0f, height = 100.0f;
        Colour strokeColour = Colour (0xff42a2c8).withAlpha(0.03f);
        g.setColour (strokeColour);
        g.fillRoundedRectangle (x, y, width, height, 5.000f);
    }
    {
        float x = 735.0f, y = 556.0f, width = 334.0f, height = 140.0f;
        Colour strokeColour = Colour (0xff42a2c8).withAlpha(0.03f);
        g.setColour (strokeColour);
        g.fillRoundedRectangle (x, y, width, height, 5.000f);
    }
    {
        float x = 735.0f, y = 701.0f, width = 334.0f, height = 78.0f;
        Colour strokeColour = Colour (0xff42a2c8).withAlpha(0.03f);
        g.setColour (strokeColour);
        g.fillRoundedRectangle (x, y, width, height, 5.000f);
    }


    //[UserPaint] Add your own custom painting code here..
    foregroundImage = Image(Image::PixelFormat::ARGB, getWidth(), getHeight(), true);
    Graphics g2 (foregroundImage);
    
    {
        // WAVEFORM
        float x = 7.0f, y = 87.0f, width = 714.0f, height = 170.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        g2.setColour (strokeColour);
        g2.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    
    {
        // SPECTRUM ANALYSER
        float x = 7.0f, y = 263.0f, width = 714.0f, height = 281.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        g2.setColour (strokeColour);
        g2.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    
    {
        // SPECTRUM DIFF / PHASE
        float x = 7.0f, y = 550.0f, width = 714.0f, height = 236.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        g2.setColour (strokeColour);
        g2.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    
    {
        // STEREO VIEW
        float x = 727.0f, y = 87.0f, width = 350.0f, height = 350.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        g2.setColour (strokeColour);
        g2.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    {
        // LEVEL METER
        float x = 1083.0f, y = 87.0f, width = 190.0f, height = 700.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        g2.setColour (strokeColour);
        g2.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    {
        Colour strokeColour = Colour (0xff42a2c8);
        g2.setColour(strokeColour);
        g2.fillRect (7.0f, 667.0f, 714.0f, 2.000f);
    }
}

void ChannelStripAnalyserAudioProcessorEditor::resized()
{
}

void ChannelStripAnalyserAudioProcessorEditor::sliderValueChanged (Slider* sliderThatWasMoved)
{
    //[UsersliderValueChanged_Pre]
    //==============================================================================
    if (sliderThatWasMoved == &sliderWaveformRange)
    {
        //[UserSliderCode_sliderCurbeOffset] -- add your slider handling code here..
        waveformAnalyser -> rangeAt.store((int) sliderWaveformRange.getValue());
        waveformAnalyser -> axisNeedsUpdate.store(true);
        //[/UserSliderCode_sliderCurbeOffset]
    }
    else if (sliderThatWasMoved == &sliderWaveformGain)
    {
        //[UserSliderCode_sliderDifferenceGain] -- add your slider handling code here..
        waveformAnalyser -> gainAt.store( sliderWaveformGain.getValue());
        //[/UserSliderCode_sliderDifferenceGain]
    }
    else if (sliderThatWasMoved == &sliderVectorscopeRange)
    {
        //[UserSliderCode_sliderRangeVectorscope] -- add your slider handling code here..
        stereoAnalyser -> zoomAt.store( sliderVectorscopeRange.getValue());
        //[/UserSliderCode_sliderRangeVectorscope]
    }
    else if (sliderThatWasMoved == &sliderVectorscopeVanishTime)
    {
        //[UserSliderCode_sliderVanishTime] -- add your slider handling code here..
        stereoAnalyser -> numOfHistorySamplesAt.store( sliderVectorscopeVanishTime.getValue());
        //[/UserSliderCode_sliderVanishTime]
    }
    else if (sliderThatWasMoved == &sliderSpectrumAnalyserRange)
    {
        spectrumAnalyser -> scaleModeAt.store((int) sliderSpectrumAnalyserRange.getValue());
        spectrumAnalyser -> axisNeedsUpdate.store(true);
    }
    else if (sliderThatWasMoved == &sliderSpectrumAnalyserReturnTime)
    {
        switch ((int)sliderSpectrumAnalyserReturnTime.getValue()) {
            case 1:
                spectrumAnalyser -> decayRatioAt.store (0.70f);
                break;
            case 2:
                spectrumAnalyser -> decayRatioAt.store (0.80f);
                break;
            case 3:
                spectrumAnalyser -> decayRatioAt.store (0.90f);
                break;
            case 4:
                spectrumAnalyser -> decayRatioAt.store (0.92f);
                break;
        }
    }
    
    //==============================================================================
    else if (sliderThatWasMoved == &sliderSpectrumDifferenceTimeAverage)
    {
        //[UserSliderCode_sliderTimeAverage] -- add your slider handling code here..
        switch ((int)sliderSpectrumDifferenceTimeAverage.getValue()) {
            case 1:
                spectrumDifference -> numOfHistorySamplesAt.store(3);
                phaseDifference    -> numOfHistorySamplesAt.store(3);
                break;
            case 2:
                spectrumDifference -> numOfHistorySamplesAt.store(5);
                phaseDifference    -> numOfHistorySamplesAt.store(5);
                break;
            case 3:
                spectrumDifference -> numOfHistorySamplesAt.store(8);
                phaseDifference    -> numOfHistorySamplesAt.store(8);
                break;
            case 4:
                spectrumDifference -> numOfHistorySamplesAt.store(12);
                phaseDifference    -> numOfHistorySamplesAt.store(12);
                break;
        }
        //[/UserSliderCode_sliderTimeAverage]
    }
    else if (sliderThatWasMoved == &sliderSpectrumDifferenceRange)
    {
        //[UserSliderCode_sliderRangeSpectrumDifference] -- add your slider handling code here..
        spectrumDifference -> scaleModeAt.store((int) sliderSpectrumDifferenceRange.getValue());
        spectrumDifference -> axisNeedsUpdate.store(true);
        //[/UserSliderCode_sliderRangeSpectrumDifference]
    }

}

void ChannelStripAnalyserAudioProcessorEditor::buttonClicked (Button* buttonThatWasClicked)
{
    //[UserbuttonClicked_Pre]
    
    juce::PopupMenu::Item item;
    item.text = "empty";
    item.itemID = 909;
    PopupMenu pluginsMenu;
    pluginsMenu.addItem(item);
    processor.knownPluginList.addToMenu(pluginsMenu, KnownPluginList::sortByManufacturer);
    
    //[/UserbuttonClicked_Pre]
    if      (buttonThatWasClicked == freezeButton)
    {
        if (freezeState == false)
        {
            freezeState = true;
            HighResolutionTimer::stopTimer();
            freezeButton->setColour (TextButton::ColourIds::buttonColourId, Colour (0xff42a2c8) );
        }
        else
        {
            freezeState = false;
            HighResolutionTimer::startTimer(43);
            freezeButton->setColour (TextButton::ColourIds::buttonColourId, Colour (0xff181f22).darker() );
        }
    }
    else if (buttonThatWasClicked == compensateDelay)
    {
        processor.triggerGraphPrepareToPlay();
    }
    else if (buttonThatWasClicked == &textButtonMonoMode)
    {
        spectrumDifference -> analyseMode.store (1);
        phaseDifference    -> analyseMode.store (1);
        
        textButtonLRMode   .setToggleState(false, juce::NotificationType::dontSendNotification);
        textButtonMSMode   .setToggleState(false, juce::NotificationType::dontSendNotification);
    }
    else if (buttonThatWasClicked == &textButtonLRMode)
    {
        spectrumDifference -> analyseMode.store (2);
        phaseDifference    -> analyseMode.store (2);
        
        textButtonMonoMode .setToggleState(false, juce::NotificationType::dontSendNotification);
        textButtonMSMode   .setToggleState(false, juce::NotificationType::dontSendNotification);
    }
    else if (buttonThatWasClicked == &textButtonMSMode)
    {
        spectrumDifference -> analyseMode.store (3);
        phaseDifference    -> analyseMode.store (3);
        
        textButtonLRMode   .setToggleState(false, juce::NotificationType::dontSendNotification);
        textButtonMonoMode .setToggleState(false, juce::NotificationType::dontSendNotification);
    }
    else if (buttonThatWasClicked == pluginsManagerButton)
    {
        
        if (pluginWinState == false)
        {
            pluginListWindow->open();
            pluginWinState = true;
        }
        else
        {
            pluginListWindow->closeButtonPressed();
        }
    }
    
    // Plugin 1 controls
    else if (buttonThatWasClicked == pluginLoadButton1)
    {
        int r = pluginsMenu.show();
        
        if (r == 909) {
            if ( processor.plugIns[0] == true ) {
                deletePlugin(1);
                
            }
        }
        else {
            if (r != 0)
            {
                if ( processor.plugIns[0] == true )
                {
                    deletePlugin(1);
                    chosenPlugins[0] = r;
                }
                createNewPlugin (getChosenType(r),1);
                
            }
        }
    }
    
    else if (buttonThatWasClicked == pluginOpenButton1)
    {
        if (processor.plugIns[0])
            if (editors[0] != nullptr) editors[0]->setVisible(true);
    }
    
    else if (buttonThatWasClicked == &pluginBypassButton1)
    {
        processor.bypassFlag[0] = buttonThatWasClicked->getToggleState();
        processor.AudioPluginChannelConfiguration();
    }
    
    // Plugin 2 controls
    else if (buttonThatWasClicked == pluginLoadButton2)
    {
        int r = pluginsMenu.show();
        
        if (r == 909) {
            if ( processor.plugIns[1] == true ) {
                deletePlugin(2);
            }
        }
        else {
            if (r != 0)
            {
                if ( processor.plugIns[1] == true )
                {
                    deletePlugin(2);
                    chosenPlugins[1] = r;
                }
                createNewPlugin (getChosenType(r),2);
            }
        }
    }
    
    else if (buttonThatWasClicked == pluginOpenButton2)
    {
        if (processor.plugIns[1])
            if (editors[1] != nullptr) editors[1]->setVisible(true);

    }
    
    else if (buttonThatWasClicked == &pluginBypassButton2)
    {
        processor.bypassFlag[1] = buttonThatWasClicked->getToggleState();
        processor.AudioPluginChannelConfiguration();
    }
    
    // Plugin 3 controls
    else if (buttonThatWasClicked == pluginLoadButton3)
    {
        int r = pluginsMenu.show();
        
        if (r == 909) {
            if ( processor.plugIns[2] == true ) {
                deletePlugin(3);
            }
        }
        else {
            if (r != 0)
            {
                if ( processor.plugIns[2] == true )
                {
                    deletePlugin(3);
                    chosenPlugins[2] = r;
                }
                createNewPlugin (getChosenType(r),3);
                
            }
        }
    }
    
    else if (buttonThatWasClicked == pluginOpenButton3)
    {
        if (processor.plugIns[2])
            if (editors[2] != nullptr) editors[2]->setVisible(true);
    }
    
    else if (buttonThatWasClicked == &pluginBypassButton3)
    {
        processor.bypassFlag[2] = buttonThatWasClicked->getToggleState();
        processor.AudioPluginChannelConfiguration();
    }
    
    // Plugin 4 controls
    else if (buttonThatWasClicked == pluginLoadButton4)
    {
        int r = pluginsMenu.show();
        
        if (r == 909) {
            if ( processor.plugIns[3] == true ) {
                deletePlugin(4);
            }
        }
        else {
            if (r != 0)
            {
                if ( processor.plugIns[3] == true )
                {
                    deletePlugin(4);
                    chosenPlugins[3] = r;
                }
                createNewPlugin (getChosenType(r),4);
                
            }
        }
    }
    
    else if (buttonThatWasClicked == pluginOpenButton4)
    {
        if (processor.plugIns[3])
            if (editors[3] != nullptr) editors[3]->setVisible(true);
    }
    
    else if (buttonThatWasClicked == &pluginBypassButton4)
    {
        processor.bypassFlag[3] = buttonThatWasClicked->getToggleState();
        processor.AudioPluginChannelConfiguration();
    }
    
    // Plugin 5 controls
    else if (buttonThatWasClicked == pluginLoadButton5)
    {
        int r = pluginsMenu.show();
        
        if (r == 909) {
            if ( processor.plugIns[4] == true ) {
                deletePlugin(5);
            }
        }
        else {
            if (r != 0)
            {
                if ( processor.plugIns[4] == true )
                {
                    deletePlugin(5);
                    chosenPlugins[4] = r;
                }
                createNewPlugin (getChosenType(r),5);
                
            }
        }
    }
    
    else if (buttonThatWasClicked == pluginOpenButton5)
    {
        if (processor.plugIns[4])
            if (editors[4] != nullptr) editors[4]->setVisible(true);
    }
    
    else if (buttonThatWasClicked == &pluginBypassButton5)
    {
        processor.bypassFlag[4] = buttonThatWasClicked->getToggleState();
        processor.AudioPluginChannelConfiguration();
    }
    
    // Plugin 6 controls
    else if (buttonThatWasClicked == pluginLoadButton6)
    {
        int r = pluginsMenu.show();
        
        if (r == 909) {
            if ( processor.plugIns[5] == true ) {
                deletePlugin(6);
            }
        }
        else {
            if (r != 0)
            {
                if ( processor.plugIns[5] == true )
                {
                    deletePlugin(6);
                    chosenPlugins[5] = r;
                }
                createNewPlugin (getChosenType(r),6);
                
            }
        }
    }
    
    else if (buttonThatWasClicked == pluginOpenButton6)
    {
        if (processor.plugIns[5])
            if (editors[5] != nullptr) editors[5]->setVisible(true);
    }
    
    else if (buttonThatWasClicked == &pluginBypassButton6)
    {
        processor.bypassFlag[5] = buttonThatWasClicked->getToggleState();
        processor.AudioPluginChannelConfiguration();
    }
    
    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}

//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
//==============================================================================
void ChannelStripAnalyserAudioProcessorEditor::addUiElements()
{
// ========================================================================================================================
    // WAVEFORM
    addAndMakeVisible (sliderWaveformRange);
    sliderWaveformRange.setRange (1, 3, 1);
    sliderWaveformRange.setSliderStyle (Slider::LinearHorizontal);
    sliderWaveformRange.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderWaveformRange.addListener (this);
    sliderWaveformRange.setBounds (744, 492, 72, 16);
    
    addAndMakeVisible (sliderWaveformGain);
    sliderWaveformGain.setRange (0, 2, 0);
    sliderWaveformGain.setSliderStyle (Slider::LinearHorizontal);
    sliderWaveformGain.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderWaveformGain.addListener (this);
    sliderWaveformGain.setBounds (744, 524, 72, 16);
    
    addAndMakeVisible (waveformNameTitle = new Label (String(),TRANS("WAVEFORM")));
    waveformNameTitle->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    waveformNameTitle->setJustificationType (Justification::centredLeft);
    waveformNameTitle->setEditable (false, false, false);
    waveformNameTitle->setColour (TextEditor::textColourId, Colours::black);
    waveformNameTitle->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    waveformNameTitle->setBounds (745, 460, 150, 24);
    
    addAndMakeVisible (waveformRangeLabelText = new Label (String(),TRANS("range\n")));
    waveformRangeLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    waveformRangeLabelText->setJustificationType (Justification::centredLeft);
    waveformRangeLabelText->setEditable (false, false, false);
    waveformRangeLabelText->setColour (TextEditor::textColourId, Colours::black);
    waveformRangeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    waveformRangeLabelText->setBounds (816, 487, 84, 24);
    
    addAndMakeVisible (waveformGainLabelText = new Label (String(),TRANS("gain\n")));
    waveformGainLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    waveformGainLabelText->setJustificationType (Justification::centredLeft);
    waveformGainLabelText->setEditable (false, false, false);
    waveformGainLabelText->setColour (TextEditor::textColourId, Colours::black);
    waveformGainLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    waveformGainLabelText->setBounds (816, 519, 90, 24);

    // ========================================================================================================================
    // VECTORSCOPE
    addAndMakeVisible (sliderVectorscopeRange);
    sliderVectorscopeRange.setRange (50, 400, 0);
    sliderVectorscopeRange.setSliderStyle (Slider::LinearHorizontal);
    sliderVectorscopeRange.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderVectorscopeRange.addListener (this);
    sliderVectorscopeRange.setBounds (909, 492, 72, 16);
    
    addAndMakeVisible (sliderVectorscopeVanishTime);
    sliderVectorscopeVanishTime.setRange (5, 50, 1);
    sliderVectorscopeVanishTime.setSliderStyle (Slider::LinearHorizontal);
    sliderVectorscopeVanishTime.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderVectorscopeVanishTime.addListener (this);
    sliderVectorscopeVanishTime.setBounds (909, 524, 72, 16);
    
    addAndMakeVisible (vectorscopeNameTitle = new Label (String(),TRANS("VECTORSCOPE\n")));
    vectorscopeNameTitle->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    vectorscopeNameTitle->setJustificationType (Justification::centredLeft);
    vectorscopeNameTitle->setEditable (false, false, false);
    vectorscopeNameTitle->setColour (TextEditor::textColourId, Colours::black);
    vectorscopeNameTitle->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    vectorscopeNameTitle->setBounds (909, 460, 150, 24);
    
    addAndMakeVisible (vectorscopeRangeLabelText = new Label (String(),TRANS("range\n")));
    vectorscopeRangeLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    vectorscopeRangeLabelText->setJustificationType (Justification::centredLeft);
    vectorscopeRangeLabelText->setEditable (false, false, false);
    vectorscopeRangeLabelText->setColour (TextEditor::textColourId, Colours::black);
    vectorscopeRangeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    vectorscopeRangeLabelText->setBounds (981, 487, 150, 24);
    
    addAndMakeVisible (vectorscopeVanishTimeLabelText = new Label (String(),TRANS("vanish time\n")));
    vectorscopeVanishTimeLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    vectorscopeVanishTimeLabelText->setJustificationType (Justification::centredLeft);
    vectorscopeVanishTimeLabelText->setEditable (false, false, false);
    vectorscopeVanishTimeLabelText->setColour (TextEditor::textColourId, Colours::black);
    vectorscopeVanishTimeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    vectorscopeVanishTimeLabelText->setBounds (981, 519, 150, 24);
    
    // ========================================================================================================================
    // SPECTRUM ANALYSER

    addAndMakeVisible (sliderSpectrumAnalyserRange);
    sliderSpectrumAnalyserRange.setRange (0, 3, 1);
    sliderSpectrumAnalyserRange.setSliderStyle (Slider::LinearHorizontal);
    sliderSpectrumAnalyserRange.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderSpectrumAnalyserRange.addListener (this);
    sliderSpectrumAnalyserRange.setBounds (744, 598, 72, 16);
    
    addAndMakeVisible (sliderSpectrumAnalyserReturnTime);
    sliderSpectrumAnalyserReturnTime.setRange (0, 10, 0);
    sliderSpectrumAnalyserReturnTime.setSliderStyle (Slider::LinearHorizontal);
    sliderSpectrumAnalyserReturnTime.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderSpectrumAnalyserReturnTime.addListener (this);
    sliderSpectrumAnalyserReturnTime.setBounds (744, 630, 72, 16);
    
    addAndMakeVisible (spectrumAnalyserNameTitle = new Label (String(),TRANS("SPECTRUM ANALYSER\n")));
    spectrumAnalyserNameTitle->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    spectrumAnalyserNameTitle->setJustificationType (Justification::centredLeft);
    spectrumAnalyserNameTitle->setEditable (false, false, false);
    spectrumAnalyserNameTitle->setColour (TextEditor::textColourId, Colours::black);
    spectrumAnalyserNameTitle->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    spectrumAnalyserNameTitle->setBounds (745, 567, 150, 24);
    
    addAndMakeVisible (spectrumAnalyserRangeLabelText = new Label (String(),TRANS("range\n")));
    spectrumAnalyserRangeLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    spectrumAnalyserRangeLabelText->setJustificationType (Justification::centredLeft);
    spectrumAnalyserRangeLabelText->setEditable (false, false, false);
    spectrumAnalyserRangeLabelText->setColour (TextEditor::textColourId, Colours::black);
    spectrumAnalyserRangeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    spectrumAnalyserRangeLabelText->setBounds (816, 593, 150, 24);
    
    addAndMakeVisible (spectrumAnalyserReturnTimeLabelText = new Label (String(),TRANS("return time\n")));
    spectrumAnalyserReturnTimeLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    spectrumAnalyserReturnTimeLabelText->setJustificationType (Justification::centredLeft);
    spectrumAnalyserReturnTimeLabelText->setEditable (false, false, false);
    spectrumAnalyserReturnTimeLabelText->setColour (TextEditor::textColourId, Colours::black);
    spectrumAnalyserReturnTimeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    spectrumAnalyserReturnTimeLabelText->setBounds (816, 625, 150, 24);
    
    // ========================================================================================================================
    // SPECTRUM DIFFERENCE

    addAndMakeVisible (sliderSpectrumDifferenceRange);
    sliderSpectrumDifferenceRange.setRange (1, 3, 1);
    sliderSpectrumDifferenceRange.setSliderStyle (Slider::LinearHorizontal);
    sliderSpectrumDifferenceRange.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderSpectrumDifferenceRange.addListener (this);
    sliderSpectrumDifferenceRange.setBounds (909, 598, 72, 16);
    
    addAndMakeVisible (sliderSpectrumDifferenceTimeAverage);
    sliderSpectrumDifferenceTimeAverage.setRange (0, 10, 0);
    sliderSpectrumDifferenceTimeAverage.setSliderStyle (Slider::LinearHorizontal);
    sliderSpectrumDifferenceTimeAverage.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderSpectrumDifferenceTimeAverage.addListener (this);
    sliderSpectrumDifferenceTimeAverage.setBounds (909, 630, 72, 16);
    
    addAndMakeVisible (spectrumDifferenceNameTitle = new Label (String(),TRANS("SPECTRUM DIFFERENCE\n""\n")));
    spectrumDifferenceNameTitle->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    spectrumDifferenceNameTitle->setJustificationType (Justification::centredLeft);
    spectrumDifferenceNameTitle->setEditable (false, false, false);
    spectrumDifferenceNameTitle->setColour (TextEditor::textColourId, Colours::black);
    spectrumDifferenceNameTitle->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    spectrumDifferenceNameTitle->setBounds (909, 567, 150, 24);
    
    addAndMakeVisible (spectrumDifferenceRangeLabelText = new Label (String(),TRANS("range\n")));
    spectrumDifferenceRangeLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    spectrumDifferenceRangeLabelText->setJustificationType (Justification::centredLeft);
    spectrumDifferenceRangeLabelText->setEditable (false, false, false);
    spectrumDifferenceRangeLabelText->setColour (TextEditor::textColourId, Colours::black);
    spectrumDifferenceRangeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    spectrumDifferenceRangeLabelText->setBounds (981, 593, 80, 24);
    
    addAndMakeVisible (spectrumDifferencetimeAverageLabelText = new Label (String(),TRANS("time average\n")));
    spectrumDifferencetimeAverageLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    spectrumDifferencetimeAverageLabelText->setJustificationType (Justification::centredLeft);
    spectrumDifferencetimeAverageLabelText->setEditable (false, false, false);
    spectrumDifferencetimeAverageLabelText->setColour (TextEditor::textColourId, Colours::black);
    spectrumDifferencetimeAverageLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    spectrumDifferencetimeAverageLabelText->setBounds (981, 625, 80, 24);
    
    
// ========================================================================================================================
    // MODE MONO STEREO MID/SIDE
    addAndMakeVisible (textButtonMonoMode);
    textButtonMonoMode.addListener (this);
    textButtonMonoMode.setColour (TextEditor::backgroundColourId, Colour (0xff181f22).darker());
    textButtonMonoMode.setColour (TextEditor::outlineColourId, Colours::grey);
    textButtonMonoMode.setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    textButtonMonoMode.setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    textButtonMonoMode.setBounds (744, 655, 32, 32);
    
    addAndMakeVisible (monoModeLabelText = new Label (String(),TRANS("M")));
    monoModeLabelText->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    monoModeLabelText->setJustificationType (Justification::centredLeft);
    monoModeLabelText->setEditable (false, false, false);
    monoModeLabelText->setColour (TextEditor::textColourId, Colours::black);
    monoModeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    monoModeLabelText->setBounds (764, 659, 25, 25);
    
    addAndMakeVisible (textButtonLRMode);
    textButtonLRMode.addListener (this);
    textButtonLRMode.setColour (TextEditor::backgroundColourId, Colour (0xff181f22).darker());
    textButtonLRMode.setColour (TextEditor::outlineColourId, Colours::grey);
    textButtonLRMode.setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    textButtonLRMode.setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    textButtonLRMode.setBounds (784, 655, 32, 32);
    
    addAndMakeVisible (leftRightModeLabelText = new Label (String(),TRANS("LR")));
    leftRightModeLabelText->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    leftRightModeLabelText->setJustificationType (Justification::centredLeft);
    leftRightModeLabelText->setEditable (false, false, false);
    leftRightModeLabelText->setColour (TextEditor::textColourId, Colours::black);
    leftRightModeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    leftRightModeLabelText->setBounds (804, 659, 25, 25);
    
    addAndMakeVisible (textButtonMSMode);
    textButtonMSMode.addListener (this);
    textButtonMSMode.setColour (TextEditor::backgroundColourId, Colour (0xff181f22).darker());
    textButtonMSMode.setColour (TextEditor::outlineColourId, Colours::grey);
    textButtonMSMode.setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    textButtonMSMode.setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    textButtonMSMode.setBounds (829, 655, 32, 32);
    
    addAndMakeVisible (midSideModeLabelText = new Label (String(),TRANS("MS")));
    midSideModeLabelText->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    midSideModeLabelText->setJustificationType (Justification::centredLeft);
    midSideModeLabelText->setEditable (false, false, false);
    midSideModeLabelText->setColour (TextEditor::textColourId, Colours::black);
    midSideModeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    midSideModeLabelText->setBounds (849, 659, 25, 25);

// ========================================================================================================================
    // FFT SIZE
    addAndMakeVisible (fftSizeButton = new ComboBox (String()));
    fftSizeButton->setColour(ComboBox::outlineColourId, Colours::grey);
    fftSizeButton->setColour(ComboBox::arrowColourId, Colour (0xff42a2c8));
    fftSizeButton->setColour(ComboBox::backgroundColourId, Colour (0xff181f22).darker());
    fftSizeButton->addItem("1024", 1);
    fftSizeButton->addItem("2048", 2);
    fftSizeButton->addItem("4098", 3);
    fftSizeButton->setSelectedId(1);
    fftSizeButton->setBounds (909, 662, 72, 18);
    fftSizeButton->onChange = [this] { fftSizeChanged(); };
    
    addAndMakeVisible (fftSizeLabelText = new Label (String(),TRANS("FFT size\n")));
    fftSizeLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    fftSizeLabelText->setJustificationType (Justification::centredLeft);
    fftSizeLabelText->setEditable (false, false, false);
    fftSizeLabelText->setColour (TextEditor::textColourId, Colours::black);
    fftSizeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    fftSizeLabelText->setBounds (981, 662, 80, 24);
    
    // DELAY
    addAndMakeVisible (textEditorTotalDelay = new TextEditor (String()));
    textEditorTotalDelay->setMultiLine (false);
    textEditorTotalDelay->setReturnKeyStartsNewLine (false);
    textEditorTotalDelay->setReadOnly (true);
    textEditorTotalDelay->setScrollbarsShown (true);
    textEditorTotalDelay->setCaretVisible (false);
    textEditorTotalDelay->setPopupMenuEnabled (true);
    textEditorTotalDelay->setColour (TextEditor::backgroundColourId, Colour (0xff181f22).darker());
    textEditorTotalDelay->setColour (TextEditor::outlineColourId, Colours::grey);
    textEditorTotalDelay->setText (String());
    textEditorTotalDelay->setBounds (747, 720, 72, 18);
    
    addAndMakeVisible (totalDelayLabelText = new Label (String(),TRANS("total delay\n")));
    totalDelayLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    totalDelayLabelText->setJustificationType (Justification::centredLeft);
    totalDelayLabelText->setEditable (false, false, false);
    totalDelayLabelText->setColour (TextEditor::textColourId, Colours::black);
    totalDelayLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    totalDelayLabelText->setBounds (819, 720, 150, 24);
    
    
    addAndMakeVisible (freezeButton = new TextButton (String()));
    freezeButton->setButtonText (TRANS("freeze"));
    freezeButton->addListener (this);
    freezeButton->setColour (TextButton::buttonColourId, Colour (0xff181f22).darker());
    freezeButton->setColour (TextButton::buttonOnColourId, Colour (0xff181f22).brighter());
    freezeButton->setBounds (915, 720, 125, 40);
    
    addAndMakeVisible (compensateDelay = new TextButton (String()));
    compensateDelay->setButtonText (TRANS("compensate delay"));
    compensateDelay->addListener (this);
    compensateDelay->setColour (TextButton::buttonColourId, Colour (0xff181f22).darker());
    compensateDelay->setColour (TextButton::buttonOnColourId, Colour (0xff181f22).brighter());
    compensateDelay->setBounds (747, 742, 136, 18);

// ========================================================================================================================
    // PLUGIN SLOTS
    
    // MANAGER
    addAndMakeVisible (pluginsManagerButton = new TextButton (String()));
    pluginsManagerButton->setButtonText (TRANS("plugins manager"));
    pluginsManagerButton->addListener (this);
    pluginsManagerButton->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginsManagerButton->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginsManagerButton->setBounds (17, 16, 143, 32);
    
    // 1
    addAndMakeVisible (pluginLoadButton1 = new TextButton (String()));
    pluginLoadButton1->setButtonText (TRANS("load plugin"));
    pluginLoadButton1->addListener (this);
    pluginLoadButton1->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginLoadButton1->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginLoadButton1->setBounds (184, 16, 104, 24);
    
    addAndMakeVisible (pluginInfoButton1 = new TextEditor (String()));
    pluginInfoButton1->setMultiLine (false);
    pluginInfoButton1->setReturnKeyStartsNewLine (false);
    pluginInfoButton1->setReadOnly (true);
    pluginInfoButton1->setScrollbarsShown (true);
    pluginInfoButton1->setCaretVisible (false);
    pluginInfoButton1->setPopupMenuEnabled (true);
    pluginInfoButton1->setColour (TextEditor::backgroundColourId, Colour (0xff181f22));
    pluginInfoButton1->setColour (TextEditor::outlineColourId, Colour (0xff42a2c8));
    pluginInfoButton1->setText (String());
    pluginInfoButton1->setBounds (184, 48, 104, 24);
    
    addAndMakeVisible (pluginOpenButton1 = new TextButton (String()));
    pluginOpenButton1->addListener (this);
    pluginOpenButton1->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginOpenButton1->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginOpenButton1->setBounds (296, 48, 24, 24);
    
    addAndMakeVisible (pluginBypassButton1);
    pluginBypassButton1.addListener (this);
    pluginBypassButton1.setBounds (296, 16, 24, 24);
    
    // 2
    addAndMakeVisible (pluginLoadButton2 = new TextButton (String()));
    pluginLoadButton2->setButtonText (TRANS("load plugin"));
    pluginLoadButton2->addListener (this);
    pluginLoadButton2->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginLoadButton2->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginLoadButton2->setBounds (336, 16, 104, 24);
    
    addAndMakeVisible (pluginInfoButton2 = new TextEditor (String()));
    pluginInfoButton2->setMultiLine (false);
    pluginInfoButton2->setReturnKeyStartsNewLine (false);
    pluginInfoButton2->setReadOnly (true);
    pluginInfoButton2->setScrollbarsShown (true);
    pluginInfoButton2->setCaretVisible (false);
    pluginInfoButton2->setPopupMenuEnabled (true);
    pluginInfoButton2->setColour (TextEditor::backgroundColourId, Colour (0xff181f22));
    pluginInfoButton2->setColour (TextEditor::outlineColourId, Colour (0xff42a2c8));
    pluginInfoButton2->setText (String());
    pluginInfoButton2->setBounds (336, 48, 104, 24);
    
    addAndMakeVisible (pluginOpenButton2 = new TextButton (String()));
    pluginOpenButton2->addListener (this);
    pluginOpenButton2->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginOpenButton2->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginOpenButton2->setBounds (448, 48, 24, 24);
    
    addAndMakeVisible (pluginBypassButton2);
    pluginBypassButton2.addListener (this);
    pluginBypassButton2.setBounds (448, 16, 24, 24);
    
    // 3
    addAndMakeVisible (pluginLoadButton3 = new TextButton (String()));
    pluginLoadButton3->setButtonText (TRANS("load plugin"));
    pluginLoadButton3->addListener (this);
    pluginLoadButton3->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginLoadButton3->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginLoadButton3->setBounds (488, 16, 104, 24);
    
    addAndMakeVisible (pluginInfoButton3 = new TextEditor (String()));
    pluginInfoButton3->setMultiLine (false);
    pluginInfoButton3->setReturnKeyStartsNewLine (false);
    pluginInfoButton3->setReadOnly (true);
    pluginInfoButton3->setScrollbarsShown (true);
    pluginInfoButton3->setCaretVisible (false);
    pluginInfoButton3->setPopupMenuEnabled (true);
    pluginInfoButton3->setColour (TextEditor::backgroundColourId, Colour (0xff181f22));
    pluginInfoButton3->setColour (TextEditor::outlineColourId, Colour (0xff42a2c8));
    pluginInfoButton3->setText (String());
    pluginInfoButton3->setBounds (488, 48, 104, 24);
    
    addAndMakeVisible (pluginOpenButton3 = new TextButton (String()));
    pluginOpenButton3->addListener (this);
    pluginOpenButton3->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginOpenButton3->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginOpenButton3->setBounds (600, 48, 24, 24);
    
    addAndMakeVisible (pluginBypassButton3);
    pluginBypassButton3.addListener (this);
    pluginBypassButton3.setBounds (600, 16, 24, 24);
    
    // 4
    addAndMakeVisible (pluginLoadButton4 = new TextButton (String()));
    pluginLoadButton4->setButtonText (TRANS("load plugin"));
    pluginLoadButton4->addListener (this);
    pluginLoadButton4->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginLoadButton4->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginLoadButton4->setBounds (640, 16, 104, 24);
    
    addAndMakeVisible (pluginInfoButton4 = new TextEditor (String()));
    pluginInfoButton4->setMultiLine (false);
    pluginInfoButton4->setReturnKeyStartsNewLine (false);
    pluginInfoButton4->setReadOnly (true);
    pluginInfoButton4->setScrollbarsShown (true);
    pluginInfoButton4->setCaretVisible (false);
    pluginInfoButton4->setPopupMenuEnabled (true);
    pluginInfoButton4->setColour (TextEditor::backgroundColourId, Colour (0xff181f22));
    pluginInfoButton4->setColour (TextEditor::outlineColourId, Colour (0xff42a2c8));
    pluginInfoButton4->setText (String());
    pluginInfoButton4->setBounds (640, 48, 104, 24);
    
    addAndMakeVisible (pluginOpenButton4 = new TextButton (String()));
    pluginOpenButton4->addListener (this);
    pluginOpenButton4->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginOpenButton4->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginOpenButton4->setBounds (752, 48, 24, 24);
    
    addAndMakeVisible (pluginBypassButton4);
    pluginBypassButton4.addListener (this);
    pluginBypassButton4.setBounds (752, 16, 24, 24);
    
    // 5
    addAndMakeVisible (pluginLoadButton5 = new TextButton (String()));
    pluginLoadButton5->setButtonText (TRANS("load plugin"));
    pluginLoadButton5->addListener (this);
    pluginLoadButton5->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginLoadButton5->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginLoadButton5->setBounds (792, 16, 104, 24);
    
    addAndMakeVisible (pluginInfoButton5 = new TextEditor (String()));
    pluginInfoButton5->setMultiLine (false);
    pluginInfoButton5->setReturnKeyStartsNewLine (false);
    pluginInfoButton5->setReadOnly (true);
    pluginInfoButton5->setScrollbarsShown (true);
    pluginInfoButton5->setCaretVisible (false);
    pluginInfoButton5->setPopupMenuEnabled (true);
    pluginInfoButton5->setColour (TextEditor::backgroundColourId, Colour (0xff181f22));
    pluginInfoButton5->setColour (TextEditor::outlineColourId, Colour (0xff42a2c8));
    pluginInfoButton5->setText (String());
    pluginInfoButton5->setBounds (792, 48, 104, 24);
    
    addAndMakeVisible (pluginOpenButton5 = new TextButton (String()));
    pluginOpenButton5->addListener (this);
    pluginOpenButton5->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginOpenButton5->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginOpenButton5->setBounds (904, 48, 24, 24);
    
    addAndMakeVisible (pluginBypassButton5);
    pluginBypassButton5.addListener (this);
    pluginBypassButton5.setBounds (904, 16, 24, 24);
    
    // 6
    addAndMakeVisible (pluginLoadButton6 = new TextButton (String()));
    pluginLoadButton6->setButtonText (TRANS("load plugin"));
    pluginLoadButton6->addListener (this);
    pluginLoadButton6->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginLoadButton6->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginLoadButton6->setBounds (944, 16, 104, 24);
    
    addAndMakeVisible (pluginInfoButton6 = new TextEditor (String()));
    pluginInfoButton6->setMultiLine (false);
    pluginInfoButton6->setReturnKeyStartsNewLine (false);
    pluginInfoButton6->setReadOnly (true);
    pluginInfoButton6->setScrollbarsShown (true);
    pluginInfoButton6->setCaretVisible (false);
    pluginInfoButton6->setPopupMenuEnabled (true);
    pluginInfoButton6->setColour (TextEditor::backgroundColourId, Colour (0xff181f22));
    pluginInfoButton6->setColour (TextEditor::outlineColourId, Colour (0xff42a2c8));
    pluginInfoButton6->setText (String());
    pluginInfoButton6->setBounds (944, 48, 104, 24);
    
    addAndMakeVisible (pluginOpenButton6 = new TextButton (String()));
    pluginOpenButton6->addListener (this);
    pluginOpenButton6->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginOpenButton6->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginOpenButton6->setBounds (1056, 48, 24, 24);
    
    addAndMakeVisible (pluginBypassButton6);
    pluginBypassButton6.addListener (this);
    pluginBypassButton6.setBounds (1056, 16, 24, 24);
    
    // PLUGIN TITLE
    addAndMakeVisible (channelStripNameTitle = new Label (String(),TRANS("CHANNEL STRIP")));
    channelStripNameTitle->setFont (Font ("DIN Alternate", 20.80f, Font::plain));
    channelStripNameTitle->setJustificationType (Justification::centredLeft);
    channelStripNameTitle->setEditable (false, false, false);
    channelStripNameTitle->setColour (Label::textColourId, Colours::aliceblue);
    channelStripNameTitle->setColour (Label::outlineColourId, Colour (0x0042a2c8));
    channelStripNameTitle->setColour (TextEditor::textColourId, Colours::black);
    channelStripNameTitle->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    channelStripNameTitle->setBounds (1101, -7, 175, 67);
    
    addAndMakeVisible (analyserNameTitle = new Label (String(),TRANS("ANALYS3R")));
    analyserNameTitle->setFont (Font ("DIN Alternate", 23.30f, Font::plain).withExtraKerningFactor (0.214f));
    analyserNameTitle->setJustificationType (Justification::centredLeft);
    analyserNameTitle->setEditable (false, false, false);
    analyserNameTitle->setColour (Label::textColourId, Colours::aliceblue);
    analyserNameTitle->setColour (Label::outlineColourId, Colour (0x0042a2c8));
    analyserNameTitle->setColour (TextEditor::textColourId, Colours::black);
    analyserNameTitle->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    analyserNameTitle->setBounds (1101, 9, 175, 67);
    
}

void ChannelStripAnalyserAudioProcessorEditor::createParametersAttachments()
{
    sliderWaveformRangeAttach.reset            (new SliderAttachment(parameters, SLIDER_WAVEFORM_RANGE_ID,
                                                                    sliderWaveformRange));
    sliderWaveformGainAttach.reset         (new SliderAttachment(parameters, SLIDER_WAVEFORM_TIMEMODE_ID,
                                                                    sliderWaveformGain));
    sliderValueChanged (&sliderWaveformRange);
    sliderValueChanged (&sliderWaveformGain);

    //========================================================================================================
    sliderVectorscopeRangeAttach.reset        (new SliderAttachment(parameters, SLIDER_RANGE_VECTORSCOPE_ID,
                                                                    sliderVectorscopeRange));
    sliderVectorscopeVanishTimeAttach.reset   (new SliderAttachment(parameters, SLIDER_VANISH_TIME_ID,
                                                                    sliderVectorscopeVanishTime));
    sliderValueChanged (&sliderVectorscopeRange);
    sliderValueChanged (&sliderVectorscopeVanishTime);
    
    //========================================================================================================
    sliderSpectrumAnalyserRangeAttach.reset         (new SliderAttachment(parameters, SLIDER_RANGE_SPECTRUM_ANALYSER_ID,
                                                                    sliderSpectrumAnalyserRange));
    sliderSpectrumAnalyserReturnTimeAttach.reset    (new SliderAttachment(parameters, SLIDER_RETURN_TIME_ID,
                                                                    sliderSpectrumAnalyserReturnTime));
    sliderValueChanged (&sliderSpectrumAnalyserRange);
    sliderValueChanged (&sliderSpectrumAnalyserReturnTime);
    
    //========================================================================================================
    sliderSpectrumDifferenceRangeAttach.reset           (new SliderAttachment(parameters, SLIDER_RANGE_SPECTRUM_DIFFERENCE_ID,
                                                                    sliderSpectrumDifferenceRange));
    sliderSpectrumDifferenceTimeAverageAttach.reset     (new SliderAttachment(parameters, SLIDER_TIME_AVERAGE_ID,
                                                                              sliderSpectrumDifferenceTimeAverage));

    sliderValueChanged (&sliderSpectrumDifferenceRange);
    sliderValueChanged (&sliderSpectrumDifferenceTimeAverage);
    
    //========================================================================================================
    pluginBypassButton1Attach.reset           (new ButtonAttachment(parameters, BUTTON_PLUGINBYPASS_1_ID,pluginBypassButton1));
    pluginBypassButton2Attach.reset           (new ButtonAttachment(parameters, BUTTON_PLUGINBYPASS_2_ID,pluginBypassButton2));
    pluginBypassButton3Attach.reset           (new ButtonAttachment(parameters, BUTTON_PLUGINBYPASS_3_ID,pluginBypassButton3));
    pluginBypassButton4Attach.reset           (new ButtonAttachment(parameters, BUTTON_PLUGINBYPASS_4_ID,pluginBypassButton4));
    pluginBypassButton5Attach.reset           (new ButtonAttachment(parameters, BUTTON_PLUGINBYPASS_5_ID,pluginBypassButton5));
    pluginBypassButton6Attach.reset           (new ButtonAttachment(parameters, BUTTON_PLUGINBYPASS_6_ID,pluginBypassButton6));
    
    monoModeButtonAttach.reset                (new ButtonAttachment(parameters, "BUTTON_MONOMODE_ID", textButtonMonoMode));
    leftRightModeButtonAttach.reset           (new ButtonAttachment(parameters, "BUTTON_LRMODE_ID", textButtonLRMode));
    midSideButtonAttach.reset                 (new ButtonAttachment(parameters, "BUTTON_MSMODE_ID", textButtonMSMode));


}


//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Projucer information section --

    This is where the Projucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="ChannelStripAnalyserAudioProcessorEditor"
                 componentName="" parentClasses="public AudioProcessorEditor, public Timer"
                 constructorParams="ChannelStripAnalyserAudioProcessor&amp; p"
                 variableInitialisers="AudioProcessorEditor (&amp;p), processor (p)"
                 snapPixels="8" snapActive="1" snapShown="1" overlayOpacity="0.330"
                 fixedSize="0" initialWidth="1280" initialHeight="791">
  <BACKGROUND backgroundColour="11111111">
    <RECT pos="0 0 1280 792" fill="solid: ff111111" hasStroke="0"/>
    <ROUNDRECT pos="7 87 714 170" cornerSize="10.00000000000000000000" fill="solid: ee0e00"
               hasStroke="1" stroke="4, mitered, butt" strokeColour="solid: ff42a2c8"/>
    <ROUNDRECT pos="7 263 714 281" cornerSize="10.00000000000000000000" fill="solid: a52aa5"
               hasStroke="1" stroke="4, mitered, butt" strokeColour="solid: ff42a2c8"/>
    <ROUNDRECT pos="7 550 714 236" cornerSize="10.00000000000000000000" fill="solid: a52aa5"
               hasStroke="1" stroke="4, mitered, butt" strokeColour="solid: ff42a2c8"/>
    <ROUNDRECT pos="727 87 350 350" cornerSize="10.00000000000000000000" fill="solid: a52aa5"
               hasStroke="1" stroke="4, mitered, butt" strokeColour="solid: ff42a2c8"/>
    <ROUNDRECT pos="1083 87 190 700" cornerSize="10.00000000000000000000" fill="solid: a52aa5"
               hasStroke="1" stroke="4, mitered, butt" strokeColour="solid: ff42a2c8"/>
    <ROUNDRECT pos="727 443 350 343" cornerSize="10.00000000000000000000" fill="solid: ff0f0f1c"
               hasStroke="0"/>
    <ROUNDRECT pos="727 443 350 343" cornerSize="10.00000000000000000000" fill="solid: a52aa5"
               hasStroke="1" stroke="4, mitered, butt" strokeColour="solid: ff42a2c8"/>
    <ROUNDRECT pos="7 5 1267 76" cornerSize="10.00000000000000000000" fill="solid: ff0f0f1c"
               hasStroke="0"/>
    <ROUNDRECT pos="7 5 1267 76" cornerSize="10.00000000000000000000" fill="solid: ee0e00"
               hasStroke="1" stroke="4, mitered, butt" strokeColour="solid: ff42a2c8"/>
  </BACKGROUND>
  <SLIDER name="" id="1306915508c9e4d5" memberName="sliderRangeVectorscope"
          virtualName="" explicitFocusOrder="0" pos="744 671 72 16" min="0.00000000000000000000"
          max="10.00000000000000000000" int="0.00000000000000000000" style="LinearHorizontal"
          textBoxPos="NoTextBox" textBoxEditable="1" textBoxWidth="80"
          textBoxHeight="20" skewFactor="1.00000000000000000000" needsCallback="1"/>
  <SLIDER name="" id="85d5f82187d3df11" memberName="sliderVanishTime" virtualName=""
          explicitFocusOrder="0" pos="744 703 72 16" min="0.00000000000000000000"
          max="10.00000000000000000000" int="0.00000000000000000000" style="LinearHorizontal"
          textBoxPos="NoTextBox" textBoxEditable="1" textBoxWidth="80"
          textBoxHeight="20" skewFactor="1.00000000000000000000" needsCallback="1"/>
  <SLIDER name="" id="88d0995e8e7c739c" memberName="sliderRangeSpectrumAnalyser"
          virtualName="" explicitFocusOrder="0" pos="904 487 72 16" min="0.00000000000000000000"
          max="10.00000000000000000000" int="0.00000000000000000000" style="LinearHorizontal"
          textBoxPos="NoTextBox" textBoxEditable="1" textBoxWidth="80"
          textBoxHeight="20" skewFactor="1.00000000000000000000" needsCallback="1"/>
  <SLIDER name="" id="e8b24c22a79f5ea9" memberName="sliderReturnTime" virtualName=""
          explicitFocusOrder="0" pos="904 519 72 16" min="0.00000000000000000000"
          max="10.00000000000000000000" int="0.00000000000000000000" style="LinearHorizontal"
          textBoxPos="NoTextBox" textBoxEditable="1" textBoxWidth="80"
          textBoxHeight="20" skewFactor="1.00000000000000000000" needsCallback="1"/>
  <SLIDER name="" id="aadbd430fa2220ef" memberName="sliderCurbeOffset"
          virtualName="" explicitFocusOrder="0" pos="744 487 72 16" min="0.00000000000000000000"
          max="10.00000000000000000000" int="0.00000000000000000000" style="LinearHorizontal"
          textBoxPos="NoTextBox" textBoxEditable="1" textBoxWidth="80"
          textBoxHeight="20" skewFactor="1.00000000000000000000" needsCallback="1"/>
  <SLIDER name="" id="94d3edbd69cb80fb" memberName="sliderDifferenceGain"
          virtualName="" explicitFocusOrder="0" pos="744 519 72 16" min="0.00000000000000000000"
          max="10.00000000000000000000" int="0.00000000000000000000" style="LinearHorizontal"
          textBoxPos="NoTextBox" textBoxEditable="1" textBoxWidth="80"
          textBoxHeight="20" skewFactor="1.00000000000000000000" needsCallback="1"/>
  <SLIDER name="" id="4763bbe89ddfa2d1" memberName="sliderRMSWindow" virtualName=""
          explicitFocusOrder="0" pos="744 551 72 16" min="0.00000000000000000000"
          max="10.00000000000000000000" int="0.00000000000000000000" style="LinearHorizontal"
          textBoxPos="NoTextBox" textBoxEditable="1" textBoxWidth="80"
          textBoxHeight="20" skewFactor="1.00000000000000000000" needsCallback="1"/>
  <SLIDER name="" id="92f0316b180882a5" memberName="sliderZoomX" virtualName=""
          explicitFocusOrder="0" pos="744 583 72 16" min="0.00000000000000000000"
          max="10.00000000000000000000" int="0.00000000000000000000" style="LinearHorizontal"
          textBoxPos="NoTextBox" textBoxEditable="1" textBoxWidth="80"
          textBoxHeight="20" skewFactor="1.00000000000000000000" needsCallback="1"/>
  <SLIDER name="" id="48d0430ee39140ba" memberName="sliderZoomY" virtualName=""
          explicitFocusOrder="0" pos="744 615 72 16" min="0.00000000000000000000"
          max="10.00000000000000000000" int="0.00000000000000000000" style="LinearHorizontal"
          textBoxPos="NoTextBox" textBoxEditable="1" textBoxWidth="80"
          textBoxHeight="20" skewFactor="1.00000000000000000000" needsCallback="1"/>
  <SLIDER name="new slider" id="6027388c599159fd" memberName="sliderTimeAverage"
          virtualName="" explicitFocusOrder="0" pos="904 615 72 16" min="0.00000000000000000000"
          max="10.00000000000000000000" int="0.00000000000000000000" style="LinearHorizontal"
          textBoxPos="NoTextBox" textBoxEditable="1" textBoxWidth="80"
          textBoxHeight="20" skewFactor="1.00000000000000000000" needsCallback="1"/>
  <SLIDER name="" id="c72be6355f8b9b1d" memberName="sliderRangeSpectrumDifference"
          virtualName="" explicitFocusOrder="0" pos="904 583 72 16" min="0.00000000000000000000"
          max="10.00000000000000000000" int="0.00000000000000000000" style="LinearHorizontal"
          textBoxPos="NoTextBox" textBoxEditable="1" textBoxWidth="80"
          textBoxHeight="20" skewFactor="1.00000000000000000000" needsCallback="1"/>
  <SLIDER name="" id="8c16e1ec097135d8" memberName="sliderLevelMeter2"
          virtualName="" explicitFocusOrder="0" pos="904 703 72 16" min="0.00000000000000000000"
          max="10.00000000000000000000" int="0.00000000000000000000" style="LinearHorizontal"
          textBoxPos="NoTextBox" textBoxEditable="1" textBoxWidth="80"
          textBoxHeight="20" skewFactor="1.00000000000000000000" needsCallback="1"/>
  <SLIDER name="" id="8c00c345c9cc15da" memberName="sliderLevelMeter1"
          virtualName="" explicitFocusOrder="0" pos="904 671 72 16" min="0.00000000000000000000"
          max="10.00000000000000000000" int="0.00000000000000000000" style="LinearHorizontal"
          textBoxPos="NoTextBox" textBoxEditable="1" textBoxWidth="80"
          textBoxHeight="20" skewFactor="1.00000000000000000000" needsCallback="1"/>
  <LABEL name="" id="abfebc7838fc08fd" memberName="curveOffsetLabelText"
         virtualName="" explicitFocusOrder="0" pos="816 482 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="curve offset&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Avenir Next"
         fontsize="15.00000000000000000000" kerning="0.00000000000000000000"
         bold="0" italic="0" justification="33"/>
  <LABEL name="" id="8ac4dc36cd056bc5" memberName="differenceGainLabelText"
         virtualName="" explicitFocusOrder="0" pos="816 514 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="difference gain&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Avenir Next"
         fontsize="15.00000000000000000000" kerning="0.00000000000000000000"
         bold="0" italic="0" justification="33"/>
  <LABEL name="" id="3f13d431adcb9c61" memberName="rmsWindowLabelText"
         virtualName="" explicitFocusOrder="0" pos="816 546 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="RMS window&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Avenir Next"
         fontsize="15.00000000000000000000" kerning="0.00000000000000000000"
         bold="0" italic="0" justification="33"/>
  <LABEL name="new label" id="59849cdc5842f78b" memberName="zoomxLabelText"
         virtualName="" explicitFocusOrder="0" pos="816 578 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="zoom X&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Avenir Next"
         fontsize="15.00000000000000000000" kerning="0.00000000000000000000"
         bold="0" italic="0" justification="33"/>
  <LABEL name="new label" id="dddf836789593d9c" memberName="zoomyLabelText"
         virtualName="" explicitFocusOrder="0" pos="816 610 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="zoom Y&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Avenir Next"
         fontsize="15.00000000000000000000" kerning="0.00000000000000000000"
         bold="0" italic="0" justification="33"/>
  <LABEL name="" id="f66d208dc4387026" memberName="waveformNameTitle"
         virtualName="" explicitFocusOrder="0" pos="745 455 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="WAVEFORM" editableSingleClick="0" editableDoubleClick="0"
         focusDiscardsChanges="0" fontname="DIN Alternate" fontsize="15.00000000000000000000"
         kerning="0.00000000000000000000" bold="1" italic="0" justification="33"
         typefaceStyle="Bold"/>
  <LABEL name="" id="c2b07ad15dab4c2e" memberName="spectrumAnalyserNameTitle"
         virtualName="" explicitFocusOrder="0" pos="904 456 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="SPECTRUM ANALYSER&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="DIN Alternate"
         fontsize="15.00000000000000000000" kerning="0.00000000000000000000"
         bold="1" italic="0" justification="33" typefaceStyle="Bold"/>
  <LABEL name="" id="423b683f6aa9d633" memberName="spectrumDifferenceNameTitle"
         virtualName="" explicitFocusOrder="0" pos="904 552 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="SPECTRUM DIFFERENCE&#10;&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="DIN Alternate"
         fontsize="15.00000000000000000000" kerning="0.00000000000000000000"
         bold="1" italic="0" justification="33" typefaceStyle="Bold"/>
  <LABEL name="" id="f91ba101ae18390f" memberName="levelMeterNameTitle"
         virtualName="" explicitFocusOrder="0" pos="904 640 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="LEVEL METER&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="DIN Alternate"
         fontsize="15.00000000000000000000" kerning="0.00000000000000000000"
         bold="1" italic="0" justification="33" typefaceStyle="Bold"/>
  <LABEL name="" id="c1f0977bbd0536a" memberName="vectorscopeNameTitle"
         virtualName="" explicitFocusOrder="0" pos="744 640 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="VECTORSCOPE&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="DIN Alternate"
         fontsize="15.00000000000000000000" kerning="0.00000000000000000000"
         bold="1" italic="0" justification="33" typefaceStyle="Bold"/>
  <LABEL name="" id="173ac02db1cb7acd" memberName="rangeVectorscopLabelText"
         virtualName="" explicitFocusOrder="0" pos="816 666 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="range&#10;" editableSingleClick="0" editableDoubleClick="0"
         focusDiscardsChanges="0" fontname="Avenir Next" fontsize="15.00000000000000000000"
         kerning="0.00000000000000000000" bold="0" italic="0" justification="33"/>
  <LABEL name="" id="895151c69756682a" memberName="vanishTimeLabelText"
         virtualName="" explicitFocusOrder="0" pos="816 698 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="vanish time&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Avenir Next"
         fontsize="15.00000000000000000000" kerning="0.00000000000000000000"
         bold="0" italic="0" justification="33"/>
  <LABEL name="" id="9b4eedc1d09c7957" memberName="rangeVectorscopeLabelText"
         virtualName="" explicitFocusOrder="0" pos="976 482 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="range&#10;" editableSingleClick="0" editableDoubleClick="0"
         focusDiscardsChanges="0" fontname="Avenir Next" fontsize="15.00000000000000000000"
         kerning="0.00000000000000000000" bold="0" italic="0" justification="33"/>
  <LABEL name="" id="fe5a5eb0d82508b5" memberName="returnTimeLabelText"
         virtualName="" explicitFocusOrder="0" pos="976 514 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="return time&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Avenir Next"
         fontsize="15.00000000000000000000" kerning="0.00000000000000000000"
         bold="0" italic="0" justification="33"/>
  <LABEL name="" id="93af561d43e6b33c" memberName="rangeSpectrumLabelText"
         virtualName="" explicitFocusOrder="0" pos="976 578 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="range&#10;" editableSingleClick="0" editableDoubleClick="0"
         focusDiscardsChanges="0" fontname="Avenir Next" fontsize="15.00000000000000000000"
         kerning="0.00000000000000000000" bold="0" italic="0" justification="33"/>
  <LABEL name="" id="3a1d37ca8b10cc0b" memberName="timeAveargeLabelText"
         virtualName="" explicitFocusOrder="0" pos="976 610 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="time average&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Avenir Next"
         fontsize="15.00000000000000000000" kerning="0.00000000000000000000"
         bold="0" italic="0" justification="33"/>
  <TEXTEDITOR name="" id="c434f79946316145" memberName="textEditorTotalDelay"
              virtualName="" explicitFocusOrder="0" pos="752 736 80 24" bkgcol="ff181f22"
              outlinecol="ff42a2c8" initialText="" multiline="0" retKeyStartsLine="0"
              readonly="1" scrollbars="1" caret="0" popupmenu="1"/>
  <LABEL name="" id="d5515713f6adad83" memberName="totalDelayLabelText"
         virtualName="" explicitFocusOrder="0" pos="840 736 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="total delay&#10;" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Avenir Next"
         fontsize="15.00000000000000000000" kerning="0.00000000000000000000"
         bold="0" italic="0" justification="33"/>
  <TEXTBUTTON name="" id="a82280cfa6f2ad67" memberName="textButtonApplyDelay"
              virtualName="" explicitFocusOrder="0" pos="936 732 96 32" bgColOff="ff42a2c8"
              buttonText="apply delay compensation" connectedEdges="0" needsCallback="1"
              radioGroupId="0"/>
  <TEXTBUTTON name="" id="73ebcfa336f682a" memberName="pluginsManagerButton"
              virtualName="" explicitFocusOrder="0" pos="17 16 143 32" bgColOff="ff42a2c8"
              bgColOn="ff181f22" buttonText="plugins manager" connectedEdges="0"
              needsCallback="1" radioGroupId="0"/>
  <TEXTBUTTON name="" id="2b7983a3c831f59b" memberName="pluginLoadButton1"
              virtualName="" explicitFocusOrder="0" pos="184 16 104 24" bgColOff="ff42a2c8"
              bgColOn="ff181f22" buttonText="load plugin" connectedEdges="0"
              needsCallback="1" radioGroupId="0"/>
  <TEXTEDITOR name="" id="4da8d1e971e7b467" memberName="pluginInfoButton1"
              virtualName="" explicitFocusOrder="0" pos="184 48 104 24" bkgcol="ff181f22"
              outlinecol="ff42a2c8" initialText="" multiline="0" retKeyStartsLine="0"
              readonly="1" scrollbars="1" caret="0" popupmenu="1"/>
  <TEXTBUTTON name="" id="4543194b26ef7172" memberName="pluginOpenButton1"
              virtualName="" explicitFocusOrder="0" pos="296 48 24 24" bgColOff="ff42a2c8"
              bgColOn="ff181f22" buttonText="" connectedEdges="0" needsCallback="1"
              radioGroupId="0"/>
  <TOGGLEBUTTON name="" id="8282dff73c9bd79b" memberName="pluginBypassButton1"
                virtualName="" explicitFocusOrder="0" pos="296 16 24 24" buttonText=""
                connectedEdges="0" needsCallback="1" radioGroupId="0" state="0"/>
  <TEXTBUTTON name="" id="9c3274c6310af6ee" memberName="pluginLoadButton2"
              virtualName="" explicitFocusOrder="0" pos="336 16 104 24" bgColOff="ff42a2c8"
              bgColOn="ff181f22" buttonText="load plugin" connectedEdges="0"
              needsCallback="1" radioGroupId="0"/>
  <TEXTEDITOR name="" id="b365bb4400742d82" memberName="pluginInfoButton2"
              virtualName="" explicitFocusOrder="0" pos="336 48 104 24" bkgcol="ff181f22"
              outlinecol="ff42a2c8" initialText="" multiline="0" retKeyStartsLine="0"
              readonly="1" scrollbars="1" caret="0" popupmenu="1"/>
  <TEXTBUTTON name="" id="608902c80f8bd6bb" memberName="pluginOpenButton2"
              virtualName="" explicitFocusOrder="0" pos="448 48 24 24" bgColOff="ff42a2c8"
              bgColOn="ff181f22" buttonText="" connectedEdges="0" needsCallback="1"
              radioGroupId="0"/>
  <TOGGLEBUTTON name="" id="224c446e1aa1358b" memberName="pluginBypassButton2"
                virtualName="" explicitFocusOrder="0" pos="448 16 24 24" buttonText=""
                connectedEdges="0" needsCallback="1" radioGroupId="0" state="0"/>
  <TEXTBUTTON name="" id="99b820ec0a70135d" memberName="pluginLoadButton3"
              virtualName="" explicitFocusOrder="0" pos="488 16 104 24" bgColOff="ff42a2c8"
              bgColOn="ff181f22" buttonText="load plugin" connectedEdges="0"
              needsCallback="1" radioGroupId="0"/>
  <TEXTEDITOR name="" id="6058013e17918728" memberName="pluginInfoButton3"
              virtualName="" explicitFocusOrder="0" pos="488 48 104 24" bkgcol="ff181f22"
              outlinecol="ff42a2c8" initialText="" multiline="0" retKeyStartsLine="0"
              readonly="1" scrollbars="1" caret="0" popupmenu="1"/>
  <TEXTBUTTON name="" id="13944432f49e74f7" memberName="pluginOpenButton3"
              virtualName="" explicitFocusOrder="0" pos="600 48 24 24" bgColOff="ff42a2c8"
              bgColOn="ff181f22" buttonText="" connectedEdges="0" needsCallback="1"
              radioGroupId="0"/>
  <TOGGLEBUTTON name="" id="b1026fa390bc5519" memberName="pluginBypassButton3"
                virtualName="" explicitFocusOrder="0" pos="600 16 24 24" buttonText=""
                connectedEdges="0" needsCallback="1" radioGroupId="0" state="0"/>
  <TEXTBUTTON name="" id="e1a16db4765af2c3" memberName="pluginLoadButton4"
              virtualName="" explicitFocusOrder="0" pos="640 16 104 24" bgColOff="ff42a2c8"
              bgColOn="ff181f22" buttonText="load plugin" connectedEdges="0"
              needsCallback="1" radioGroupId="0"/>
  <TEXTEDITOR name="" id="67161246ed6c7b9" memberName="pluginInfoButton4" virtualName=""
              explicitFocusOrder="0" pos="640 48 104 24" bkgcol="ff181f22"
              outlinecol="ff42a2c8" initialText="" multiline="0" retKeyStartsLine="0"
              readonly="1" scrollbars="1" caret="0" popupmenu="1"/>
  <TEXTBUTTON name="" id="f1d0d67641fe40bf" memberName="pluginOpenButton4"
              virtualName="" explicitFocusOrder="0" pos="752 48 24 24" bgColOff="ff42a2c8"
              bgColOn="ff181f22" buttonText="" connectedEdges="0" needsCallback="1"
              radioGroupId="0"/>
  <TOGGLEBUTTON name="" id="32d2c66ca9c7b140" memberName="pluginBypassButton4"
                virtualName="" explicitFocusOrder="0" pos="752 16 24 24" buttonText=""
                connectedEdges="0" needsCallback="1" radioGroupId="0" state="0"/>
  <TEXTBUTTON name="" id="1541f59fa6b5d060" memberName="pluginLoadButton5"
              virtualName="" explicitFocusOrder="0" pos="792 16 104 24" bgColOff="ff42a2c8"
              bgColOn="ff181f22" buttonText="load plugin" connectedEdges="0"
              needsCallback="1" radioGroupId="0"/>
  <TEXTEDITOR name="" id="ca5670628c1dd119" memberName="pluginInfoButton5"
              virtualName="" explicitFocusOrder="0" pos="792 48 104 24" bkgcol="ff181f22"
              outlinecol="ff42a2c8" initialText="" multiline="0" retKeyStartsLine="0"
              readonly="1" scrollbars="1" caret="0" popupmenu="1"/>
  <TEXTBUTTON name="" id="a188020316446c4b" memberName="pluginOpenButton5"
              virtualName="" explicitFocusOrder="0" pos="904 48 24 24" bgColOff="ff42a2c8"
              bgColOn="ff181f22" buttonText="" connectedEdges="0" needsCallback="1"
              radioGroupId="0"/>
  <TOGGLEBUTTON name="" id="2598a67a33f04694" memberName="pluginBypassButton5"
                virtualName="" explicitFocusOrder="0" pos="904 16 24 24" buttonText=""
                connectedEdges="0" needsCallback="1" radioGroupId="0" state="0"/>
  <TEXTBUTTON name="" id="4b8e171eb486af58" memberName="pluginLoadButton6"
              virtualName="" explicitFocusOrder="0" pos="944 16 104 24" bgColOff="ff42a2c8"
              bgColOn="ff181f22" buttonText="load plugin" connectedEdges="0"
              needsCallback="1" radioGroupId="0"/>
  <TEXTEDITOR name="" id="ddef1395d2f50d69" memberName="pluginInfoButton6"
              virtualName="" explicitFocusOrder="0" pos="944 48 104 24" bkgcol="ff181f22"
              outlinecol="ff42a2c8" initialText="" multiline="0" retKeyStartsLine="0"
              readonly="1" scrollbars="1" caret="0" popupmenu="1"/>
  <TEXTBUTTON name="" id="549b3a6b75612629" memberName="pluginOpenButton6"
              virtualName="" explicitFocusOrder="0" pos="1056 48 24 24" bgColOff="ff42a2c8"
              bgColOn="ff181f22" buttonText="" connectedEdges="0" needsCallback="1"
              radioGroupId="0"/>
  <TOGGLEBUTTON name="" id="f0799897e2a87403" memberName="pluginBypassButton6"
                virtualName="" explicitFocusOrder="0" pos="1056 16 24 24" buttonText=""
                connectedEdges="0" needsCallback="1" radioGroupId="0" state="0"/>
  <LABEL name="" id="3733f67e43241f7c" memberName="channelStripNameTitle"
         virtualName="" explicitFocusOrder="0" pos="1101 -7 175 67" textCol="fff0f8ff"
         outlineCol="42a2c8" edTextCol="ff000000" edBkgCol="0" labelText="CHANNEL STRIP"
         editableSingleClick="0" editableDoubleClick="0" focusDiscardsChanges="0"
         fontname="DIN Alternate" fontsize="20.80000000000000071054" kerning="0.00000000000000000000"
         bold="0" italic="0" justification="33"/>
  <LABEL name="" id="e7e67d294827e58a" memberName="analyserNameTitle"
         virtualName="" explicitFocusOrder="0" pos="1101 9 175 67" textCol="fff0f8ff"
         outlineCol="42a2c8" edTextCol="ff000000" edBkgCol="0" labelText="ANALYSER"
         editableSingleClick="0" editableDoubleClick="0" focusDiscardsChanges="0"
         fontname="DIN Alternate" fontsize="23.30000000000000071054" kerning="0.21399999999999999578"
         bold="0" italic="0" justification="33"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]
