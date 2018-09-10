#include "PluginEditor.h"

//[MiscUserDefs] You can add your own user definitions and misc code here...
//==============================================================================
class ChannelStripAnalyserAudioProcessorEditor::PluginListWindow   : public DocumentWindow
{
public:
    PluginListWindow(ChannelStripAnalyserAudioProcessorEditor& owner)
    : DocumentWindow("Available Plugins", Colours::darkseagreen
                     ,DocumentWindow::closeButton
                     ),
    owner(owner)
    {
        setResizable(false, false);
        
        std::unique_ptr <File> deadMansPedalFile(new File(owner.processor.applicationProperties.
                                                                          getUserSettings()->
                                                                          getFile().getSiblingFile("RecentlyCrashedPluginsList")));
        
        
        this->setTitleBarButtonsRequired(4, true);
        
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
        (stereoAnalyser     = new StereoAnalyser     (sampleRate, fftSize, 15, 200, mainAudioBufferSystem, forwFFT));
    addAndMakeVisible
        (waveformAnalyser   = new WaveformAnalyser   (sampleRate, fftSize, 8, 0, 120, mainAudioBufferSystem));
    
    createParametersAttachments();
    
    pluginWinState   = false;
    pluginListWindow = new PluginListWindow (*this);
    
    editors.ensureStorageAllocated(6);
    for (auto i=0; i<6; i++)
    {
        editors.add(nullptr);
    }
    
    Timer::startTimer(50);
    diffcount.store(0);
//    HighResolutionTimer::startTimer(43);
}

ChannelStripAnalyserAudioProcessorEditor::~ChannelStripAnalyserAudioProcessorEditor()
{
    HighResolutionTimer::stopTimer();
}

void ChannelStripAnalyserAudioProcessorEditor::hiResTimerCallback()
{
//    cancelPendingUpdate();
    mainAudioBufferSystem.pushAudioBufferIntoHistoryBuffer();
    triggerAsyncUpdate();
    ++diffcount;

}
void ChannelStripAnalyserAudioProcessorEditor::handleAsyncUpdate()
{
//    spectrumAnalyser   -> createFrame();
//    spectrumDifference -> createFrame();
//    phaseDifference    -> createFrame();
//    stereoAnalyser     -> createFrame();
//    waveformAnalyser   -> createFrame();
    --diffcount;
//    spectrumAnalyser   -> repaint();
//    spectrumDifference -> repaint();
//    phaseDifference    -> repaint();
//    stereoAnalyser     -> repaint();
    waveformAnalyser   -> repaint();
}

void ChannelStripAnalyserAudioProcessorEditor::timerCallback()
{
    if ( ! HighResolutionTimer::isTimerRunning())
        HighResolutionTimer::startTimer(43);
    // checking flag states for plugin deletion, creation and re-creation
    for(auto i=0; i<6; i++)
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
        if (processor.plugIns[i]) processor.shownPlugins[i] = editors[i]->isVisible();

    }
//    --diffcount;
//    spectrumAnalyser   -> repaint();
//    spectrumDifference -> repaint();
//    phaseDifference    -> repaint();
//    stereoAnalyser     -> repaint();
//    waveformAnalyser   -> repaint();

    // needs to block audioThread in order to get last delay in graph data
    // processor.graph.prepareToPlay (processor.asampleRate.load(), processor.ablockSize.load());
    String totalDelay   = (String) ( processor.graph.getLatencySamples());
    float position = processor.currentPosition.ppqPosition;
    float rest = std::fmod(position, 4);
    String playPosition = (String) ( rest );
    String numOfNewPixels   = (String) ( waveformAnalyser->numOfNewPixelsToPaint);
    String diffCount   = (String) ( diffcount.load());
    textEditorTotalDelay -> setText(diffCount);
    //repaintVisualizers();
    
    

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
//==============================================================================
//==============================================================================
void ChannelStripAnalyserAudioProcessorEditor::paint (Graphics& g)
{
    g.drawImage (backgroundImage, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
    //repaintVisualizers();
}

void ChannelStripAnalyserAudioProcessorEditor::createBackgroundUi()
{
    backgroundImage = Image(Image::PixelFormat::ARGB, getWidth(), getHeight(), true);
    Graphics g (backgroundImage);
    
    g.fillAll (Colour (0x11111111));
    {
        int x = 0, y = 0, width = 1280, height = 792;
        Colour fillColour = Colour (0xff111111);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.fillRect (x, y, width, height);
    }
    
    {
        // WAVEFORM
        float x = 7.0f, y = 87.0f, width = 714.0f, height = 170.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArgum ents]
        g.setColour (strokeColour);
        g.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    
    {
        // SPECTRUM ANALYSER
        float x = 7.0f, y = 263.0f, width = 714.0f, height = 281.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (strokeColour);
        g.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    
    {
        // SPECTRUM DIFF / PHASE
        float x = 7.0f, y = 550.0f, width = 714.0f, height = 236.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (strokeColour);
        g.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    
    {
        // STEREO VIEW
        float x = 727.0f, y = 87.0f, width = 350.0f, height = 350.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (strokeColour);
        g.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    
    {
        float x = 1083.0f, y = 87.0f, width = 190.0f, height = 700.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (strokeColour);
        g.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    
    {
        float x = 727.0f, y = 443.0f, width = 350.0f, height = 343.0f;
        Colour fillColour = Colour (0xff0f0f1c);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.fillRoundedRectangle (x, y, width, height, 10.000f);
    }
    
    {
        float x = 727.0f, y = 443.0f, width = 350.0f, height = 343.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (strokeColour);
        g.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    
    {
        float x = 7.0f, y = 5.0f, width = 1267.0f, height = 76.0f;
        Colour fillColour = Colour (0xff0f0f1c);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.fillRoundedRectangle (x, y, width, height, 10.000f);
    }
    
    {
        float x = 7.0f, y = 5.0f, width = 1267.0f, height = 76.0f;
        Colour strokeColour = Colour (0xff42a2c8);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (strokeColour);
        g.drawRoundedRectangle (x, y, width, height, 10.000f, 4.000f);
    }
    
    //[UserPaint] Add your own custom painting code here..
    g.setColour (Colours::white);
    g.setFont (55.0f);
    
}

void ChannelStripAnalyserAudioProcessorEditor::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]
    
    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}

void ChannelStripAnalyserAudioProcessorEditor::sliderValueChanged (Slider* sliderThatWasMoved)
{
    //[UsersliderValueChanged_Pre]
    //==============================================================================
    if (sliderThatWasMoved == &sliderRangeVectorscope)
    {
        //[UserSliderCode_sliderRangeVectorscope] -- add your slider handling code here..
        //[/UserSliderCode_sliderRangeVectorscope]
    }
    else if (sliderThatWasMoved == &sliderVanishTime)
    {
        //[UserSliderCode_sliderVanishTime] -- add your slider handling code here..
        //[/UserSliderCode_sliderVanishTime]
    }
    else if (sliderThatWasMoved == &sliderCurbeOffset)
    {
        //[UserSliderCode_sliderCurbeOffset] -- add your slider handling code here..
        //[/UserSliderCode_sliderCurbeOffset]
    }
    else if (sliderThatWasMoved == &sliderDifferenceGain)
    {
        //[UserSliderCode_sliderDifferenceGain] -- add your slider handling code here..
        //[/UserSliderCode_sliderDifferenceGain]
    }
    else if (sliderThatWasMoved == &sliderRMSWindow)
    {
        //[UserSliderCode_sliderRMSWindow] -- add your slider handling code here..
        //[/UserSliderCode_sliderRMSWindow]
    }
    else if (sliderThatWasMoved == &sliderZoomX)
    {
        //[UserSliderCode_sliderZoomX] -- add your slider handling code here..
        //[/UserSliderCode_sliderZoomX]
    }
    else if (sliderThatWasMoved == &sliderZoomY)
    {
        //[UserSliderCode_sliderZoomY] -- add your slider handling code here..
        //[/UserSliderCode_sliderZoomY]
    }
    
    //==============================================================================
    else if (sliderThatWasMoved == &sliderRangeSpectrumAnalyser)
    {
        spectrumAnalyser -> scaleModeAt.store((int) sliderRangeSpectrumAnalyser.getValue());
        spectrumAnalyser -> axisNeedsUpdate.store(true);
    }
    else if (sliderThatWasMoved == &sliderReturnTime)
    {
        switch ((int)sliderReturnTime.getValue()) {
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
    else if (sliderThatWasMoved == &sliderTimeAverage)
    {
        //[UserSliderCode_sliderTimeAverage] -- add your slider handling code here..
        switch ((int)sliderTimeAverage.getValue()) {
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
    else if (sliderThatWasMoved == &sliderRangeSpectrumDifference)
    {
        //[UserSliderCode_sliderRangeSpectrumDifference] -- add your slider handling code here..
        spectrumDifference -> scaleModeAt.store((int) sliderRangeSpectrumDifference.getValue());
        spectrumDifference -> axisNeedsUpdate.store(true);
        //[/UserSliderCode_sliderRangeSpectrumDifference]
    }
    
    //==============================================================================
    else if (sliderThatWasMoved == &sliderLevelMeter2)
    {
        //[UserSliderCode_sliderLevelMeter2] -- add your slider handling code here..
        //[/UserSliderCode_sliderLevelMeter2]
    }
    else if (sliderThatWasMoved == &sliderLevelMeter1)
    {
        //[UserSliderCode_sliderLevelMeter1] -- add your slider handling code here..
        //[/UserSliderCode_sliderLevelMeter1]
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
    if      (buttonThatWasClicked ==& textButtonMonoMode)
    {
        spectrumDifference -> analyseMode.store (1);
        phaseDifference    -> analyseMode.store (1);
        
        textButtonLRMode   .setToggleState(false, juce::NotificationType::dontSendNotification);
        textButtonMSMode   .setToggleState(false, juce::NotificationType::dontSendNotification);
    }
    else if (buttonThatWasClicked ==& textButtonLRMode)
    {
        spectrumDifference -> analyseMode.store (2);
        phaseDifference    -> analyseMode.store (2);
        
        textButtonMonoMode .setToggleState(false, juce::NotificationType::dontSendNotification);
        textButtonMSMode   .setToggleState(false, juce::NotificationType::dontSendNotification);
    }
    else if (buttonThatWasClicked ==& textButtonMSMode)
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
        if (processor.plugIns[0]) editors[0]->setVisible(true);
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
        if (processor.plugIns[1]) editors[1]->setVisible(true);
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
        if (processor.plugIns[2]) editors[2]->setVisible(true);
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
        if (processor.plugIns[3]) editors[3]->setVisible(true);
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
        if (processor.plugIns[4]) editors[4]->setVisible(true);
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
        if (processor.plugIns[5]) editors[5]->setVisible(true);
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
void ChannelStripAnalyserAudioProcessorEditor::canVisualizersPaint(bool state)
{
//    spectrumAnalyser   -> canPaint.store(state);
//    spectrumDifference -> canPaint.store(state);
//    phaseDifference    -> canPaint.store(state);
//    stereoAnalyser     -> canPaint.store(state);
//    waveformAnalyser   -> canPaint.store(state);
}

void ChannelStripAnalyserAudioProcessorEditor::setBuffersForVisualizers()
{
//    spectrumAnalyser   -> setBuffers (processor.getAudioBuffers(0), processor.getAudioBuffers(1) );
//    spectrumDifference -> setBuffers (processor.getAudioBuffers(0), processor.getAudioBuffers(1) );
//    phaseDifference    -> setBuffers (processor.getAudioBuffers(0), processor.getAudioBuffers(1) );
//    stereoAnalyser     -> setBuffers (processor.getAudioBuffers(0), processor.getAudioBuffers(1) );
//    waveformAnalyser   -> setBuffers (processor.getAudioBuffers(0), processor.getAudioBuffers(1) );
}

void ChannelStripAnalyserAudioProcessorEditor::repaintVisualizers()
{
//    spectrumAnalyser   -> lastWrittenSize.  store (processor.lastWrittenSize.load());
//    spectrumAnalyser   -> lastWrittenIndex. store (processor.lastWrittenIndex.load());
//    spectrumAnalyser   -> repaint();
    
//    spectrumDifference -> repaint();
//    phaseDifference    -> repaint();
//    stereoAnalyser     -> repaint();
//    waveformAnalyser   -> repaint();

}

void ChannelStripAnalyserAudioProcessorEditor::addUiElements()
{
    
    addAndMakeVisible (sliderRangeVectorscope);
    sliderRangeVectorscope.setRange (0, 10, 0);
    sliderRangeVectorscope.setSliderStyle (Slider::LinearHorizontal);
    sliderRangeVectorscope.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderRangeVectorscope.addListener (this);
    sliderRangeVectorscope.setBounds (744, 671, 72, 16);
    
    addAndMakeVisible (sliderVanishTime);
    sliderVanishTime.setRange (0, 10, 0);
    sliderVanishTime.setSliderStyle (Slider::LinearHorizontal);
    sliderVanishTime.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderVanishTime.addListener (this);
    sliderVanishTime.setBounds (744, 703, 72, 16);
    
    addAndMakeVisible (sliderRangeSpectrumAnalyser);
    sliderRangeSpectrumAnalyser.setRange (0, 3, 1);
    sliderRangeSpectrumAnalyser.setSliderStyle (Slider::LinearHorizontal);
    sliderRangeSpectrumAnalyser.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderRangeSpectrumAnalyser.addListener (this);
    sliderRangeSpectrumAnalyser.setBounds (904, 487, 72, 16);
    
    addAndMakeVisible (sliderReturnTime);
    sliderReturnTime.setRange (0, 10, 0);
    sliderReturnTime.setSliderStyle (Slider::LinearHorizontal);
    sliderReturnTime.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderReturnTime.addListener (this);
    sliderReturnTime.setBounds (904, 519, 72, 16);
    
    addAndMakeVisible (sliderCurbeOffset);
    sliderCurbeOffset.setRange (0, 10, 0);
    sliderCurbeOffset.setSliderStyle (Slider::LinearHorizontal);
    sliderCurbeOffset.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderCurbeOffset.addListener (this);
    sliderCurbeOffset.setBounds (744, 487, 72, 16);
    
    addAndMakeVisible (sliderDifferenceGain);
    sliderDifferenceGain.setRange (0, 10, 0);
    sliderDifferenceGain.setSliderStyle (Slider::LinearHorizontal);
    sliderDifferenceGain.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderDifferenceGain.addListener (this);
    sliderDifferenceGain.setBounds (744, 519, 72, 16);
    
    addAndMakeVisible (sliderRMSWindow);
    sliderRMSWindow.setRange (0, 10, 0);
    sliderRMSWindow.setSliderStyle (Slider::LinearHorizontal);
    sliderRMSWindow.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderRMSWindow.addListener (this);
    sliderRMSWindow.setBounds (744, 551, 72, 16);
    
    addAndMakeVisible (sliderZoomX);
    sliderZoomX.setRange (0, 10, 0);
    sliderZoomX.setSliderStyle (Slider::LinearHorizontal);
    sliderZoomX.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderZoomX.addListener (this);
    sliderZoomX.setBounds (744, 583, 72, 16);
    
    addAndMakeVisible (sliderZoomY);
    sliderZoomY.setRange (0, 10, 0);
    sliderZoomY.setSliderStyle (Slider::LinearHorizontal);
    sliderZoomY.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderZoomY.addListener (this);
    sliderZoomY.setBounds (744, 615, 72, 16);
    
    addAndMakeVisible (sliderTimeAverage);
    sliderTimeAverage.setRange (0, 10, 0);
    sliderTimeAverage.setSliderStyle (Slider::LinearHorizontal);
    sliderTimeAverage.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderTimeAverage.addListener (this);
    sliderTimeAverage.setBounds (904, 615, 72, 16);
    
    addAndMakeVisible (sliderRangeSpectrumDifference);
    sliderRangeSpectrumDifference.setRange (0, 10, 0);
    sliderRangeSpectrumDifference.setSliderStyle (Slider::LinearHorizontal);
    sliderRangeSpectrumDifference.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderRangeSpectrumDifference.addListener (this);
    sliderRangeSpectrumDifference.setBounds (904, 583, 72, 16);
    
    addAndMakeVisible (sliderLevelMeter2);
    sliderLevelMeter2.setRange (0, 10, 0);
    sliderLevelMeter2.setSliderStyle (Slider::LinearHorizontal);
    sliderLevelMeter2.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderLevelMeter2.addListener (this);
    sliderLevelMeter2.setBounds (904, 703, 72, 16);
    
    addAndMakeVisible (sliderLevelMeter1);
    sliderLevelMeter1.setRange (0, 10, 0);
    sliderLevelMeter1.setSliderStyle (Slider::LinearHorizontal);
    sliderLevelMeter1.setTextBoxStyle (Slider::NoTextBox, false, 80, 20);
    sliderLevelMeter1.addListener (this);
    sliderLevelMeter1.setBounds (904, 671, 72, 16);
    
    
    //[/Constructor_pre]
    
    // ----------------------------------------------------------- GUI CODE ----------------------------------------------------------------------------
    
    addAndMakeVisible (curveOffsetLabelText = new Label (String(),TRANS("curve offset\n")));
    curveOffsetLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    curveOffsetLabelText->setJustificationType (Justification::centredLeft);
    curveOffsetLabelText->setEditable (false, false, false);
    curveOffsetLabelText->setColour (TextEditor::textColourId, Colours::black);
    curveOffsetLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    curveOffsetLabelText->setBounds (816, 482, 84, 24);
    
    addAndMakeVisible (differenceGainLabelText = new Label (String(),TRANS("difference gain\n")));
    differenceGainLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    differenceGainLabelText->setJustificationType (Justification::centredLeft);
    differenceGainLabelText->setEditable (false, false, false);
    differenceGainLabelText->setColour (TextEditor::textColourId, Colours::black);
    differenceGainLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    differenceGainLabelText->setBounds (816, 514, 90, 24);
    
    addAndMakeVisible (rmsWindowLabelText = new Label (String(),TRANS("RMS window\n")));
    rmsWindowLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    rmsWindowLabelText->setJustificationType (Justification::centredLeft);
    rmsWindowLabelText->setEditable (false, false, false);
    rmsWindowLabelText->setColour (TextEditor::textColourId, Colours::black);
    rmsWindowLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    rmsWindowLabelText->setBounds (816, 546, 84, 24);
    
    addAndMakeVisible (zoomxLabelText = new Label ("new label",TRANS("zoom X\n")));
    zoomxLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    zoomxLabelText->setJustificationType (Justification::centredLeft);
    zoomxLabelText->setEditable (false, false, false);
    zoomxLabelText->setColour (TextEditor::textColourId, Colours::black);
    zoomxLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    zoomxLabelText->setBounds (816, 578, 84, 24);
    
    addAndMakeVisible (zoomyLabelText = new Label ("new label",TRANS("zoom Y\n")));
    zoomyLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    zoomyLabelText->setJustificationType (Justification::centredLeft);
    zoomyLabelText->setEditable (false, false, false);
    zoomyLabelText->setColour (TextEditor::textColourId, Colours::black);
    zoomyLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    zoomyLabelText->setBounds (816, 610, 84, 24);
    
    addAndMakeVisible (waveformNameTitle = new Label (String(),TRANS("WAVEFORM")));
    waveformNameTitle->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    waveformNameTitle->setJustificationType (Justification::centredLeft);
    waveformNameTitle->setEditable (false, false, false);
    waveformNameTitle->setColour (TextEditor::textColourId, Colours::black);
    waveformNameTitle->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    waveformNameTitle->setBounds (745, 455, 150, 24);
    
    addAndMakeVisible (spectrumAnalyserNameTitle = new Label (String(),TRANS("SPECTRUM ANALYSER\n")));
    spectrumAnalyserNameTitle->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    spectrumAnalyserNameTitle->setJustificationType (Justification::centredLeft);
    spectrumAnalyserNameTitle->setEditable (false, false, false);
    spectrumAnalyserNameTitle->setColour (TextEditor::textColourId, Colours::black);
    spectrumAnalyserNameTitle->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    spectrumAnalyserNameTitle->setBounds (904, 456, 150, 24);
    
    addAndMakeVisible (spectrumDifferenceNameTitle = new Label (String(),TRANS("SPECTRUM DIFFERENCE\n""\n")));
    spectrumDifferenceNameTitle->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    spectrumDifferenceNameTitle->setJustificationType (Justification::centredLeft);
    spectrumDifferenceNameTitle->setEditable (false, false, false);
    spectrumDifferenceNameTitle->setColour (TextEditor::textColourId, Colours::black);
    spectrumDifferenceNameTitle->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    spectrumDifferenceNameTitle->setBounds (904, 552, 150, 24);
    
    addAndMakeVisible (levelMeterNameTitle = new Label (String(),TRANS("LEVEL METER\n")));
    levelMeterNameTitle->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    levelMeterNameTitle->setJustificationType (Justification::centredLeft);
    levelMeterNameTitle->setEditable (false, false, false);
    levelMeterNameTitle->setColour (TextEditor::textColourId, Colours::black);
    levelMeterNameTitle->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    levelMeterNameTitle->setBounds (904, 640, 150, 24);
    
    addAndMakeVisible (vectorscopeNameTitle = new Label (String(),TRANS("VECTORSCOPE\n")));
    vectorscopeNameTitle->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    vectorscopeNameTitle->setJustificationType (Justification::centredLeft);
    vectorscopeNameTitle->setEditable (false, false, false);
    vectorscopeNameTitle->setColour (TextEditor::textColourId, Colours::black);
    vectorscopeNameTitle->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    vectorscopeNameTitle->setBounds (744, 640, 150, 24);
    
    addAndMakeVisible (rangeVectorscopLabelText = new Label (String(),TRANS("range\n")));
    rangeVectorscopLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    rangeVectorscopLabelText->setJustificationType (Justification::centredLeft);
    rangeVectorscopLabelText->setEditable (false, false, false);
    rangeVectorscopLabelText->setColour (TextEditor::textColourId, Colours::black);
    rangeVectorscopLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    rangeVectorscopLabelText->setBounds (816, 666, 80, 24);
    
    addAndMakeVisible (vanishTimeLabelText = new Label (String(),TRANS("vanish time\n")));
    vanishTimeLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    vanishTimeLabelText->setJustificationType (Justification::centredLeft);
    vanishTimeLabelText->setEditable (false, false, false);
    vanishTimeLabelText->setColour (TextEditor::textColourId, Colours::black);
    vanishTimeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    vanishTimeLabelText->setBounds (816, 698, 80, 24);
    
    addAndMakeVisible (rangeVectorscopeLabelText = new Label (String(),TRANS("range\n")));
    rangeVectorscopeLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    rangeVectorscopeLabelText->setJustificationType (Justification::centredLeft);
    rangeVectorscopeLabelText->setEditable (false, false, false);
    rangeVectorscopeLabelText->setColour (TextEditor::textColourId, Colours::black);
    rangeVectorscopeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    rangeVectorscopeLabelText->setBounds (976, 482, 150, 24);
    
    addAndMakeVisible (returnTimeLabelText = new Label (String(),TRANS("return time\n")));
    returnTimeLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    returnTimeLabelText->setJustificationType (Justification::centredLeft);
    returnTimeLabelText->setEditable (false, false, false);
    returnTimeLabelText->setColour (TextEditor::textColourId, Colours::black);
    returnTimeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    returnTimeLabelText->setBounds (976, 514, 150, 24);
    
    addAndMakeVisible (rangeSpectrumLabelText = new Label (String(),TRANS("range\n")));
    rangeSpectrumLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    rangeSpectrumLabelText->setJustificationType (Justification::centredLeft);
    rangeSpectrumLabelText->setEditable (false, false, false);
    rangeSpectrumLabelText->setColour (TextEditor::textColourId, Colours::black);
    rangeSpectrumLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    rangeSpectrumLabelText->setBounds (976, 578, 150, 24);
    
    addAndMakeVisible (timeAveargeLabelText = new Label (String(),TRANS("time average\n")));
    timeAveargeLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    timeAveargeLabelText->setJustificationType (Justification::centredLeft);
    timeAveargeLabelText->setEditable (false, false, false);
    timeAveargeLabelText->setColour (TextEditor::textColourId, Colours::black);
    timeAveargeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    timeAveargeLabelText->setBounds (976, 610, 150, 24);
    
    addAndMakeVisible (textEditorTotalDelay = new TextEditor (String()));
    textEditorTotalDelay->setMultiLine (false);
    textEditorTotalDelay->setReturnKeyStartsNewLine (false);
    textEditorTotalDelay->setReadOnly (true);
    textEditorTotalDelay->setScrollbarsShown (true);
    textEditorTotalDelay->setCaretVisible (false);
    textEditorTotalDelay->setPopupMenuEnabled (true);
    textEditorTotalDelay->setColour (TextEditor::backgroundColourId, Colour (0xff181f22));
    textEditorTotalDelay->setColour (TextEditor::outlineColourId, Colour (0xff42a2c8));
    textEditorTotalDelay->setText (String());
    textEditorTotalDelay->setBounds (752, 736, 80, 24);
    
    addAndMakeVisible (totalDelayLabelText = new Label (String(),TRANS("total delay\n")));
    totalDelayLabelText->setFont (Font ("Avenir Next", 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    totalDelayLabelText->setJustificationType (Justification::centredLeft);
    totalDelayLabelText->setEditable (false, false, false);
    totalDelayLabelText->setColour (TextEditor::textColourId, Colours::black);
    totalDelayLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    totalDelayLabelText->setBounds (840, 736, 150, 24);
    
    
    addAndMakeVisible (textButtonMonoMode);
    textButtonMonoMode.addListener (this);
    textButtonMonoMode.setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    textButtonMonoMode.setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    textButtonMonoMode.setBounds (906, 732, 32, 32);
    
    addAndMakeVisible (monoModeLabelText = new Label (String(),TRANS("M")));
    monoModeLabelText->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    monoModeLabelText->setJustificationType (Justification::centredLeft);
    monoModeLabelText->setEditable (false, false, false);
    monoModeLabelText->setColour (TextEditor::textColourId, Colours::black);
    monoModeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    monoModeLabelText->setBounds (926, 736, 25, 25);
    
    addAndMakeVisible (textButtonLRMode);
    textButtonLRMode.addListener (this);
    textButtonLRMode.setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    textButtonLRMode.setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    textButtonLRMode.setBounds (946, 732, 32, 32);
    
    addAndMakeVisible (leftRightModeLabelText = new Label (String(),TRANS("LR")));
    leftRightModeLabelText->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    leftRightModeLabelText->setJustificationType (Justification::centredLeft);
    leftRightModeLabelText->setEditable (false, false, false);
    leftRightModeLabelText->setColour (TextEditor::textColourId, Colours::black);
    leftRightModeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    leftRightModeLabelText->setBounds (966, 736, 25, 25);
    
    addAndMakeVisible (textButtonMSMode);
    textButtonMSMode.addListener (this);
    textButtonMSMode.setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    textButtonMSMode.setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    textButtonMSMode.setBounds (991, 732, 32, 32);
    
    addAndMakeVisible (midSideModeLabelText = new Label (String(),TRANS("MS")));
    midSideModeLabelText->setFont (Font ("DIN Alternate", 15.00f, Font::plain).withTypefaceStyle ("Bold"));
    midSideModeLabelText->setJustificationType (Justification::centredLeft);
    midSideModeLabelText->setEditable (false, false, false);
    midSideModeLabelText->setColour (TextEditor::textColourId, Colours::black);
    midSideModeLabelText->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    midSideModeLabelText->setBounds (1011, 736, 25, 25);


    
    
    addAndMakeVisible (pluginsManagerButton = new TextButton (String()));
    pluginsManagerButton->setButtonText (TRANS("plugins manager"));
    pluginsManagerButton->addListener (this);
    pluginsManagerButton->setColour (TextButton::buttonColourId, Colour (0xff42a2c8));
    pluginsManagerButton->setColour (TextButton::buttonOnColourId, Colour (0xff181f22));
    pluginsManagerButton->setBounds (17, 16, 143, 32);
    
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
    
    addAndMakeVisible (channelStripNameTitle = new Label (String(),TRANS("CHANNEL STRIP")));
    channelStripNameTitle->setFont (Font ("DIN Alternate", 20.80f, Font::plain));
    channelStripNameTitle->setJustificationType (Justification::centredLeft);
    channelStripNameTitle->setEditable (false, false, false);
    channelStripNameTitle->setColour (Label::textColourId, Colours::aliceblue);
    channelStripNameTitle->setColour (Label::outlineColourId, Colour (0x0042a2c8));
    channelStripNameTitle->setColour (TextEditor::textColourId, Colours::black);
    channelStripNameTitle->setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    channelStripNameTitle->setBounds (1101, -7, 175, 67);
    
    addAndMakeVisible (analyserNameTitle = new Label (String(),TRANS("ANALYSER")));
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
    
    sliderRangeVectorscopeAttach.reset        (new SliderAttachment(parameters, SLIDER_RANGE_VECTORSCOPE_ID,
                                                                    sliderRangeVectorscope));
    sliderVanishTimeAttach.reset              (new SliderAttachment(parameters, SLIDER_VANISH_TIME_ID,
                                                                    sliderVanishTime));
    sliderValueChanged (&sliderRangeVectorscope);
    sliderValueChanged (&sliderVanishTime);
    //========================================================================================================
    sliderCurbeOffsetAttach.reset             (new SliderAttachment(parameters, SLIDER_CURBE_OFFSET_ID,
                                                                    sliderCurbeOffset));
    sliderDifferenceGainAttach.reset          (new SliderAttachment(parameters, SLIDER_DIFFERENCE_GAIN_ID,
                                                                    sliderDifferenceGain));
    sliderRMSWindowAttach.reset               (new SliderAttachment(parameters, SLIDER_RMS_WINDOW_ID,
                                                                    sliderRMSWindow));
    sliderZoomXAttach.reset                   (new SliderAttachment(parameters, SLIDER_ZOOM_X_ID,
                                                                    sliderZoomX));
    sliderZoomYAttach.reset                   (new SliderAttachment(parameters, SLIDER_ZOOM_Y_ID,
                                                                    sliderZoomY));
    
    
    //========================================================================================================
    sliderRangeSpectrumAnalyserAttach.reset   (new SliderAttachment(parameters, SLIDER_RANGE_SPECTRUM_ANALYSER_ID,
                                                                    sliderRangeSpectrumAnalyser));
    sliderReturnTimeAttach.reset              (new SliderAttachment(parameters, SLIDER_RETURN_TIME_ID,
                                                                    sliderReturnTime));
    sliderValueChanged (&sliderRangeSpectrumAnalyser);
    sliderValueChanged (&sliderReturnTime);
    //========================================================================================================
    sliderTimeAverageAttach.reset             (new SliderAttachment(parameters, SLIDER_TIME_AVERAGE_ID,
                                                                    sliderTimeAverage));
    sliderRangeSpectrumDifferenceAttach.reset (new SliderAttachment(parameters, SLIDER_RANGE_SPECTRUM_DIFFERENCE_ID,
                                                                    sliderRangeSpectrumDifference));
    sliderValueChanged (&sliderTimeAverage);
    sliderValueChanged (&sliderRangeSpectrumDifference);
    //========================================================================================================
    sliderLevelMeter1Attach.reset             (new SliderAttachment(parameters, SLIDER_LEVEL_METER_1_ID,
                                                                    sliderLevelMeter1));
    sliderLevelMeter2Attach.reset             (new SliderAttachment(parameters, SLIDER_LEVEL_METER_2_ID,
                                                                    sliderLevelMeter2));
    
    
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
