#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChannelStripAnalyserAudioProcessor::ChannelStripAnalyserAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", AudioChannelSet::stereo(), true)
                     #endif
                       ),
        parameters (*this, nullptr),
        mainAudioBufferSystem  (2, 1, 1)
#endif
{
    formatManager.addDefaultFormats();
    
    // Initialitzation for plugin manager window
    PropertiesFile::Options options;
    options.applicationName = "blackbox";
    options.commonToAllUsers = true;
    options.folderName = "GG PLUGINS";
    options.filenameSuffix = "xml";
    options.storageFormat = PropertiesFile::storeAsXML;
    options.ignoreCaseOfKeyNames = true;
    options.osxLibrarySubFolder = "Application Support";
    
    applicationProperties.setStorageParameters(options);
    applicationProperties.getCommonSettings(true);
    applicationProperties.saveIfNeeded();
 
    // Initialitzation for plugin flagging system
    std::fill_n(plugIns, 6, false);
    std::fill_n(bypassFlag, 6, false);
    std::fill_n(deletedPlugins, 6, false);
    std::fill_n(createPluginCue, 6, false);
    std::fill_n(shownPlugins, 6, false);
    std::fill_n(recalledPlugins, 6, false);
    
    // Initialitzation of graph and input/output nodes
    if( graph.getNumNodes() == 0 )
    {
        AudioProcessorGraph::AudioGraphIOProcessor* in  = new AudioProcessorGraph::AudioGraphIOProcessor
        (AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode);
        AudioProcessorGraph::AudioGraphIOProcessor* out = new AudioProcessorGraph::AudioGraphIOProcessor
        (AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode);
        
        graph.addNode (in,  1);
        graph.addNode (out, 2);
    }
    
    AudioPluginChannelConfiguration();
    
    // Intialitzation of plugin parameters and attachments
    createParameters();
    
    prepareToPlay(getSampleRate(), getBlockSize());
}

ChannelStripAnalyserAudioProcessor::~ChannelStripAnalyserAudioProcessor()
{
    
}

//==============================================================================
void ChannelStripAnalyserAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // graph pre prepareToPlay settings
    graph.setPlayConfigDetails   (getTotalNumInputChannels(), getTotalNumOutputChannels(), sampleRate, samplesPerBlock);
    graph.setProcessingPrecision (AudioProcessor::singlePrecision);
    asampleRate.store (sampleRate);
    ablockSize.store  (samplesPerBlock);
    AudioPluginChannelConfiguration();
    
    const GenericScopedTryLock <CriticalSection> myTryLock (graph.getCallbackLock());
    if ( myTryLock.isLocked())
    {
        graph.prepareToPlay(sampleRate, samplesPerBlock);
    }
    
    const int numOfChannelsInInputStream = getTotalNumInputChannels();
    const int sizeOfAudioBuffer   =  getSampleRate(); //size of audioBuffer is ~1s (fftSize multiple)
    const int sizeOfHistoryBuffer =  5 * getSampleRate(); //size of historyBuffer is ~5s (fftSize multiple)

    mainAudioBufferSystem.reset (numOfChannelsInInputStream, sizeOfAudioBuffer, sizeOfHistoryBuffer);
}

void ChannelStripAnalyserAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ChannelStripAnalyserAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;
    
    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif
    
    return true;
#endif
}
#endif

void ChannelStripAnalyserAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;
    
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    const int numOfSamplesInIncomingBlock = buffer.getNumSamples();
    
    mainAudioBufferSystem.bufferPre.writeBufferIntoAudioBuffer (buffer, numOfSamplesInIncomingBlock);
    
    if (graphLatencySamples != graph.getLatencySamples() )
    {
        graphLatencySamples = graph.getLatencySamples();
        mainAudioBufferSystem.bufferPre.processorDelay.store (graphLatencySamples);
    }
    
    // graph audio stream processing
    const GenericScopedTryLock <CriticalSection> myTryLock (graph.getCallbackLock());
    if ( myTryLock.isLocked())
    {
        graph.processBlock (buffer, midiMessages);
    }
    mainAudioBufferSystem.bufferPost.writeBufferIntoAudioBuffer (buffer, numOfSamplesInIncomingBlock);
}

//==============================================================================
AudioProcessorEditor* ChannelStripAnalyserAudioProcessor::createEditor()
{
    return new ChannelStripAnalyserAudioProcessorEditor (*this, parameters, mainAudioBufferSystem );
}

bool ChannelStripAnalyserAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

//==============================================================================
const String ChannelStripAnalyserAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ChannelStripAnalyserAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ChannelStripAnalyserAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ChannelStripAnalyserAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ChannelStripAnalyserAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================
int ChannelStripAnalyserAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ChannelStripAnalyserAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ChannelStripAnalyserAudioProcessor::setCurrentProgram (int index)
{
}

const String ChannelStripAnalyserAudioProcessor::getProgramName (int index)
{
    return {};
}

void ChannelStripAnalyserAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void ChannelStripAnalyserAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    
    // creating main XML element
    std::unique_ptr<XmlElement> rootXml ( new XmlElement("root_xml"));
    
    for (auto i=0; i<6; i++) {
        rootXml->setAttribute("plugin" + (String)(i+1) + "_state", plugIns[i]);
        rootXml->setAttribute("plugin" + (String)(i+1) + "_bypass", bypassFlag[i]);
        rootXml->setAttribute("plugin" + (String)(i+1) + "_isVisible", shownPlugins[i]);
    }
    
    // Creating internal state XML representation and adding it to the main XML element
    ScopedPointer<XmlElement> internalParameters (parameters.copyState().createXml());
    rootXml->addChildElement(internalParameters);
    
    // for each loaded plugin; exporting the plugin description and the actual state of the processor and saving them in different XML elements
    ScopedPointer <XmlElement> loadedPluginsDescriptions (new XmlElement("LoadedPluginsDescriptions"));
    ScopedPointer <XmlElement> loadedPluginsProcessorState (new XmlElement("LoadedPluginsProcessorState"));
    
    for (auto i=0; i<6; i++)
    {
        if (plugIns[i])
        {
            PluginDescription pd;
            auto* plugin = dynamic_cast<AudioPluginInstance*> (graph.getNodeForId(juce::uint32(i+3))->getProcessor());
            plugin->fillInPluginDescription(pd);
            loadedPluginsDescriptions ->addChildElement(pd.createXml());
            
            MemoryBlock m;
            graph.getNodeForId(juce::uint32(i+3))->getProcessor()->getStateInformation (m);
            loadedPluginsProcessorState ->addChildElement(new XmlElement("Plugin" + (String)(i)));
            loadedPluginsProcessorState ->getChildByName("Plugin" + (String)(i)) ->addTextElement(m.toBase64Encoding());
        }
    }
    
    // adding the created XML elements to the main XML element
    rootXml->addChildElement(loadedPluginsDescriptions);
    rootXml->addChildElement(loadedPluginsProcessorState);
    
    // exporting the main XML to the data block
    copyXmlToBinary(*rootXml,destData);
    
    // de-attaching the different XML elements
    for (auto i=0; i<6; i++)
    {
        if (plugIns[i]) loadedPluginsProcessorState->removeChildElement(loadedPluginsProcessorState->getChildByName((String)(i)), true);
    }
    rootXml->removeChildElement(loadedPluginsProcessorState, false);
    rootXml->removeChildElement(loadedPluginsDescriptions, false);
    rootXml->removeChildElement(internalParameters, false);
}

void ChannelStripAnalyserAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    
    // importing the data into a XML element
    ScopedPointer<XmlElement> rootXml (getXmlFromBinary(data, sizeInBytes));
    
    for (auto i=0; i<6; i++)
    {
        plugIns[i]         = rootXml->getBoolAttribute("plugin" + (String)(i+1) + "_state");
        bypassFlag[i]      = rootXml->getBoolAttribute("plugin" + (String)(i+1) + "_bypass");
        recalledPlugins[i] = rootXml->getBoolAttribute("plugin" + (String)(i+1) + "_isVisible");
    }
    
    // exporting internal parameters from main XML element
    ScopedPointer<XmlElement> internalParameters  (new XmlElement (*rootXml->getChildByName("plugin_parameters")));
    
    // exporting plugin descriptions from the main XML element
    ScopedPointer<XmlElement> pluginsDescriptions (new XmlElement (*rootXml->getChildByName("LoadedPluginsDescriptions")));
    ScopedPointer<XmlElement> pluginsStates (new XmlElement (*rootXml->getChildByName("LoadedPluginsProcessorState")));
    
    for (auto i=0; i<6; i++)
    {
        if (plugIns[i])
        {
            {
                PluginDescription pd;
                ScopedPointer <XmlElement> pdXml (pluginsDescriptions->getFirstChildElement());
                pd.loadFromXml (*pdXml);
                createPluginProcessor(&pd, i+1);
                pluginsDescriptions->removeChildElement(pdXml, false);
            }
            {
                MemoryBlock m;
                ScopedPointer<XmlElement> psXml (pluginsStates->getFirstChildElement());
                m.fromBase64Encoding (psXml->getAllSubText());
                graph.getNodeForId (juce::uint32(i+3)) -> getProcessor() -> setStateInformation (m.getData(), (int) m.getSize());
                pluginsStates -> removeChildElement (psXml, false);
            }
        }
    }
    
    // exporting internal parameters from main XML element
    if (internalParameters!= nullptr)
    {
        if (internalParameters-> hasTagName(parameters.state.getType()))
        {
            parameters.replaceState(ValueTree::fromXml(*internalParameters));
        }
    }
    
    // removing elements from the main XML element
    rootXml->removeChildElement (pluginsDescriptions, false);
    rootXml->removeChildElement (internalParameters,  false);
    rootXml->removeAllAttributes();
    
}

//==============================================================================
void ChannelStripAnalyserAudioProcessor::AudioPluginChannelConfiguration()
{
    // creates the layout of the graph, connecting the active nodes beetwen them and the input/output nodes
    
    bool firstTime = true;
    int lastOn=0;
    
    graph.disconnectNode(1); // input node
    graph.disconnectNode(2); // output node
    
    for (int i = 3; i < 9; i++)
    {
        graph.disconnectNode(juce::uint32(i));
        
        if ((bypassFlag[i-3] == false) && (plugIns[i-3] == true)) // is not bypassed and has been created
        {
            
            if (firstTime == true)
            {
                AudioProcessorGraph::NodeAndChannel nodeFromL;
                nodeFromL.channelIndex = 0;
                nodeFromL.nodeID = 1;
                AudioProcessorGraph::NodeAndChannel nodeFromR;
                nodeFromR.channelIndex = 1;
                nodeFromR.nodeID = 1;
                
                AudioProcessorGraph::NodeAndChannel nodeToL;
                nodeToL.channelIndex = 0;
                nodeToL.nodeID = juce::uint32(i);
                AudioProcessorGraph::NodeAndChannel nodeToR;
                nodeToR.channelIndex = 1;
                nodeToR.nodeID = juce::uint32(i);
                
                AudioProcessorGraph::Connection connectionL(nodeFromL,nodeToL);
                AudioProcessorGraph::Connection connectionR(nodeFromR,nodeToR);
                
                graph.addConnection(connectionL);
                graph.addConnection(connectionR);
                
                lastOn = i;
                firstTime = false;
            }
            else
            {
                AudioProcessorGraph::NodeAndChannel nodeFromL;
                nodeFromL.channelIndex = 0;
                nodeFromL.nodeID = juce::uint32(lastOn);;
                AudioProcessorGraph::NodeAndChannel nodeFromR;
                nodeFromR.channelIndex = 1;
                nodeFromR.nodeID = juce::uint32(lastOn);;
                
                AudioProcessorGraph::NodeAndChannel nodeToL;
                nodeToL.channelIndex = 0;
                nodeToL.nodeID = juce::uint32(i);
                AudioProcessorGraph::NodeAndChannel nodeToR;
                nodeToR.channelIndex = 1;
                nodeToR.nodeID = juce::uint32(i);
                
                AudioProcessorGraph::Connection connectionL(nodeFromL,nodeToL);
                AudioProcessorGraph::Connection connectionR(nodeFromR,nodeToR);
                
                graph.addConnection(connectionL);
                graph.addConnection(connectionR);
                
                lastOn = i;
            }
        }
    }
    
    if (lastOn == 0)
    {
        AudioProcessorGraph::NodeAndChannel nodeFromL;
        nodeFromL.channelIndex = 0;
        nodeFromL.nodeID = 1;
        AudioProcessorGraph::NodeAndChannel nodeFromR;
        nodeFromR.channelIndex = 1;
        nodeFromR.nodeID = 1;
        
        AudioProcessorGraph::NodeAndChannel nodeToL;
        nodeToL.channelIndex = 0;
        nodeToL.nodeID = 2;
        AudioProcessorGraph::NodeAndChannel nodeToR;
        nodeToR.channelIndex = 1;
        nodeToR.nodeID = 2;
        
        AudioProcessorGraph::Connection connectionL(nodeFromL,nodeToL);
        AudioProcessorGraph::Connection connectionR(nodeFromR,nodeToR);
        
        graph.addConnection(connectionL);
        graph.addConnection(connectionR);
    }
    else
    {
        AudioProcessorGraph::NodeAndChannel nodeFromL;
        nodeFromL.channelIndex = 0;
        nodeFromL.nodeID = juce::uint32(lastOn);
        AudioProcessorGraph::NodeAndChannel nodeFromR;
        nodeFromR.channelIndex = 1;
        nodeFromR.nodeID = juce::uint32(lastOn);
        
        AudioProcessorGraph::NodeAndChannel nodeToL;
        nodeToL.channelIndex = 0;
        nodeToL.nodeID = 2;
        AudioProcessorGraph::NodeAndChannel nodeToR;
        nodeToR.channelIndex = 1;
        nodeToR.nodeID = 2;
        
        AudioProcessorGraph::Connection connectionL(nodeFromL,nodeToL);
        AudioProcessorGraph::Connection connectionR(nodeFromR,nodeToR);
        
        graph.addConnection(connectionL);
        graph.addConnection(connectionR);
    }
}

void ChannelStripAnalyserAudioProcessor::createPluginProcessor(const PluginDescription* desc, int i)
{
    plugIns[i-1] = true;
    String errorMessage;
    graph.addNode (formatManager.createPluginInstance (*desc, getSampleRate(), getBlockSize(), errorMessage),juce::uint32(i+2));
    AudioPluginChannelConfiguration();
}

void ChannelStripAnalyserAudioProcessor::deletePluginProcessor(int i)
{
    graph.removeNode(juce::uint32(i+2));
    AudioPluginChannelConfiguration();
}

void ChannelStripAnalyserAudioProcessor::createParameters()
{
    
    // WAVEFORM
    parameters.createAndAddParameter(SLIDER_WAVEFORM_RANGE_ID, SLIDER_WAVEFORM_RANGE_NAME, SLIDER_WAVEFORM_RANGE_NAME,
                                     NormalisableRange<float> (1.0f,3.0f, 1.0f), 1.0f, nullptr, nullptr);
    parameters.createAndAddParameter(SLIDER_WAVEFORM_TIMEMODE_ID, SLIDER_WAVEFORM_TIMEMODE_NAME, SLIDER_WAVEFORM_TIMEMODE_NAME,
                                     NormalisableRange<float> (0.0f,2.0f), 1.0f, nullptr, nullptr);
    
    // VECTORSCOPE
    parameters.createAndAddParameter(SLIDER_RANGE_VECTORSCOPE_ID, SLIDER_RANGE_VECTORSCOPE_NAME, SLIDER_RANGE_VECTORSCOPE_NAME,
                                     NormalisableRange<float> (50,400,1), 200, nullptr, nullptr);
    parameters.createAndAddParameter(SLIDER_VANISH_TIME_ID, SLIDER_VANISH_TIME_NAME, SLIDER_VANISH_TIME_NAME,
                                     NormalisableRange<float> (5,50,1), 20, nullptr, nullptr);
    
    // SPECTRUM ANALYSER
    parameters.createAndAddParameter(SLIDER_RANGE_SPECTRUM_ANALYSER_ID, SLIDER_RANGE_SPECTRUM_ANALYSER_NAME, SLIDER_RANGE_SPECTRUM_ANALYSER_NAME,
                                     NormalisableRange<float> (1.0f,4.0f,1.0f), 4.f, nullptr, nullptr);
    parameters.createAndAddParameter(SLIDER_RETURN_TIME_ID, SLIDER_RETURN_TIME_NAME, SLIDER_RETURN_TIME_NAME,
                                     NormalisableRange<float> (1.f, 4.f, 1.f), 4.f, nullptr, nullptr);
    
    // SPECTRUM DIFFERENCE
    parameters.createAndAddParameter(SLIDER_RANGE_SPECTRUM_DIFFERENCE_ID, SLIDER_RANGE_SPECTRUM_DIFFERENCE_NAME, SLIDER_RANGE_SPECTRUM_DIFFERENCE_NAME,
                                     NormalisableRange<float> (1.0f,4.0f,1.0f), 1.f, nullptr, nullptr);
    parameters.createAndAddParameter(SLIDER_TIME_AVERAGE_ID, SLIDER_TIME_AVERAGE_NAME, SLIDER_TIME_AVERAGE_NAME,
                                     NormalisableRange<float> (1.f ,4.f ,1.f), 3.f, nullptr, nullptr);
    
    // PLUGIN SLOTS
    parameters.createAndAddParameter(BUTTON_PLUGINBYPASS_1_ID, BUTTON_PLUGINBYPASS_1_NAME, BUTTON_PLUGINBYPASS_1_NAME, NormalisableRange<float> (0.0f,1.0f), false, nullptr, nullptr, false, true, true);
    parameters.createAndAddParameter(BUTTON_PLUGINBYPASS_2_ID, BUTTON_PLUGINBYPASS_2_NAME, BUTTON_PLUGINBYPASS_2_NAME, NormalisableRange<float> (0.0f,1.0f), false, nullptr, nullptr, false, true, true);
    parameters.createAndAddParameter(BUTTON_PLUGINBYPASS_3_ID, BUTTON_PLUGINBYPASS_3_NAME, BUTTON_PLUGINBYPASS_3_NAME, NormalisableRange<float> (0.0f,1.0f), false, nullptr, nullptr, false, true, true);
    parameters.createAndAddParameter(BUTTON_PLUGINBYPASS_4_ID, BUTTON_PLUGINBYPASS_4_NAME, BUTTON_PLUGINBYPASS_4_NAME, NormalisableRange<float> (0.0f,1.0f), false, nullptr, nullptr, false, true, true);
    parameters.createAndAddParameter(BUTTON_PLUGINBYPASS_5_ID, BUTTON_PLUGINBYPASS_5_NAME, BUTTON_PLUGINBYPASS_5_NAME, NormalisableRange<float> (0.0f,1.0f), false, nullptr, nullptr, false, true, true);
    parameters.createAndAddParameter(BUTTON_PLUGINBYPASS_6_ID, BUTTON_PLUGINBYPASS_6_NAME, BUTTON_PLUGINBYPASS_6_NAME, NormalisableRange<float> (0.0f,1.0f), false, nullptr, nullptr, false, true, true);
    
    // M S M/S
    parameters.createAndAddParameter("BUTTON_MONOMODE_ID", "BUTTON_MONOMODE_NAME", "BUTTON_MONOMODE_NAME", NormalisableRange<float> (0.0f,1.0f), true, nullptr, nullptr, false, true, true);
    parameters.createAndAddParameter("BUTTON_LRMODE_ID", "BUTTON_LRMODE_NAME", "BUTTON_LRMODE_NAME", NormalisableRange<float> (0.0f,1.0f), false, nullptr, nullptr, false, true, true);
    parameters.createAndAddParameter("BUTTON_MSMODE_ID", "BUTTON_MSMODE_NAME", "BUTTON_MSMODE_NAME", NormalisableRange<float> (0.0f,1.0f), false, nullptr, nullptr, false, true, true);
    
    parameters.state = ValueTree("plugin_parameters");
    
}

void ChannelStripAnalyserAudioProcessor::triggerGraphPrepareToPlay()
{
    graph.prepareToPlay(getSampleRate(), getBlockSize());
}
//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChannelStripAnalyserAudioProcessor();
}

//==============================================================================
AudioBufferManagement::AudioBufferManagement (int channels, int sizeAudioBuffer, int sizeHistoryBuffer)
:
bufferPre  (channels, sizeAudioBuffer, sizeHistoryBuffer, "bufferPre", threadMutex),
bufferPost (channels, sizeAudioBuffer, sizeHistoryBuffer, "bufferPost", threadMutex)
{
    visualizersSemaphore.store(0);
}

void AudioBufferManagement::pushAudioBufferIntoHistoryBuffer()
{
    if (bufferPre.abstractFifoAudio.getNumReady() != bufferPost.abstractFifoAudio.getNumReady())
        return;
    
    for (int bufferPrePost = 0; bufferPrePost < 2; bufferPrePost++)
    {
        audioBufferManagementType& buffer ( bufferPrePost ? bufferPost : bufferPre);
        
        const int historyFifoFreespace = buffer.abstractFifoHistory.getFreeSpace();
        const int numOfNewSamplesInAudioBuffer = buffer.abstractFifoAudio.getNumReady();
        const int numOfSamplesInHistoryBuffer = buffer.abstractFifoHistory.getTotalSize();
        const int numOfChannels = buffer.historyBuffer.getNumChannels();
        
        if ( historyFifoFreespace < numOfNewSamplesInAudioBuffer)
        {
            buffer.abstractFifoHistory.finishedRead (numOfNewSamplesInAudioBuffer - historyFifoFreespace);
        }
        
        int writeIndex1, writeSize1, writeIndex2, writeSize2;
        int readIndex1, readSize1, readIndex2, readSize2;
        buffer.abstractFifoHistory .prepareToWrite (numOfNewSamplesInAudioBuffer, writeIndex1, writeSize1, writeIndex2, writeSize2);
        buffer.abstractFifoAudio   .prepareToRead  (numOfNewSamplesInAudioBuffer, readIndex1, readSize1, readIndex2, readSize2);
        
        if (writeSize2 == 0)
        {
            for (int ch = 0; ch < numOfChannels ; ++ch)
            {
                buffer.historyBuffer.copyFrom (ch, writeIndex1, buffer.audioBuffer, ch, readIndex1, readSize1);
                if (readSize2 > 0)
                    buffer.historyBuffer.copyFrom (ch, writeIndex1 + readSize1, buffer.audioBuffer, ch, readIndex2, readSize2);
            }
        }
        else
        {
            if (readSize1 > writeSize1)
            {
                for (int ch = 0; ch < numOfChannels ; ++ch)
                {
                    buffer.historyBuffer.copyFrom (ch, writeIndex1, buffer.audioBuffer, ch, readIndex1, writeSize1);
                    buffer.historyBuffer.copyFrom (ch, 0, buffer.audioBuffer, ch, readIndex1 + writeSize1, readSize1 - writeSize1);
                    if (readSize2 > 0)
                        buffer.historyBuffer.copyFrom (ch, readSize1 - writeSize1, buffer.audioBuffer, ch, readIndex2, readSize2);
                }
            }
            else
            {
                if (readSize1 + readSize2 > writeSize1)
                {
                    const int numOfSamplesToTheEnd = ( readSize1 + readSize2 ) - writeSize1;
                    for (int ch = 0; ch < numOfChannels ; ++ch)
                    {
                        buffer.historyBuffer.copyFrom (ch, writeIndex1, buffer.audioBuffer, ch, readIndex1, readSize1);
                        buffer.historyBuffer.copyFrom (ch, writeIndex1 + readSize1, buffer.audioBuffer, ch, readIndex2, readSize2 - numOfSamplesToTheEnd);
                        buffer.historyBuffer.copyFrom (ch, 0, buffer.audioBuffer, ch, readIndex2 + readSize2 - numOfSamplesToTheEnd, numOfSamplesToTheEnd);
                    }
                    
                }
                
            }
            
        }
        int lastSampleIndex;
        int absoluteIndex = writeIndex1 + writeSize1 + writeSize2;
        if (absoluteIndex > numOfSamplesInHistoryBuffer )
            lastSampleIndex = absoluteIndex % (numOfSamplesInHistoryBuffer);
        else
            lastSampleIndex = absoluteIndex;
        
        buffer.lastSampleIndexHistoryBuffer.store  (lastSampleIndex);
        buffer.abstractFifoHistory  .finishedWrite (readSize1 + readSize2);
        buffer.abstractFifoAudio    .finishedRead  (readSize1 + readSize2);

    }
}

void AudioBufferManagement::reset(int newChannel, int newSizeAudioBuffer, int newSizeHistoryBuffer)
{
    bufferPre.reset(newChannel, newSizeAudioBuffer, newSizeHistoryBuffer);
    bufferPost.reset(newChannel, newSizeAudioBuffer, newSizeHistoryBuffer);
}

void AudioBufferManagement::audioBufferManagementType::reset(int ch, int sAb, int sHb)
{
    historyBuffer.clear();
    abstractFifoHistory.reset();
    audioBuffer.clear();
    abstractFifoAudio.reset();
    
    historyBuffer.setSize(ch, sHb);
    abstractFifoHistory.setTotalSize(sHb > 0 ? sHb : 1);
    audioBuffer.setSize(ch, sAb);
    abstractFifoAudio.setTotalSize(sAb > 0 ? sAb : 1);
    
    lastSampleIndexAudioBuffer.store(0);
    lastSampleIndexHistoryBuffer.store(0);
}

void AudioBufferManagement::audioBufferManagementType::writeBufferIntoAudioBuffer(AudioBuffer<float> &fromBuffer, int numOfSamples)
{
    int index1, size1, index2, size2;
    
    const int fifoFreeSpace = abstractFifoAudio.getFreeSpace();
    const int numOfSamplesInInputBlock = fromBuffer.getNumSamples();
    
    // if there is no space for the incoming audioBlock, make room for it.
    if (fifoFreeSpace < numOfSamplesInInputBlock )
    {
        abstractFifoAudio.finishedRead ( numOfSamplesInInputBlock - fifoFreeSpace ); //sets the oldest data to readed
    }
    
    // copying the data to the audioBuffer
    abstractFifoAudio.prepareToWrite (numOfSamples, index1, size1, index2, size2);
    if (size1 > 0)
    {
        for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
            audioBuffer.copyFrom(ch, index1, fromBuffer, ch, 0, size1);
    }
    if (size2 > 0)
    {
        for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
            audioBuffer.copyFrom(ch, index2, fromBuffer, ch, size1, size2);
    }
    
    int lastSampleIndex = size2 == 0 ? index1 + size1 : index2 + size2;
    
    lastSampleIndexAudioBuffer.store  (lastSampleIndex);
    abstractFifoAudio.finishedWrite ( size1 + size2 );
    
}

void AudioBufferManagement::audioBufferManagementType::copySamplesFromHistoryBuffer(AudioBuffer<float> &toBuffer, int numOfSamples)
{
    const int procDelay = processorDelay.load();
    const int numOfSamplesInBuffer = historyBuffer.getNumSamples();
    const int lastSampleIndex = lastSampleIndexHistoryBuffer.load() - procDelay;
    
    int firstSampleIndex = lastSampleIndex - numOfSamples;
    if (firstSampleIndex < 0) firstSampleIndex = numOfSamplesInBuffer + firstSampleIndex;
    if ( firstSampleIndex + numOfSamples > numOfSamplesInBuffer)
    {
        int size1 = numOfSamplesInBuffer - firstSampleIndex;
        int size2 = numOfSamples - size1;
        for (auto channel = 0; channel < historyBuffer.getNumChannels(); channel++)
        {
            toBuffer.copyFrom(channel, 0, historyBuffer, channel, firstSampleIndex, size1);
            toBuffer.copyFrom(channel, size1, historyBuffer, channel, 0, size2);
        }
    }
    else
    {
        for (auto channel = 0; channel < historyBuffer.getNumChannels(); channel++)
            toBuffer.copyFrom(channel, 0, historyBuffer, channel, firstSampleIndex, numOfSamples);
    }
}

float AudioBufferManagement::audioBufferManagementType::getRMSMonoValueInSample(int samplesInThePast, int windowSize)
{
    const int procDelay = processorDelay.load();
    const int numOfSamplesInBuffer = historyBuffer.getNumSamples();
    const int lastSampleIndex = lastSampleIndexHistoryBuffer.load() - procDelay - samplesInThePast;
    
    AudioBuffer<float> monoBuffer ( 1, windowSize );
    int firstSampleIndex = lastSampleIndex - windowSize;
    if (firstSampleIndex < 0) firstSampleIndex = numOfSamplesInBuffer + firstSampleIndex;
    if ( firstSampleIndex + windowSize > numOfSamplesInBuffer)
    {
        int size1 = numOfSamplesInBuffer - firstSampleIndex;
        int size2 = windowSize - size1;
        
        FloatVectorOperations::add
            (monoBuffer.getWritePointer(0, 0), historyBuffer.getReadPointer(0, firstSampleIndex), historyBuffer.getReadPointer(1, firstSampleIndex), size1);
        FloatVectorOperations::add
            (monoBuffer.getWritePointer(0, size1), historyBuffer.getReadPointer(0, 0), historyBuffer.getReadPointer(1, 0), size2);
    }
    else
    {
        FloatVectorOperations::add
            (monoBuffer.getWritePointer(0, 0), historyBuffer.getReadPointer(0, firstSampleIndex), historyBuffer.getReadPointer(1, firstSampleIndex), windowSize);
    }
    return monoBuffer.getRMSLevel(0, 0, windowSize);
}

float AudioBufferManagement::audioBufferManagementType::getRMSChannelValueInSample(int samplesInThePast, int windowSize, int channel)
{
    const int procDelay = processorDelay.load();
    const int numOfSamplesInBuffer = historyBuffer.getNumSamples();
    const int lastSampleIndex = lastSampleIndexHistoryBuffer.load() - procDelay - samplesInThePast;
    
    AudioBuffer<float> monoBuffer ( 1, windowSize );
    int firstSampleIndex = lastSampleIndex - windowSize;
    if (firstSampleIndex < 0) firstSampleIndex = numOfSamplesInBuffer + firstSampleIndex;
    if ( firstSampleIndex + windowSize > numOfSamplesInBuffer)
    {
        int size1 = numOfSamplesInBuffer - firstSampleIndex;
        int size2 = windowSize - size1;
        FloatVectorOperations::copy(monoBuffer.getWritePointer(0, 0), historyBuffer.getReadPointer(channel, firstSampleIndex), size1);
        FloatVectorOperations::copy(monoBuffer.getWritePointer(0, size1), historyBuffer.getReadPointer(channel, 0), size2);
    }
    else
    {
        FloatVectorOperations::copy(monoBuffer.getWritePointer(0, 0), historyBuffer.getReadPointer(channel, firstSampleIndex), windowSize);
    }
    return monoBuffer.getRMSLevel(0, 0, windowSize);
}

int AudioBufferManagement::audioBufferManagementType::getLastIndexPositionInHistoryBuffer()
{
    return lastSampleIndexHistoryBuffer.load();
}


//==============================================================================
void forwardFFT::createWindowTable()
{
    for (auto i = 0; i < fftSize; i++)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            float value = 1 * (0.5f * (1 - std::cos ( (2 * juce::float_Pi * i) / (fftSize - 1) ) ) );
            windowTable.setSample(ch, i, value);
        }
    }
}

void forwardFFT::performFFT( AudioBuffer<float>& audioBuffer )
{
    std::lock_guard<std::mutex> lock (m);
    const int numOfChannels = audioBuffer.getNumChannels();
    for (auto channel = 0; channel < numOfChannels; channel++)
    {
        FloatVectorOperations::multiply (audioBuffer.getWritePointer(channel), windowTable.getReadPointer(channel), fftSize);
        forwFFT->performRealOnlyForwardTransform (audioBuffer.getWritePointer(channel));
    }
}

void forwardFFT::changeFFTSize(int newSize)
{
    std::lock_guard<std::mutex> lock (m);
    fftSize = newSize;
    createWindowTable();
    forwFFT.reset (new dsp::FFT (roundToInt (std::log2 (fftSize))));
}









