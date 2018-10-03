  #include "Visualizers.h"

//==============================================================================
// SPECTRUM ANALYSER
SpectrumAnalyser::SpectrumAnalyser(int sR, int fS, int cH, AudioBufferManagement& buffManag, forwardFFT& fFFT)
:
axisNeedsUpdate(true),
scaleModeAt(4),
decayRatioAt(0.92f),
mainAudioBufferSystem (buffManag),
forwFFT(fFFT)
{
    setTopLeftPosition(9,265); //7, 263
    setSize (710, 277); // 714,281
    setVisible(true);
    setPaintingIsUnclipped(true);
    setOpaque(true);

    conversionTable.    clear();
    maxValuesPre.       clear();
    maxValuesPost.      clear();
    monoMagDataPre.     clear();
    monoMagDataPost.    clear();
    
    sampleRateAt .store(sR);
    fftSizeAt    .store(fS);
    
    int sampleRate = sampleRateAt.load();
    int fftSize    = fftSizeAt.load();
    
    createNewAxis();
    
    // Init. the data & indexes Vectors
    for (auto i = 0; i < fftSize; i++)
    {
        maxValuesPre.   push_back(0);
        maxValuesPost.   push_back(0);
    }
    
    for (auto i = 0; i < getWidth(); i++)
    {
        float res = ((float)sampleRate) / ((float)fftSize);
        float F = std::exp((i * std::log((sampleRate / 2.f) / 10.f ) / getWidth()) + std::log(10));
        float freqInBinScale = F * (1 / res);
        
        int   bin1 = (int)freqInBinScale;
        float posBin1 = (getWidth() / log((sampleRate / 2) / 10)) * log((bin1 * res) / 10);
        if (isinf(posBin1)) posBin1 = 0;
        float posBin2 = (getWidth() / log((sampleRate / 2) / 10)) * log(((bin1 + 1) * res) / 10);
        
        std::vector<float> convTab(3,0);
        convTab[0] = bin1;
        convTab[1] = posBin1;
        convTab[2] = posBin2;
        
        conversionTable.push_back(convTab);
    }
}

SpectrumAnalyser::~SpectrumAnalyser()
{
}

void SpectrumAnalyser::paint (Graphics& g)
{
    createFrame();
    g.drawImage (spectrumFrame, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
}

void SpectrumAnalyser::createFrame()
{

    spectrumFrame = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g (spectrumFrame);
    
    g.setColour (Colour (0xff0f0f1c));
    g.fillRoundedRectangle(0, 0, getWidth(), getHeight(), 8.0f);

    if ( axisNeedsUpdate.load())
    {
        createNewAxis();
        axisNeedsUpdate.store(false);
    }
    processAllFftData (mainAudioBufferSystem.bufferPre,  monoMagDataPre,  maxValuesPre);
    processAllFftData (mainAudioBufferSystem.bufferPost, monoMagDataPost, maxValuesPost);
    
    Path drawPathPre, drawPathPost;
    
    createPath (monoMagDataPre,  drawPathPre);
    createPath (monoMagDataPost, drawPathPost);
    
    g.setColour  (Colours::lightgrey);
    g.setOpacity (0.3f);
    g.strokePath (drawPathPre, PathStrokeType(1, PathStrokeType::beveled));
    g.setColour  (Colours::dimgrey);
    g.setOpacity (0.1f);
    g.fillPath   (drawPathPre);
    
    g.setColour  (Colours::whitesmoke);
    g.setOpacity (1.0f);
    g.strokePath (drawPathPost, PathStrokeType (1.3, PathStrokeType::beveled));
    g.setColour  (Colours::transparentWhite);
    g.setOpacity (0.1f);
    g.fillPath   (drawPathPost);
    
    g.setOpacity(1.0f);
    g.drawImage (shadeWindow, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
    g.drawImage (axisImage, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
}

void SpectrumAnalyser::createPath ( std::vector<double>& magnitudeDBmono, Path &drawPath)
{
    int maxRange;
    switch (scaleModeAt.load()){
        case 1: maxRange = 54.f; break; case 2: maxRange = 72.f; break;
        case 3: maxRange = 90.f; break; case 4: maxRange = 108.f; break;
    }
    int index = 0;
    int finalPoint = getWidth() - 1;
    int stuff;
    
    drawPath.startNewSubPath  (0, getHeight());
    while ( index < finalPoint )
    {
        if ( index > 500 &&  index < 708)
        {
            stuff = 100;
        }
        
        float posNextBin   = conversionTable [index][2];
        float magActualBin = magnitudeDBmono [conversionTable [index][0]];
        
        float magDb = magActualBin;
        magDb = ( 1 + (magDb / maxRange) ) * getHeight();
        if (isinf(magDb)  || isnan(magDb))   magDb  = 0;
        
        drawPath.lineTo          (Point<float> (index, getHeight() - magDb));
        index = (int)(posNextBin) + 1;
    }
    drawPath.lineTo(getWidth(), getHeight());
}

void SpectrumAnalyser::processAllFftData (audioBufferManagementType& buffer,  std::vector<double>& magintudeDbMono, std::vector<float>& maxValues )
{
    magintudeDbMono.clear();
    
    const int fftSize = fftSizeAt.load();
    const int numOfChannels = buffer.historyBuffer.getNumChannels();
     
    AudioBuffer<float> auxBuffer (numOfChannels, fftSize * 2);
    auxBuffer.clear();
    buffer.copySamplesFromHistoryBuffer(auxBuffer, fftSize);
    forwFFT.performFFT(auxBuffer);

    double decayRatio = decayRatioAt.load();
    for (auto i = 0; i < fftSize; i++)
    {
        float levelReal = 0;
        float levelImag = 0;
        
        for (auto channel = 0; channel < auxBuffer.getNumChannels(); channel++)
        {
            levelReal =  auxBuffer.getSample(channel, (2 * i)) + levelReal;
            levelImag =  auxBuffer.getSample(channel, (2 * i) + 1) + levelImag;
        }
        levelReal = levelReal / auxBuffer.getNumChannels();
        levelImag = levelImag / auxBuffer.getNumChannels();
        
        float magLin = sqrt (std::pow(levelReal, 2) + std::pow(levelImag, 2));
        peakHolder(i, magLin, maxValues, decayRatio);
        
        float magInDb = 20 * log10( magLin / (fftSize / 4));
        magintudeDbMono.push_back(magInDb);
    }
}

void SpectrumAnalyser::peakHolder(int index, float& magLin, std::vector<float>& maxValues, double &decayRatio)
{
    if ( maxValues[index] > magLin )
    {
        magLin = maxValues[index];
        maxValues[index] = maxValues[index] * decayRatio;
    }
    else
    {
        maxValues[index] = magLin;
    }
}

void SpectrumAnalyser::resized() {}

void SpectrumAnalyser::createNewAxis()
{
    axisImage = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g (axisImage);
    
    int height = getHeight();
    int width  = getWidth();
    
    int maxRange;
    switch (scaleModeAt.load()){
        case 1: maxRange = 54.0f; break; case 2: maxRange = 72.000000; break; case 3: maxRange = 90.000000; break;  case 4: maxRange = 108.000000; break;
    }
    
    float sampleRate = sampleRateAt.load();
    float dashedlength[2];
    dashedlength[0] = 8;
    dashedlength[1] = 4;
    
    for (int i = 1; i <= 10; i++){ // plot FREQ axis 1
        
        int fo;
        switch (i){
            case 1: fo = 20;    break; case 2:  fo = 50;    break;
            case 3: fo = 100;   break; case 4:  fo = 200;   break;
            case 5: fo = 500;   break; case 6:  fo = 1000;  break;
            case 7: fo = 2000;  break; case 8:  fo = 5000;  break;
            case 9: fo = 10000; break; case 10: fo = 20000; break;
        }
        int freq2draw = (width / std::log((sampleRate / 2) / 10))*std::log(fo / 10);
        g.setColour(Colours::lightgrey);
        g.setOpacity(0.3);
        g.drawVerticalLine(freq2draw, 0, height);
        g.setOpacity(1.0f);
        g.setColour(Colours::white);
        g.setFont(11.f);
        g.drawText((String)fo, freq2draw + 10, height - 17, 50, 20, juce::Justification::left, true);
    }
    
    for (int i = 1; i <= 18; i++)
    { //    plot the labels and the axis
        int fo;
        switch (i)
        {
            case 1: fo = 30;     break;  case 2: fo = 40;     break;  case 3: fo = 60;     break;
            case 4: fo = 70;     break;  case 5: fo = 80;     break;  case 6: fo = 90;     break;
            case 7: fo = 300;    break;  case 8: fo = 400;    break;  case 9: fo = 600;    break;
            case 10: fo = 700;   break;  case 11: fo = 800;   break;  case 12: fo = 900;   break;
            case 13: fo = 3000;  break;  case 14: fo = 4000;  break;  case 15: fo = 6000;  break;
            case 16: fo = 7000;  break;  case 17: fo = 8000;  break;  case 18: fo = 9000;  break;
        }
        float freq2draw = (width / std::log((sampleRate / 2) / 10))*std::log(fo / 10);
        g.setColour(Colours::grey);
        g.setOpacity(0.5);
        g.drawDashedLine(juce::Line<float>(freq2draw, height, freq2draw, 0), dashedlength, 2, 1);
    }

    for (int i = 0; i <= 12; i++){  // PLOT MAG AXIS
        int fo;
        switch (i){
            case 0:  fo = 0; break;
            case 1:  fo = -maxRange + (maxRange/12) * (12 - i); break;  case 2:  fo = -maxRange + (maxRange/12) * (12 - i); break;  case 3:  fo = -maxRange + (maxRange/12) * (12 - i); break;
            case 4:  fo = -maxRange + (maxRange/12) * (12 - i); break;  case 5:  fo = -maxRange + (maxRange/12) * (12 - i); break;  case 6:  fo = -maxRange + (maxRange/12) * (12 - i); break;
            case 7:  fo = -maxRange + (maxRange/12) * (12 - i); break;  case 8:  fo = -maxRange + (maxRange/12) * (12 - i); break;  case 9:  fo = -maxRange + (maxRange/12) * (12 - i); break;
            case 10: fo = -maxRange + (maxRange/12) * (12 - i); break;  case 11: fo = -maxRange + (maxRange/12) * (12 - i); break;  case 12: fo = -maxRange + (maxRange/12) * (12 - i); break;
        }

        if (i % 2 == 0){
            if (i == 0 || i == 12){
                
                g.setOpacity(0.5);
                g.drawHorizontalLine (height * (i / 12.0f), 30, (float)width - 10);
                g.setOpacity(1.0f);
                g.setColour(Colours::white);
                g.setFont(11.f);
                g.drawText ((String)fo, 10, i *(height / 12.0f) - 5, 50, 20, juce::Justification::left, true);
            }
            else{
                g.setColour(Colours::lightgrey);
                g.setOpacity(0.3);
                g.drawHorizontalLine(height*(i / 12.0f), 30, (float)width - 10);
                g.setOpacity(1.0f);
                g.setColour(Colours::white);
                g.setFont(11.f);
                g.drawText((String)fo, 10, i*(height / 12.0f) - 10, 50, 20, juce::Justification::left, true);
            }
        }
        else{
            g.setColour(Colours::grey);
            g.setOpacity(0.4);
            g.drawHorizontalLine(height*(i / 12.0f), 0, (float)width - 10);
        }
    }
    
    shadeWindow = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g2 (shadeWindow);
    for (int i = 0; i < 20; i++)
    {
        float opacity = (float(i) + 30) / 50;
        g2.setColour(Colours::black.withAlpha(isPositiveAndBelow(1 - opacity, 1) ? 1 - opacity : 0.f));
        g2.drawVerticalLine   (width - i, 0, height);
        g2.drawHorizontalLine (i, 0, width);
        g2.drawHorizontalLine (height - i, 0, width);
    }
    for (int i = 0; i < 50; i++)
    {
        float opacity = (float(i) + 20) / 50.f;
        g2.setColour(Colours::black);
        g2.setOpacity(isPositiveAndBelow(1 - opacity, 1) ? 1 - opacity : 0.f);
        g2.drawVerticalLine(i, 0, height);
    }
    axisNeedsUpdate.store(false);
}


//==============================================================================
// SPECTRUM DIFFERENCE
SpectrumDifference::SpectrumDifference(int sR, int fS, int cH, AudioBufferManagement& buffManag, forwardFFT& fFFT)
:
    axisNeedsUpdate(true),
    analyseMode(1),
    scaleModeAt(1),
    fftSizeAt(fS),
    sampleRateAt(sR),
    numOfHistorySamplesAt(8),
    mainAudioBufferSystem(buffManag),
    forwFFT(fFFT)
{
    setTopLeftPosition(9,552); //7, 263
    setSize (710, 116); // 714,281
    setVisible(true);
    setPaintingIsUnclipped(true);
    setOpaque(true);

    numOfHistorySamples = numOfHistorySamplesAt.load();
    
    conversionTable.    clear();
    linGainData1.       clear();
    linGainData2.       clear();
    
    int sampleRate = sampleRateAt.load();
    int fftSize    = fftSizeAt.load();
    int numOfHistorySamples = numOfHistorySamplesAt.load();
    
    createNewAxis();

    // Init. the data & indexes Vectors
    for (auto i = 0; i < fftSize; i++)
    {
        linGainData1. push_back(0);
        linGainData2. push_back(0);
        historyValues1.push_back (std::vector<float> (numOfHistorySamples, 1.0f));
        historyValues2.push_back (std::vector<float> (numOfHistorySamples, 1.0f));
    }
    
    for (auto i = 0; i < getWidth(); i++)
    {
        float res = ((float)sampleRate) / ((float)fftSize);
        float F = std::exp((i * std::log((sampleRate / 2.f) / 10.f ) / getWidth()) + std::log(10));
        float freqInBinScale = F * (1 / res);
        
        int   bin1 = (int)freqInBinScale;
        float posBin1 = (getWidth() / log((sampleRate / 2) / 10))
        * log((bin1 * res) / 10);
        if (isinf(posBin1)) posBin1 = 0;
        
        float posBin2 = (getWidth() / log((sampleRate / 2) / 10))
        * log(((bin1 + 1) * res) / 10);
        
        std::vector<float> convTab(3,0);
        convTab[0] = bin1;
        convTab[1] = posBin1;
        convTab[2] = posBin2;
        
        conversionTable.push_back(convTab);
    }
}

SpectrumDifference::~SpectrumDifference() 
{
}

void SpectrumDifference::paint (Graphics& g)
{
    createFrame();
    g.drawImage (mainFrame, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
}

void SpectrumDifference::createFrame()
{

    mainFrame = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g (mainFrame);

    g.setColour (Colour (0xff0f0f1c));
    g.fillRoundedRectangle(0, 0, getWidth(), getHeight(), 8.0f);

    if ( axisNeedsUpdate.load() ) createNewAxis();

    processAllFftData (mainAudioBufferSystem.bufferPre, mainAudioBufferSystem.bufferPost, linGainData1, linGainData2 );
    
    if (analyseMode.load() == 1 )
    {
        Path drawPath1;
        createPath (linGainData1, drawPath1);
        g.setColour  (Colours::whitesmoke);
        g.setOpacity (1.0f);
        g.strokePath (drawPath1, PathStrokeType (1.3, PathStrokeType::beveled));
    }
    else if (analyseMode.load() == 2 )
    {
        Path drawPath1, drawPath2;
        createPath (linGainData1, drawPath1);
        createPath (linGainData2, drawPath2);
        
        g.setColour  (Colours::lightgoldenrodyellow);
        g.setOpacity (1.0f);
        g.strokePath (drawPath1, PathStrokeType (1.3, PathStrokeType::beveled));
        
        g.setColour  (Colours::skyblue);
        g.setOpacity (1.0f);
        g.strokePath (drawPath2, PathStrokeType (1.3, PathStrokeType::beveled));
    }
    else if (analyseMode.load() == 3 )
    {
        Path drawPath1, drawPath2;
        createPath (linGainData1, drawPath1);
        createPath (linGainData2, drawPath2);
        
        g.setColour  (Colours::whitesmoke);
        g.setOpacity (1.0f);
        g.strokePath (drawPath1, PathStrokeType (1.3, PathStrokeType::beveled));
        
        g.setColour  (Colours::palevioletred);
        g.setOpacity (1.0f);
        g.strokePath (drawPath2, PathStrokeType (1.3, PathStrokeType::beveled));
    }
    
    g.drawImage (shadeWindow, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
    g.drawImage (axisImage, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
}

void SpectrumDifference::createPath ( std::vector<double>& magnitudeDBmono, Path &drawPath)
{
    int maxRange;
    switch (scaleModeAt.load())
    {
        case 1: maxRange = 24.f; break; case 2: maxRange = 72.f; break;
        case 3: maxRange = 144.f; break; case 4: maxRange = 240.f; break;
    }
    int index = 0;
    int finalPoint = getWidth() - 1;
    
    drawPath.startNewSubPath  (0, getHeight()/2);
    while ( index != finalPoint )
    {
        float posNextBin   = conversionTable [index][2];
        float magActualBin = magnitudeDBmono [conversionTable [index][0]];
        
        float magDb = magActualBin;
        magDb = ( (magDb / maxRange) ) * getHeight();
        if (isinf(magDb)  || isnan(magDb))   magDb  = 0;
        
        drawPath.lineTo          (Point<float> (index, (getHeight()/2.f) - magDb));
        index = (int)(posNextBin) + 1;
    }
    drawPath.lineTo(getWidth(), getHeight()/2.f);
}

void SpectrumDifference::processAllFftData (audioBufferManagementType& bufferPre, audioBufferManagementType& bufferPost, std::vector<double>& linGain1, std::vector<double>& linGain2 )
{
    linGain1.clear();
    linGain2.clear();
    
    const int fftSize    = fftSizeAt.load();
    const int numOfChannels = bufferPre.historyBuffer.getNumChannels();
    
    AudioBuffer<float> auxBufferPre (numOfChannels, fftSize * 2);
    auxBufferPre.clear();
    bufferPre.copySamplesFromHistoryBuffer(auxBufferPre, fftSize);
    forwFFT.performFFT(auxBufferPre);
    
    AudioBuffer<float> auxBufferPost (numOfChannels, fftSize * 2);
    auxBufferPost.clear();
    bufferPost.copySamplesFromHistoryBuffer(auxBufferPost, fftSize);
    forwFFT.performFFT(auxBufferPost);
    
    for (auto i = 0; i < fftSize; i++)
    {
        
        switch (analyseMode.load())
        {
            case 1: // MONO ANALYSIS
            {
                float levelRealPre = 0;
                float levelImagPre = 0;
                float levelRealPost = 0;
                float levelImagPost = 0;

                for (auto channel = 0; channel < 1; channel++)
                {
                    levelRealPre =   auxBufferPre.getSample (channel, (2 * i))         + levelRealPre;
                    levelImagPre =   auxBufferPre.getSample (channel, (2 * i) + 1)     + levelImagPre;
                    levelRealPost =  auxBufferPost.getSample (channel, (2 * i))     + levelRealPost;
                    levelImagPost =  auxBufferPost.getSample (channel, (2 * i) + 1) + levelImagPost;
                }
                levelRealPre  = levelRealPre / auxBufferPre.getNumChannels();
                levelImagPre  = levelImagPre / auxBufferPre.getNumChannels();
                levelRealPost = levelRealPost / auxBufferPost.getNumChannels();
                levelImagPost = levelImagPost / auxBufferPost.getNumChannels();
                
                float magLinPre  = sqrt (std::pow(levelRealPre, 2)  + std::pow(levelImagPre, 2));
                float magLinPost = sqrt (std::pow(levelRealPost, 2) + std::pow(levelImagPost, 2));
                
                float gain = magLinPost / magLinPre;
                if (isinf(gain) ) gain = 0;
                
                peakHolder(i, gain, historyValues1);
                
                float gainInDb = 20 * log10 (gain);
                linGain1.push_back(gainInDb);
                break;
            }
            case 2: // LEFT/RIGHT ANALYSIS
            {
                float levelRealPreL  = auxBufferPre.getSample(0, (2 * i));
                float levelImagPreL  = auxBufferPre.getSample(0, (2 * i) + 1);
                float levelRealPostL = auxBufferPost.getSample(0, (2 * i));
                float levelImagPostL = auxBufferPost.getSample(0, (2 * i) + 1);
                
                float magLinPreL  = sqrt  (std::pow(levelRealPreL, 2) + std::pow(levelImagPreL, 2));
                float magLinPostL = sqrt (std::pow(levelRealPostL, 2) + std::pow(levelImagPostL, 2));

                float gainLinL = magLinPostL / magLinPreL;
                
                peakHolder(i, gainLinL, historyValues1);
                
                float gainInDbL = 20 * log10 (gainLinL);
                linGain1.push_back(gainInDbL);
                
                float levelRealPreR  = auxBufferPre.getSample(1, (2 * i));
                float levelImagPreR  = auxBufferPre.getSample(1, (2 * i) + 1);
                float levelRealPostR = auxBufferPost.getSample(1, (2 * i));
                float levelImagPostR = auxBufferPost.getSample(1, (2 * i) + 1);
                
                float magLinPreR  = sqrt (std::pow(levelRealPreR, 2) + std::pow(levelImagPreR, 2));
                float magLinPostR = sqrt (std::pow(levelRealPostR, 2) + std::pow(levelImagPostR, 2));
                
                float gainLinR = magLinPostR / magLinPreR;
                
                peakHolder(i, gainLinR, historyValues2);
                
                float gainInDbR = 20 * log10 (gainLinR);
                linGain2.push_back(gainInDbR);
                
                break;
            }
            case 3: // MID/SIDE ANALYSIS
            {
                float levelRealL = auxBufferPre.getSample(0, (2 * i));
                float levelImagL = auxBufferPre.getSample(0, (2 * i) + 1);
                float levelRealR = auxBufferPre.getSample(1, (2 * i));
                float levelImagR = auxBufferPre.getSample(1, (2 * i) + 1);

                float magLinMidPre = sqrt( std::pow((levelRealL + levelRealR) / 2, 2)
                                          + std::pow((levelImagL + levelImagR) / 2, 2));
                
                levelRealL = auxBufferPost.getSample(0, (2 * i));
                levelImagL = auxBufferPost.getSample(0, (2 * i) + 1);
                levelRealR = auxBufferPost.getSample(1, (2 * i));
                levelImagR = auxBufferPost.getSample(1, (2 * i) + 1);

                float magLinMidPost = sqrt( std::pow((levelRealL + levelRealR) / 2, 2)
                                           + std::pow((levelImagL + levelImagR) / 2, 2));
                
                float gainLinMid = magLinMidPost / magLinMidPre;
                
                peakHolder(i, gainLinMid, historyValues1);
                float gainInDbMid = 20 * log10 (gainLinMid);
                linGain1.push_back(gainInDbMid);
                
                
                
                levelRealL = auxBufferPre.getSample(0, (2 * i));
                levelImagL = auxBufferPre.getSample(0, (2 * i) + 1);
                levelRealR = auxBufferPre.getSample(1, (2 * i));
                levelImagR = auxBufferPre.getSample(1, (2 * i) + 1);
                
                float sideLinPre = sqrt( std::pow((levelRealL - levelRealR) / 2, 2)
                                          + std::pow((levelImagL - levelImagR) / 2, 2));
                
                levelRealL = auxBufferPost.getSample(0, (2 * i));
                levelImagL = auxBufferPost.getSample(0, (2 * i) + 1);
                levelRealR = auxBufferPost.getSample(1, (2 * i));
                levelImagR = auxBufferPost.getSample(1, (2 * i) + 1);
                
                float sideLinPost = sqrt( std::pow((levelRealL - levelRealR) / 2, 2)
                                        + std::pow((levelImagL - levelImagR) / 2, 2));

                float gainLinSide = sideLinPost / sideLinPre;
                
                peakHolder(i, gainLinSide, historyValues2);
                float gainInDbSide = 20 * log10 (gainLinSide);
                linGain2.push_back(gainInDbSide);
                
                break;
            }
        }
    }
}

void SpectrumDifference::peakHolder(int index, float& magLin, floatMatrix &historySamples)
{
    if (numOfHistorySamples != numOfHistorySamplesAt.load())
    {
        historyValues1.clear();
        historyValues2.clear();
        numOfHistorySamples = numOfHistorySamplesAt.load();
        int fftSize = fftSizeAt.load();
        for (auto i = 0; i < fftSize; i++)
        {
            historyValues1.push_back (std::vector<float> (numOfHistorySamples, 1.0f));
            historyValues2.push_back (std::vector<float> (numOfHistorySamples, 1.0f));
        }
    }
        
    for (auto i = 0 ; i < numOfHistorySamples; ++i)
    {
        if (i == numOfHistorySamples - 1)
            historySamples[index][i] = magLin;
        else
            historySamples[index][i] = historySamples[index][i + 1];
    }
    
    float averageValue = 0;
    for (auto i = 0; i < numOfHistorySamples; ++i)
        averageValue = historySamples[index][i] + averageValue;
    averageValue = averageValue / numOfHistorySamples;
    
    magLin = averageValue;
}

void SpectrumDifference::resized() {}

void SpectrumDifference::createNewAxis()
{
    axisImage = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g (axisImage);
    
    int height = getHeight();
    int width  = getWidth();
    
    g.setColour(Colour (0xff42a2c8));
    g.drawHorizontalLine(getHeight(), 0, getWidth());
    g.drawHorizontalLine(getHeight() - 1, 0, getWidth());

    
    float sampleRate = sampleRateAt.load();
    float dashedlength[2];
    dashedlength[0] = 4;
    dashedlength[1] = 2;
    
    for (int i = 1; i <= 10; i++){ // plot the labels and the axis
        
        int fo;
        switch (i)
        {
            case 1: fo = 20; break; case 2: fo = 50; break;
            case 3: fo = 100; break; case 4: fo = 200; break;
            case 5: fo = 500; break; case 6: fo = 1000; break;
            case 7: fo = 2000; break; case 8: fo = 5000; break;
            case 9: fo = 10000; break; case 10: fo = 20000; break;
        }
        int freq2draw = (width / std::log((sampleRate / 2) / 10))*std::log(fo / 10);
        g.setColour(Colours::lightgrey);
        g.setOpacity(0.3);
        g.drawVerticalLine(freq2draw, 0, height);
        g.setColour(Colours::white);
    }
    
    for (int i = 1; i <= 18; i++)
    { //    plot the labels and the axis
        int fo;
        switch (i)
        {
            case 1: fo = 30; break; case 2: fo = 40; break; case 3: fo = 60; break;
            case 4: fo = 70; break; case 5: fo = 80; break; case 6: fo = 90; break;
            case 7: fo = 300; break; case 8: fo = 400; break; case 9: fo = 600; break;
            case 10: fo = 700; break; case 11: fo = 800; break; case 12: fo = 900; break;
            case 13: fo = 3000; break; case 14: fo = 4000; break; case 15: fo = 6000; break;
            case 16: fo = 7000; break; case 17: fo = 8000; break; case 18: fo = 9000; break;
        }
        float freq2draw = (width / std::log((sampleRate / 2) / 10))*std::log(fo / 10);
        g.setColour(Colours::grey);
        g.setOpacity(0.5);
        g.drawDashedLine(juce::Line<float>(freq2draw, height, freq2draw, 0), dashedlength, 2, 1);
    }
    
    switch (scaleModeAt.load()){
        case 1:
            for (int i = 0; i <= 8; i++){
                String fo;
                switch (i)
                {
                    case 0: fo = ""; break;
                    case 1: fo = "+9"; break; case 2: fo = "+6"; break; case 3: fo = "+3"; break;
                    case 4: fo = "0"; break; case 5: fo = "-3"; break; case 6: fo = "-6"; break;
                    case 7: fo = "-9"; break; case 8: fo = ""; break;
                }
                Colour numberColour;
                if (i == 1 || i == 2 || i == 3 ) numberColour = Colours::palegreen;
                if (i == 4 ) numberColour = Colours::white;
                if (i == 5 || i == 6 || i == 7 ) numberColour = Colours::palevioletred;
                
                if (i == 4)
                {
                    g.setColour(Colours::white);
                    g.setOpacity(0.6);
                    g.drawHorizontalLine(height*(i / 8.000000), 30, (float)width - 10);
                    g.setColour(numberColour);
                    g.setFont(11.f);
                    g.drawText((String)fo, 10, i*(height / 8.000000) - 10, 50, 20, juce::Justification::left, true);
                }
                else
                {
                    g.setColour(Colours::grey);
                    g.setOpacity(0.3);
                    g.drawHorizontalLine(height*(i / 8.000000), 30, (float)width - 10);
                    g.setColour(numberColour);
                    g.setFont(11.f);
                    g.drawText((String)fo, 10, i*(height / 8.000000) - 10, 50, 20, juce::Justification::left, true);
                }
                
            }
            break;
        case 2:
            for (int i = 0; i <= 8; i++){
                String fo;
                switch (i)
                {
                    case 0: fo = ""; break;
                    case 1: fo = "+27"; break; case 2: fo = "+18"; break; case 3: fo = "+9"; break;
                    case 4: fo = "0"; break; case 5: fo = "-9"; break; case 6: fo = "-18"; break;
                    case 7: fo = "-27"; break; case 8: fo = ""; break;
                }
                
                Colour numberColour;
                if (i == 1 || i == 2 || i == 3 ) numberColour = Colours::palegreen;
                if (i == 4 ) numberColour = Colours::white;
                if (i == 5 || i == 6 || i == 7 ) numberColour = Colours::palevioletred;

                if (i == 4)
                {
                    g.setColour(Colours::white);
                    g.setOpacity(0.6);
                    g.drawHorizontalLine(height*(i / 8.000000), 30, (float)width - 10);
                    g.setColour(numberColour);
                    g.setFont(11.f);
                    g.drawText((String)fo, 10, i*(height / 8.000000) - 10, 50, 20, juce::Justification::left, true);
                }
                else
                {
                    g.setColour(Colours::grey);
                    g.setOpacity(0.3);
                    g.drawHorizontalLine(height*(i / 8.000000), 30, (float)width - 10);
                    g.setColour(numberColour);
                    g.setFont(11.f);
                    g.drawText((String)fo, 10, i*(height / 8.000000) - 10, 50, 20, juce::Justification::left, true);
                }
            }
            break;
        case 3:
            for (int i = 0; i <= 8; i++){
                String fo;
                switch (i)
                {
                    case 0: fo = ""; break;
                    case 1: fo = "+54"; break; case 2: fo = "+36"; break; case 3: fo = "+18"; break;
                    case 4: fo = "0"; break; case 5: fo = "-18"; break; case 6: fo = "-36"; break;
                    case 7: fo = "-54"; break; case 8: fo = ""; break;
                }
                
                Colour numberColour;
                if (i == 1 || i == 2 || i == 3 ) numberColour = Colours::palegreen;
                if (i == 4 ) numberColour = Colours::white;
                if (i == 5 || i == 6 || i == 7 ) numberColour = Colours::palevioletred;

                if (i == 4)
                {
                    g.setColour(Colours::white);
                    g.setOpacity(0.6);
                    g.drawHorizontalLine(height*(i / 8.000000), 30, (float)width - 10);
                    g.setColour(numberColour);
                    g.setFont(11.f);
                    g.drawText((String)fo, 10, i*(height / 8.000000) - 10, 50, 20, juce::Justification::left, true);
                }
                else
                {
                    g.setColour(Colours::grey);
                    g.setOpacity(0.3);
                    g.drawHorizontalLine(height*(i / 8.000000), 30, (float)width - 10);
                    g.setColour(numberColour);
                    g.setFont(11.f);
                    g.drawText((String)fo, 10, i*(height / 8.000000) - 10, 50, 20, juce::Justification::left, true);
                }
            }
            break;
        case 4:
            for (int i = 0; i <= 8; i++){
                String fo;
                switch (i)
                {
                    case 0: fo = ""; break;
                    case 1: fo = "+90"; break; case 2: fo = "+60"; break; case 3: fo = "+30"; break;
                    case 4: fo = "0"; break; case 5: fo = "-30"; break; case 6: fo = "-60"; break;
                    case 7: fo = "-90"; break; case 8: fo = ""; break;
                }
                Colour numberColour;
                if (i == 1 || i == 2 || i == 3 ) numberColour = Colours::palegreen;
                if (i == 4 ) numberColour = Colours::white;
                if (i == 5 || i == 6 || i == 7 ) numberColour = Colours::palevioletred;

                if (i == 4)
                {
                    g.setColour(Colours::white);
                    g.setOpacity(0.6);
                    g.drawHorizontalLine(height*(i / 8.000000), 30, (float)width - 10);
                    g.setColour(numberColour);
                    g.setFont(11.f);
                    g.drawText((String)fo, 10, i*(height / 8.000000) - 10, 50, 20, juce::Justification::left, true);
                }
                else
                {
                    g.setColour(Colours::grey);
                    g.setOpacity(0.3);
                    g.drawHorizontalLine(height*(i / 8.000000), 30, (float)width - 10);
                    g.setColour(numberColour);
                    g.setFont(11.f);
                    g.drawText((String)fo, 10, i*(height / 8.000000) - 10, 50, 20, juce::Justification::left, true);
                }
            }
            break;
    }
    shadeWindow = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g2 (shadeWindow);
    for (int i = 0; i < 20; i++)
    {
        float opacity = (float(i) + 30) / 50;
        g2.setColour(Colours::black.withAlpha(isPositiveAndBelow(1 - opacity, 1) ? 1 - opacity : 0.f));
        g2.drawVerticalLine   (width - i, 0, height);
        g2.drawHorizontalLine (i, 0, width);
        g2.drawHorizontalLine (height - i, 0, width);
    }
    for (int i = 0; i < 50; i++)
    {
        float opacity = (float(i) + 20) / 50.f;
        g2.setColour(Colours::black.withAlpha(isPositiveAndBelow(1 - opacity, 1) ? 1 - opacity : 0.f));
        g2.drawVerticalLine(i, 0, height);
    }

    axisNeedsUpdate.store(false);
}


//==============================================================================
// PHASE DIFFERENCE
PhaseDifference::PhaseDifference(int sR, int fS, int nC, AudioBufferManagement& buffManag, forwardFFT& fFFT)
:
    analyseMode(1),
    numOfHistorySamplesAt(8),
    mainAudioBufferSystem(buffManag),
    forwFFT(fFFT)
{
    setTopLeftPosition(9,668); //7, 263
    setSize (710, 116); // 714,281
    setVisible(true);
    setPaintingIsUnclipped(true);
    setOpaque(true);

    conversionTable.    clear();
    linGainData1.       clear();
    linGainData2.       clear();
    
    sampleRateAt .store(sR);
    fftSizeAt    .store(fS);
    
    
    const int sampleRate = sampleRateAt.load();
    const int fftSize    = fftSizeAt.load();
    numOfHistorySamples = numOfHistorySamplesAt.load();
    
    createNewAxis();
    
    // Init. the data & indexes Vectors
    for (auto i = 0; i < fftSize; i++)
    {
        linGainData1. push_back(0);
        linGainData2. push_back(0);
        historyValues1.push_back (std::vector<float> (numOfHistorySamples, 0.f));
        historyValues2.push_back (std::vector<float> (numOfHistorySamples, 0.f));
    }
    
    for (auto i = 0; i < getWidth(); i++)
    {
        float res = ((float)sampleRate) / ((float)fftSize);
        float F = std::exp((i * std::log((sampleRate / 2.f) / 10.f ) / getWidth()) + std::log(10));
        float freqInBinScale = F * (1 / res);
        
        int   bin1 = (int)freqInBinScale;
        float posBin1 = (getWidth() / log((sampleRate / 2) / 10))
        * log((bin1 * res) / 10);
        if (isinf(posBin1)) posBin1 = 0;
        
        float posBin2 = (getWidth() / log((sampleRate / 2) / 10))
        * log(((bin1 + 1) * res) / 10);
        
        std::vector<float> convTab(3,0);
        convTab[0] = bin1;
        convTab[1] = posBin1;
        convTab[2] = posBin2;
        
        conversionTable.push_back(convTab);
    }
}

PhaseDifference::~PhaseDifference()
{
}

void PhaseDifference::paint (Graphics& g)
{
    createFrame();
    g.drawImage (mainFrame, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
}

void PhaseDifference::createFrame()
{
    mainFrame = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g (mainFrame);
    
    g.setColour (Colour (0xff0f0f1c));
    g.fillRoundedRectangle(0, 0, getWidth(), getHeight(), 8.0f);
    
    processAllFftData (mainAudioBufferSystem.bufferPre, mainAudioBufferSystem.bufferPost, linGainData1, linGainData2 );
    
    if (analyseMode.load() == 1 )
    {
        Path drawPath1;
        createPath (linGainData1, drawPath1);
        g.setColour  (Colours::whitesmoke);
        g.setOpacity (1.0f);
        g.strokePath (drawPath1, PathStrokeType (1.3, PathStrokeType::beveled));
    }
    else if (analyseMode.load() == 2 )
    {
        Path drawPath1, drawPath2;
        createPath (linGainData1, drawPath1);
        createPath (linGainData2, drawPath2);
        
        g.setColour  (Colours::lightgoldenrodyellow);
        g.setOpacity (1.0f);
        g.strokePath (drawPath1, PathStrokeType (1.3, PathStrokeType::beveled));
        
        g.setColour  (Colours::skyblue);
        g.setOpacity (1.0f);
        g.strokePath (drawPath2, PathStrokeType (1.3, PathStrokeType::beveled));
    }
    else if (analyseMode.load() == 3 )
    {
        Path drawPath1, drawPath2;
        createPath (linGainData1, drawPath1);
        createPath (linGainData2, drawPath2);
        
        g.setColour  (Colours::whitesmoke);
        g.setOpacity (1.0f);
        g.strokePath (drawPath1, PathStrokeType (1.3, PathStrokeType::beveled));
        
        g.setColour  (Colours::palevioletred);
        g.setOpacity (1.0f);
        g.strokePath (drawPath2, PathStrokeType (1.3, PathStrokeType::beveled));
    }
    
    g.drawImage (shadeWindow, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
    g.drawImage (axisImage, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
}

void PhaseDifference::createPath ( std::vector<double>& magnitudeDBmono, Path &drawPath)
{

    int index = 0;
    int finalPoint = getWidth() - 1;
    
    drawPath.startNewSubPath  (0, getHeight()/2);
    while ( index != finalPoint )
    {
        float posNextBin   = conversionTable [index][2];
        float magActualBin = magnitudeDBmono [conversionTable [index][0]];
        
        float magDb = magActualBin;
        magDb = magActualBin * getHeight();
        if (isinf(magDb)  || isnan(magDb))   magDb  = 0;
        
        drawPath.lineTo          (Point<float> (index, (getHeight()/2.f) - magDb));
        index = (int)(posNextBin) + 1;
    }
    drawPath.lineTo(getWidth(), getHeight()/2.f);
}

void PhaseDifference::processAllFftData (audioBufferManagementType& bufferPre, audioBufferManagementType& bufferPost, std::vector<double>& linGain1, std::vector<double>& linGain2 )
{
    linGain1.clear();
    linGain2.clear();
    
    const int fftSize    = fftSizeAt.load();
    const int numOfChannels = bufferPre.historyBuffer.getNumChannels();
    
    
    AudioBuffer<float> auxBufferPre (numOfChannels, fftSize * 2);
    auxBufferPre.clear();
    bufferPre.copySamplesFromHistoryBuffer(auxBufferPre, fftSize);
    forwFFT.performFFT(auxBufferPre);
    
    AudioBuffer<float> auxBufferPost (numOfChannels, fftSize * 2);
    auxBufferPost.clear();
    bufferPost.copySamplesFromHistoryBuffer(auxBufferPost, fftSize);
    forwFFT.performFFT(auxBufferPost);

    for (auto i = 0; i < fftSize; i++)
    {
        
        switch (analyseMode.load())
        {
            case 1: // MONO ANALYSIS
            {
                float levelRealPre = 0;
                float levelImagPre = 0;
                float levelRealPost = 0;
                float levelImagPost = 0;
                
                for (auto channel = 0; channel < 1; channel++)
                {
                    levelRealPre =   auxBufferPre.getSample (channel, (2 * i))         + levelRealPre;
                    levelImagPre =   auxBufferPre.getSample (channel, (2 * i) + 1)     + levelImagPre;
                    levelRealPost =  auxBufferPost.getSample (channel, (2 * i))     + levelRealPost;
                    levelImagPost =  auxBufferPost.getSample (channel, (2 * i) + 1) + levelImagPost;
                }
                levelRealPre  = levelRealPre / auxBufferPre.getNumChannels();
                levelImagPre  = levelImagPre / auxBufferPre.getNumChannels();
                levelRealPost = levelRealPost / auxBufferPost.getNumChannels();
                levelImagPost = levelImagPost / auxBufferPost.getNumChannels();
                
                float magLinPre  = sqrt (std::pow(levelRealPre, 2)  + std::pow(levelImagPre, 2));
                float angPre = std::asin (levelImagPre / magLinPre);
                if (levelRealPre < 0 && levelImagPre > 0) angPre = -angPre + juce::float_Pi;
                if (levelRealPre < 0 && levelImagPre < 0) angPre = -angPre - juce::float_Pi;

                float magLinPost = sqrt (std::pow(levelRealPost, 2) + std::pow(levelImagPost, 2));
                float angPost = std::asin (levelImagPost / magLinPost);
                if (levelRealPost < 0 && levelImagPost > 0) angPost = -angPost + juce::float_Pi;
                if (levelRealPost < 0 && levelImagPost < 0) angPost = -angPost - juce::float_Pi;
                
                float phaseDiffPre = angPre - angPost;
                int numOfCompleteCycles = (int) (phaseDiffPre / juce::float_Pi);
                if (numOfCompleteCycles < -1 ) numOfCompleteCycles = -1;
                float d = phaseDiffPre - (2 * juce::float_Pi * numOfCompleteCycles);
                phaseDiffPre = d;
                if (d >  juce::float_Pi) d =  juce::float_Pi;
                if (d < -juce::float_Pi) d = -juce::float_Pi;
                phaseDiffPre = d / (2 * juce::float_Pi);
                
                peakHolder(i, phaseDiffPre, historyValues1);
                
                linGain1.push_back(phaseDiffPre);
                break;
            }
            case 2: // LEFT/RIGHT ANALYSIS
            {
                // LEFT CHANNEL
                float levelRealPre  = auxBufferPre.getSample(0, (2 * i));
                float levelImagPre  = auxBufferPre.getSample(0, (2 * i) + 1);
                float levelRealPost = auxBufferPost.getSample(0, (2 * i));
                float levelImagPost = auxBufferPost.getSample(0, (2 * i) + 1);
                
                float magLinPre  = sqrt (std::pow(levelRealPre, 2)  + std::pow(levelImagPre, 2));
                float angPre = std::asin (levelImagPre / magLinPre);
                if (levelRealPre < 0 && levelImagPre > 0) angPre = -angPre + juce::float_Pi;
                if (levelRealPre < 0 && levelImagPre < 0) angPre = -angPre - juce::float_Pi;
                
                float magLinPost = sqrt (std::pow(levelRealPost, 2) + std::pow(levelImagPost, 2));
                float angPost = std::asin (levelImagPost / magLinPost);
                if (levelRealPost < 0 && levelImagPost > 0) angPost = -angPost + juce::float_Pi;
                if (levelRealPost < 0 && levelImagPost < 0) angPost = -angPost - juce::float_Pi;

                float phaseDiffPre = angPre - angPost;
                int numOfCompleteCycles = (int) (phaseDiffPre / juce::float_Pi);
                if (numOfCompleteCycles < -1 ) numOfCompleteCycles = -1;
                float d = phaseDiffPre - (2 * juce::float_Pi * numOfCompleteCycles);
                phaseDiffPre = d;
                if (d >  juce::float_Pi) d =  juce::float_Pi;
                if (d < -juce::float_Pi) d = -juce::float_Pi;
                phaseDiffPre = d / (2 * juce::float_Pi);
                
                peakHolder(i, phaseDiffPre, historyValues1);
                linGain1.push_back(phaseDiffPre);
                
                // RIGHT CHANNEL
                levelRealPre  = auxBufferPre.getSample(1, (2 * i));
                levelImagPre  = auxBufferPre.getSample(1, (2 * i) + 1);
                levelRealPost = auxBufferPost.getSample(1, (2 * i));
                levelImagPost = auxBufferPost.getSample(1, (2 * i) + 1);
                
                magLinPre  = sqrt (std::pow(levelRealPre, 2)  + std::pow(levelImagPre, 2));
                angPre = std::asin (levelImagPre / magLinPre);
                if (levelRealPre < 0 && levelImagPre > 0) angPre = -angPre + juce::float_Pi;
                if (levelRealPre < 0 && levelImagPre < 0) angPre = -angPre - juce::float_Pi;
                
                magLinPost = sqrt (std::pow(levelRealPost, 2) + std::pow(levelImagPost, 2));
                angPost = std::asin (levelImagPost / magLinPost);
                if (levelRealPost < 0 && levelImagPost > 0) angPost = -angPost + juce::float_Pi;
                if (levelRealPost < 0 && levelImagPost < 0) angPost = -angPost - juce::float_Pi;
                
                phaseDiffPre = angPre - angPost;
                numOfCompleteCycles = (int) (phaseDiffPre / juce::float_Pi);
                if (numOfCompleteCycles < -1 ) numOfCompleteCycles = -1;
                d = phaseDiffPre - (2 * juce::float_Pi * numOfCompleteCycles);
                phaseDiffPre = d;
                if (d >  juce::float_Pi) d =  juce::float_Pi;
                if (d < -juce::float_Pi) d = -juce::float_Pi;
                phaseDiffPre = d / (2 * juce::float_Pi);
                
                peakHolder(i, phaseDiffPre, historyValues2);
                linGain2.push_back(phaseDiffPre);

                break;
            }
            case 3: // MID/SIDE ANALYSIS
            {
                // MID
                float levelRealL = auxBufferPre.getSample(0, (2 * i));
                float levelImagL = auxBufferPre.getSample(0, (2 * i) + 1);
                float levelRealR = auxBufferPre.getSample(1, (2 * i));
                float levelImagR = auxBufferPre.getSample(1, (2 * i) + 1);
                
                float levelImag = levelImagL + levelImagR;
                float levelReal = levelRealL + levelRealR;
                float magLin = sqrt( std::pow((levelReal) / 2, 2)
                                          + std::pow((levelImag) / 2, 2));
                float angPre = std::asin (levelImag / magLin);
                if (levelReal < 0 && levelImag > 0) angPre = -angPre + juce::float_Pi;
                if (levelReal < 0 && levelImag < 0) angPre = -angPre - juce::float_Pi;

                
                levelRealL = auxBufferPost.getSample(0, (2 * i));
                levelImagL = auxBufferPost.getSample(0, (2 * i) + 1);
                levelRealR = auxBufferPost.getSample(1, (2 * i));
                levelImagR = auxBufferPost.getSample(1, (2 * i) + 1);
                
                levelImag = levelImagL + levelImagR;
                levelReal = levelRealL + levelRealR;
                magLin= sqrt( std::pow((levelReal) / 2, 2)
                                       + std::pow((levelImag) / 2, 2));
                float angPost = std::asin (levelImag / magLin);
                if (levelReal < 0 && levelImag > 0) angPre = -angPost + juce::float_Pi;
                if (levelReal < 0 && levelImag < 0) angPre = -angPost - juce::float_Pi;

                float phaseDiff = angPre - angPost;
                float numOfCompleteCycles = (int) (phaseDiff / juce::float_Pi);
                if (numOfCompleteCycles < -1 ) numOfCompleteCycles = -1;
                float d = phaseDiff - (2 * juce::float_Pi * numOfCompleteCycles);
                phaseDiff = d;
                if (d >  juce::float_Pi) d =  juce::float_Pi;
                if (d < -juce::float_Pi) d = -juce::float_Pi;
                phaseDiff= d / (2 * juce::float_Pi);
                
                peakHolder(i, phaseDiff, historyValues1);
                linGain1.push_back(phaseDiff);
                
                // SIDE
                levelRealL = auxBufferPre.getSample(0, (2 * i));
                levelImagL = auxBufferPre.getSample(0, (2 * i) + 1);
                levelRealR = auxBufferPre.getSample(1, (2 * i));
                levelImagR = auxBufferPre.getSample(1, (2 * i) + 1);
                
                levelImag = levelImagL - levelImagR;
                levelReal = levelRealL - levelRealR;
                magLin = sqrt( std::pow((levelReal) / 2, 2)
                                    + std::pow((levelImag) / 2, 2));
                angPre = std::asin (levelImag / magLin);
                if (levelReal < 0 && levelImag > 0) angPre = -angPre + juce::float_Pi;
                if (levelReal < 0 && levelImag < 0) angPre = -angPre - juce::float_Pi;
                
                
                levelRealL = auxBufferPost.getSample(0, (2 * i));
                levelImagL = auxBufferPost.getSample(0, (2 * i) + 1);
                levelRealR = auxBufferPost.getSample(1, (2 * i));
                levelImagR = auxBufferPost.getSample(1, (2 * i) + 1);
                
                levelImag = levelImagL - levelImagR;
                levelReal = levelRealL - levelRealR;
                magLin= sqrt( std::pow((levelReal) / 2, 2)
                             + std::pow((levelImag) / 2, 2));
                angPost = std::asin (levelImag / magLin);
                if (levelReal < 0 && levelImag > 0) angPre = -angPost + juce::float_Pi;
                if (levelReal < 0 && levelImag < 0) angPre = -angPost - juce::float_Pi;
                
                phaseDiff = angPre - angPost;
                numOfCompleteCycles = (int) (phaseDiff / juce::float_Pi);
                if (numOfCompleteCycles < -1 ) numOfCompleteCycles = -1;
                d = phaseDiff - (2 * juce::float_Pi * numOfCompleteCycles);
                phaseDiff = d;
                if (d >  juce::float_Pi) d =  juce::float_Pi;
                if (d < -juce::float_Pi) d = -juce::float_Pi;
                phaseDiff= d / (2 * juce::float_Pi);
                
                peakHolder(i, phaseDiff, historyValues2);
                linGain2.push_back(phaseDiff);
                
                break;
            }
        }
    }
}

void PhaseDifference::peakHolder(int index, float& magLin, floatMatrix &historySamples)
{
    if (numOfHistorySamples != numOfHistorySamplesAt.load())
    {
        historyValues1.clear();
        historyValues2.clear();
        numOfHistorySamples = numOfHistorySamplesAt.load();
        int fftSize = fftSizeAt.load();
        for (auto i = 0; i < fftSize; i++)
        {
            historyValues1.push_back (std::vector<float> (numOfHistorySamples, 0));
            historyValues2.push_back (std::vector<float> (numOfHistorySamples, 0));
        }
    }
    
    for (auto i = 0 ; i < numOfHistorySamples; ++i)
    {
        if (i == numOfHistorySamples - 1)
            historySamples[index][i] = magLin;
        else
            historySamples[index][i] = historySamples[index][i + 1];
    }
    
    float averageValue = 0;
    for (auto i = 0; i < numOfHistorySamples; ++i)
        averageValue = historySamples[index][i] + averageValue;
    averageValue = averageValue / numOfHistorySamples;
    
    magLin = averageValue;
}

void PhaseDifference::resized() {}

void PhaseDifference::createNewAxis()
{
    axisImage = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g (axisImage);
    
    int height = getHeight();
    int width  = getWidth();
    
    float sampleRate = sampleRateAt.load();

    float dashedlength[2];
    dashedlength[0] = 4;
    dashedlength[1] = 2;
    
    for (int i = 1; i <= 10; i++){ // plot the labels and the axis
        
        int fo;
        switch (i){
            case 1: fo = 20; break; case 2: fo = 50; break;
            case 3: fo = 100; break; case 4: fo = 200; break;
            case 5: fo = 500; break; case 6: fo = 1000; break;
            case 7: fo = 2000; break; case 8: fo = 5000; break;
            case 9: fo = 10000; break; case 10: fo = 20000; break;
        }
        int freq2draw = (width / std::log((sampleRate / 2) / 10))*std::log(fo / 10);
        g.setColour(Colours::lightgrey);
        g.setOpacity(0.3);
        g.drawVerticalLine(freq2draw, 0, height);
        g.setColour(Colours::white);
        
        
    }
    
    for (int i = 1; i <= 18; i++)
    { //    plot the labels and the axis
        int fo;
        switch (i)
        {
            case 1: fo = 30; break; case 2: fo = 40; break; case 3: fo = 60; break;
            case 4: fo = 70; break; case 5: fo = 80; break; case 6: fo = 90; break;
            case 7: fo = 300; break; case 8: fo = 400; break; case 9: fo = 600; break;
            case 10: fo = 700; break; case 11: fo = 800; break; case 12: fo = 900; break;
            case 13: fo = 3000; break; case 14: fo = 4000; break; case 15: fo = 6000; break;
            case 16: fo = 7000; break; case 17: fo = 8000; break; case 18: fo = 9000; break;
        }
        float freq2draw = (width / std::log((sampleRate / 2) / 10))*std::log(fo / 10);
        g.setColour(Colours::grey);
        g.setOpacity(0.5);
        g.drawDashedLine(juce::Line<float>(freq2draw, height, freq2draw, 0), dashedlength, 2, 1);
    }

    for (int i = 0; i <= 4; i++)
    {
        String fo;
        switch (i){
            case 0: fo = "+""180"; break; case 1: fo = "+""90"; break;
            case 2: fo = "0"; break;   case 3: fo = "-""90"; break;
            case 4: fo = "-""180"; break;
        }
        
        g.setColour(Colours::lightgrey);
        g.setOpacity(0.3);
        g.drawHorizontalLine(height*(i / 4.000000), 30, (float)width - 10);
        
        if (i == 0 || i == 1 ) g.setColour(Colours::palegreen);
        if (i == 2 )           g.setColour(Colours::white);
        if (i == 3 || i == 4 ) g.setColour(Colours::palevioletred);
        
        g.setFont(11.f);
        if (i == 0 || i == 4){
            if (i == 0){
                g.drawText((String)fo, 10, i*(height / 4.000000), 50, 20, juce::Justification::left, true);
            }
            if (i == 4){
                g.drawText((String)fo, 10, i*(height / 4.000000) - 20, 50, 20, juce::Justification::left, true);
            }
        }
        if (i == 2){
            g.setColour(Colours::white);
            g.setOpacity(0.6);
            g.drawHorizontalLine(height*(i / 4.000000), 30, (float)width - 10);
            g.setColour(Colours::white);
            g.setOpacity(1.0f);
            g.drawText((String)fo, 10, i*(height / 4.000000) - 10, 50, 20, juce::Justification::left, true);
        }
        
        if( i== 1 || i == 3 ){
            g.drawText((String)fo, 10, i*(height / 4.000000) - 10, 50, 20, juce::Justification::left, true);
        }
    }
    
    shadeWindow = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g2 (shadeWindow);
    for (int i = 0; i < 20; i++)
    {
        float opacity = (float(i) + 30) / 50;
        g2.setColour(Colours::black.withAlpha(isPositiveAndBelow(1 - opacity, 1) ? 1 - opacity : 0.f));
        g2.drawVerticalLine   (width - i, 0, height);
        g2.drawHorizontalLine (i, 0, width);
        g2.drawHorizontalLine (height - i, 0, width);
    }
    for (int i = 0; i < 50; i++)
    {
        float opacity = (float(i) + 20) / 50.f;
        g2.setColour(Colours::black.withAlpha(isPositiveAndBelow(1 - opacity, 1) ? 1 - opacity : 0.f));
        g2.drawVerticalLine(i, 0, height);
    }

}


//==============================================================================
// STEREO ANALYSER
StereoAnalyser::StereoAnalyser(int sR, int bS, AudioBufferManagement& buffManag, forwardFFT& fFFT)
:
blockSizeAt(bS),
sampleRateAt(sR),
numOfHistorySamplesAt(20),
zoomAt(200),
mainAudioBufferSystem(buffManag),
forwFFT(fFFT)
{
    setTopLeftPosition(729, 89); //7, 263
    setSize (346, 346); // 714,281
    setVisible(true);
    setPaintingIsUnclipped(true);
    setOpaque(true);

    energyPre  = {0, 0};
    energyPost = {0, 0};
    meanValuePre  = 0;
    meanValuePost = 0;
    corrRMSWindowLength = sR * 0.8f;
    
    createNewAxis();
    createCorrelationBackground();
}

StereoAnalyser::~StereoAnalyser()
{
}

void StereoAnalyser::paint (Graphics& g)
{
    createFrame();
    g.drawImage (mainFrame, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
}

void StereoAnalyser::createFrame()
{
    processAllFftData ();
    createCurbes();

    mainFrame = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g (mainFrame);
    
    g.drawImage (axisImage, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
    g.drawImage (lissajouCurbe, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
    g.drawImage (shadeWindow, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
    g.drawImage (correlationBackground, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
    g.drawImage (correlationLines, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
}

void StereoAnalyser::createCurbes ()
{
    int sizeOfQueue = setOfPointsHistDataPre.size();
    int blockSize = blockSizeAt.load();
    float zoom = zoomAt.load();
    
    lissajouCurbe = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Image::BitmapData bitMap (lissajouCurbe, Image::BitmapData::readWrite);

    Colour colPre  = Colours::lightgrey;
    int count = 0;
    for (auto index = sizeOfQueue - 1 ; index >= 0; --index)
    {
        int opacity = (float(count) / (float(sizeOfQueue) + index)) * 255;
        if (sizeOfQueue == 1) opacity = 1.0f;
        for (auto i = 0; i < blockSize; ++i)
        {
            int X = (getWidth() / 2) - (setOfPointsHistDataPre[index][i].first  * zoom);
            int Y = (getWidth() / 2) - (setOfPointsHistDataPre[index][i].second * zoom);
            if ( isPositiveAndBelow (X, getWidth()) && isPositiveAndBelow(Y, getHeight()))
                bitMap.setPixelColour(X, Y, colPre.withAlpha(juce::uint8(opacity/1.5f)));
        }
        count++;
    }
    
    Colour colPost = Colours::whitesmoke;
    count = 0;
    for (auto index = sizeOfQueue - 1 ; index >= 0; --index)
    {
        int opacity = (float(count) / (float(sizeOfQueue) + index)) * 255;
        if (sizeOfQueue == 1) opacity = 1.0f;
        for (auto i = 0; i < blockSize; ++i)
        {
            int X = (getWidth() / 2) - (setOfPointsHistDataPost[index][i].first  * zoom);
            int Y = (getWidth() / 2) - (setOfPointsHistDataPost[index][i].second * zoom);
            if ( isPositiveAndBelow (X, getWidth()) && isPositiveAndBelow(Y, getHeight()))
                bitMap.setPixelColour(X, Y, colPost.withAlpha(juce::uint8(opacity)));
        }
        count++;
    }
    
    correlationLines = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Image::BitmapData bitMap2 (correlationLines, Image::BitmapData::readWrite);
    Colour colCorr;

    float corrPre = setOfCorrelationHistData.corrPre * ((getWidth() - 54) / 2);
    if (corrPre >= 0) colCorr = Colours::palegreen; else colCorr = Colours::palevioletred;
    Colour colCorrPre = colCorr;
    
    float corrPost = setOfCorrelationHistData.corrPost * ((getWidth() - 54) / 2);
    if (corrPost >= 0) colCorr = Colours::palegreen; else colCorr = Colours::palevioletred;
    Colour colCorrPost = colCorr;
    
    float corrRest = setOfCorrelationHistData.corrRest * ((getWidth() - 54) / 4);
    if (corrRest >= 0) colCorr = Colours::palegreen; else colCorr = Colours::palevioletred;
    Colour colCorrRest = colCorr;
    
    int rectAnchor = getHeight() - (15 + getHeight() * .22f);
    int rectHeight = ((getHeight() * 0.22f) / 3) - 7;
    for (auto y = 0; y < rectHeight; ++y)
    {
        if ( abs(corrPre) < getHeight()/2.0f )
            bitMap2.setPixelColour ( (getHeight()/2.0f) - corrPre,      rectAnchor + y + 7, colCorrPre.brighter());
        bitMap2.setPixelColour ( (getHeight()/2.0f) - corrPre - 1,  rectAnchor + y + 7, colCorrPre);
        bitMap2.setPixelColour ( (getHeight()/2.0f) - corrPre + 1,  rectAnchor + y + 7, colCorrPre);
        
        if ( abs(corrPost) < getHeight()/2.0f )
            bitMap2.setPixelColour ( (getHeight()/2.0f) - corrPost, rectAnchor + rectHeight   + y + 14, colCorrPost.brighter());
        bitMap2.setPixelColour ( (getHeight()/2.0f) - corrPost - 1, rectAnchor + rectHeight   + y + 14, colCorrPost.brighter());
        bitMap2.setPixelColour ( (getHeight()/2.0f) - corrPost + 1, rectAnchor + rectHeight   + y + 14, colCorrPost.brighter());
        
        
        if ( abs(corrRest) < (getHeight()/2.0f) - 1 )
            bitMap2.setPixelColour ( (getHeight()/2.0f) - corrRest, rectAnchor + 2*rectHeight + y + 21, colCorrRest.brighter());
        bitMap2.setPixelColour ( (getHeight()/2.0f) - corrRest - 1, rectAnchor + 2*rectHeight + y + 21, colCorrRest.brighter());
        bitMap2.setPixelColour ( (getHeight()/2.0f) - corrRest + 1, rectAnchor + 2*rectHeight + y + 21, colCorrRest.brighter());
    }
}

void StereoAnalyser::processAllFftData ()
{
    const int blockSize = blockSizeAt.load();
    const int numOfHistorySamples = numOfHistorySamplesAt.load();
    const int numOfChannels = mainAudioBufferSystem.bufferPre.historyBuffer.getNumChannels();
    
    std::vector<pairOfXYpoints> setOfXYpointsPre;
    std::vector<pairOfXYpoints> setOfXYpointsPost;
    
    AudioBuffer<float> audioBlockPre (numOfChannels, blockSize);
    audioBlockPre.clear();
    mainAudioBufferSystem.bufferPre.copySamplesFromHistoryBuffer(audioBlockPre, blockSize);
    
    AudioBuffer<float> audioBlockPost (numOfChannels, blockSize);
    audioBlockPost.clear();
    mainAudioBufferSystem.bufferPost.copySamplesFromHistoryBuffer(audioBlockPost, blockSize);


    for (auto i = 0; i < blockSize; ++i)
    {
        // PRE
        float xL =   std::cos(juce::float_Pi / 4) * audioBlockPre.getSample(0, i);
        float yL =   std::sin(juce::float_Pi / 4) * audioBlockPre.getSample(0, i);
        float xR, yR;
        if (audioBlockPre.getNumChannels() > 1) // stereo
        {
            xR = - std::cos(juce::float_Pi / 4) * audioBlockPre.getSample(1, i);
            yR =   std::sin(juce::float_Pi / 4) * audioBlockPre.getSample(1, i);
            setOfXYpointsPre.push_back(pairOfXYpoints(xL + xR, yL + yR));
            meanValuePre = ((audioBlockPre.getSample(0, i) * audioBlockPre.getSample(1, i)))
                                + meanValuePre
                                - (meanValuePre / float(corrRMSWindowLength));
            energyPre.first  = std::pow(audioBlockPre.getSample(0, i), 2)
                                + energyPre.first
                                - ( energyPre.first / float(corrRMSWindowLength) );
            energyPre.second = std::pow(audioBlockPre.getSample(1, i), 2)
                                + energyPre.second
                                - ( energyPre.second / float(corrRMSWindowLength) );
        }
        else // mono
        {
            setOfXYpointsPre.push_back(pairOfXYpoints( 2 * xL, 2 * yL));
            meanValuePre = std::pow(audioBlockPre.getSample(0, i) , 2)
                                    + meanValuePre
                                    - (meanValuePre / float(corrRMSWindowLength));
            energyPre.first  = std::pow(audioBlockPre.getSample(0, i), 2)
                                + energyPre.first
                                - ( energyPre.first / float(corrRMSWindowLength) );
            energyPre.second = std::pow(audioBlockPre.getSample(0, i), 2)
                                + energyPre.second
                                - ( energyPre.second / float(corrRMSWindowLength) );
        }
        
        // POST
        xL =   std::cos(juce::float_Pi / 4) * audioBlockPost.getSample(0, i);
        yL =   std::sin(juce::float_Pi / 4) * audioBlockPost.getSample(0, i);
        if (audioBlockPost.getNumChannels() > 1) // stereo
        {
            xR = - std::cos(juce::float_Pi / 4) * audioBlockPost.getSample(1, i);
            yR =   std::sin(juce::float_Pi / 4) * audioBlockPost.getSample(1, i);
            setOfXYpointsPost.push_back(pairOfXYpoints(xL + xR, yL + yR));
            meanValuePost = ((audioBlockPost.getSample(0, i) * audioBlockPost.getSample(1, i)))
                            + meanValuePost
                            - (meanValuePost / float(corrRMSWindowLength));;
            energyPost.first  = std::pow(audioBlockPost.getSample(0, i), 2)
                            + energyPost.first
                            - ( energyPost.first / float(corrRMSWindowLength) );
            energyPost.second = std::pow(audioBlockPost.getSample(1, i), 2)
                            + energyPost.second
                            - ( energyPost.second / float(corrRMSWindowLength) );
        }
        else // mono
        {
            setOfXYpointsPost.push_back(pairOfXYpoints( 2 * xL, 2 * yL));
            meanValuePost = std::pow(audioBlockPost.getSample(0, i) , 2)
                            + meanValuePost
                            - ( meanValuePost / float(corrRMSWindowLength));
            energyPost.first  = std::pow(audioBlockPost.getSample(0, i), 2)
                                + energyPost.first
                                - ( energyPost.first / float(corrRMSWindowLength) );
            energyPost.second = std::pow(audioBlockPost.getSample(0, i), 2)
                                + energyPost.second
                                - ( energyPost.second / float(corrRMSWindowLength) );
        }
    }
    
    while (setOfPointsHistDataPre.size() >= numOfHistorySamples) setOfPointsHistDataPre.pop_back();
    setOfPointsHistDataPre.push_front(setOfXYpointsPre);
    while (setOfPointsHistDataPost.size() >= numOfHistorySamples) setOfPointsHistDataPost.pop_back();
    setOfPointsHistDataPost.push_front(setOfXYpointsPost);
     
    float corrPre  = (meanValuePre  / float(corrRMSWindowLength)) / std::sqrt((energyPre.first / float(corrRMSWindowLength))  * (energyPre.second / float(corrRMSWindowLength)));
    if (isnan(corrPre)  || isinf(corrPre))  corrPre = 0;
    float corrPost = (meanValuePost / float(corrRMSWindowLength)) / std::sqrt((energyPost.first / float(corrRMSWindowLength)) * (energyPost.second / float(corrRMSWindowLength)));
    if (isnan(corrPost) || isinf(corrPost)) corrPost = 0;
    float corrRest = corrPost - corrPre;
    if (corrPre < -2 || corrPost < -2 || corrRest < -2)
    {
        energyPost.first = 3;
    }
    //if ( setOfCorrelationHistData.size() == numOfHistorySamples )
    setOfCorrelationHistData = {corrPre, corrPost, corrRest};
}

void StereoAnalyser::resized() {}

void StereoAnalyser::createNewAxis()
{
    axisImage = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g (axisImage);
    
    int height = getHeight();
    int width  = getWidth();
    
    g.setColour (Colour (0xff0f0f1c));
    g.fillRoundedRectangle(0, 0, getWidth(), getHeight(), 8.0f);
    
    g.setColour(Colours::white);
    g.setOpacity(0.2f);
    g.drawLine(0, height, width, 0);
    g.drawLine(0, 0, width, height);
    g.drawLine(0, height/2, width, height/2);
    g.drawLine(width/2, 0, width/2, height);
    
    shadeWindow = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g2 (shadeWindow);
    for (int i = 0; i < 20; i++)
    {
        float opacity = (float(i) + 30) / 50;
        g2.setColour(Colours::black.withAlpha(isPositiveAndBelow(1 - opacity, 1) ? 1 - opacity : 0.f));
        g2.drawVerticalLine   (i, 0, height);
        g2.drawVerticalLine   (width - i, 0, height);
        g2.drawHorizontalLine (i, 0, width);
        g2.drawHorizontalLine (height - i, 0, width);
    }
}

void StereoAnalyser::createCorrelationBackground()
{
    correlationBackground = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g (correlationBackground);
    
    int height = getHeight();
    int width  = getWidth();
    int rectHeight = height * 0.22f;
    int rectWidth  = width - 30;

    int X = 15;
    int Y = height - (15 + rectHeight);
    
    g.setColour  (Colours::black);
    g.setOpacity (0.8f);
    g.fillRoundedRectangle(X, Y, rectWidth, rectHeight + 4, 5.f);

    g.setColour  (Colour (0xff42a2c8).withSaturation(0.85f));
    g.setOpacity (1.f);
    g.drawRoundedRectangle(X, Y, rectWidth, rectHeight + 4, 5.f, 1.7f);
    
    g.setColour(Colour (0xff42a2c8).withMultipliedSaturation(0.65f));
    g.setOpacity(1.f);
    g.drawRoundedRectangle( X + 8, Y + 5, rectWidth - 20 + 2, (rectHeight / 3) - 5, 1.5f, 1.5f);
    g.drawRoundedRectangle( X + 8, Y + 10 + ((rectHeight / 3) - 5), rectWidth - 20 + 2, (rectHeight / 3) - 5, 1.5f, 1.5f);
    g.drawRoundedRectangle( X + 8, Y + 15 + (2 * ((rectHeight / 3) - 5)), rectWidth - 20 + 2, (rectHeight / 3) - 5, 1.5f, 1.5f);
}


//==============================================================================
// WAVEFORM ANALYSER
WaveformAnalyser::WaveformAnalyser(int sR, int bS, AudioBufferManagement& buffManag)
:
blockSizeAt(bS),
sampleRateAt(sR),
modeAt(3),
rangeAt(1),
gainAt(1.0f),
axisNeedsUpdate(true),
mainAudioBufferSystem(buffManag)
{
    setTopLeftPosition(9, 89); //7, 263
    setSize (710, 166); // 714,281
    setVisible(true);
    setPaintingIsUnclipped(true);
    setOpaque(true);
    
    rmsWindowLength = sR * 0.4f;
    historyBufferPre   = std::vector<float> ( getWidth(), getHeight());
    historyBufferPost  = std::vector<float> ( getWidth(), getHeight());
    historyBufferMinMaxPre = std::vector<Range<float>> ( getWidth(), Range<float>(getHeight() / 2, getHeight() / 2));
    historyBufferMinMaxPost = std::vector<Range<float>> ( getWidth(), Range<float>(getHeight() / 2, getHeight() / 2));
    rmsGainBuffer  = std::vector<float> ( getWidth(), getHeight() / 2);

    rmsGainPath.preallocateSpace(7 + getWidth());
    
    createNewAxis();
}

WaveformAnalyser::~WaveformAnalyser()
{
}

void WaveformAnalyser::paint (Graphics& g)
{
    createFrame();
    g.drawImage (waveformImage, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
    if (axisNeedsUpdate.load()) createNewAxis();
    g.drawImage (axisImage, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
}

void WaveformAnalyser::createFrame()
{
    processData();
    
    const int width = getWidth();
    const int height = getHeight();
    
    waveformImage = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g (waveformImage);
    
    g.setColour (Colour (0xff0f0f1c));
    g.fillRoundedRectangle(0, 0, getWidth(), getHeight(), 8.0f);
    
    g.setColour  (Colours::lightgrey);
    g.setOpacity (0.3f);
    int x1 = 0;
    for (Range<float> valuesInPixel : historyBufferMinMaxPre)
    {
        g.drawVerticalLine(x1, valuesInPixel.getStart(), height / 2);
        g.drawVerticalLine(x1, height / 2, valuesInPixel.getEnd() );
        ++x1;
    }
    
    g.setColour  (Colours::whitesmoke);
    g.setOpacity (0.85f);
    int x2 = 0;
    for (Range<float> valuesInPixel : historyBufferMinMaxPost)
    {
        g.drawVerticalLine(x2, valuesInPixel.getStart(), height / 2);
        g.drawVerticalLine(x2, height / 2, valuesInPixel.getEnd() );
        ++x2;
    }
    
    rmsGainPath.clear();
    rmsGainPath.startNewSubPath(0, height / 2);
    int x3 = 0;
    for (float valueInPixel : rmsGainBuffer)
    {
        rmsGainPath.lineTo (x3, valueInPixel);
        ++x3;
    }
    rmsGainPath.lineTo(width, height / 2);
    
    g.setColour  (Colour (0xff42a2c8));
    g.setOpacity (1.0f);
    g.strokePath (rmsGainPath, PathStrokeType (2.f, PathStrokeType::beveled));
}

void WaveformAnalyser::processData()
{
    const int width = getWidth();
    const int height = getHeight();
    const int sampleRate = sampleRateAt.load();
    const int numOfChannels = mainAudioBufferSystem.bufferPre.historyBuffer.getNumChannels();
    
    const float gain = gainAt.load();
    
    const int mode = modeAt.load();
    int modeDurationSeconds;
    switch (mode)
    {
        case 1: modeDurationSeconds = 1; break;
        case 2: modeDurationSeconds = 4; break;
        case 3: modeDurationSeconds = 8; break;
        default: modeDurationSeconds = 8; break;
    }
    
    const int range = rangeAt.load();
    int rmsGainCurbeRange;
    switch (range)
    {
        case 1: rmsGainCurbeRange = 12; break;
        case 2: rmsGainCurbeRange = 24; break;
        case 3: rmsGainCurbeRange = 36; break;
        default: modeDurationSeconds = 12; break;
    }

    
    int numOfSamplesInTimeMode = sampleRate * modeDurationSeconds; // 44100 * 2 88200 samples ( 2s of audio )
    int numOfSamplesPerPixel = numOfSamplesInTimeMode / width; // ~ 125 samples / pixel
    
    int numOfNewSamples = sampleRate * ( 43 / 1000.f );
    int numOfNewPixelValues = std::ceil(numOfNewSamples / (float)numOfSamplesPerPixel);
    
    AudioBuffer<float> audioBlockPre (2, numOfNewPixelValues * numOfSamplesPerPixel );
    audioBlockPre.clear();
    mainAudioBufferSystem.bufferPre.copySamplesFromHistoryBuffer(audioBlockPre, numOfNewSamples);
    
    AudioBuffer<float> audioBlockPost (2, numOfNewPixelValues * numOfSamplesPerPixel );
    audioBlockPost.clear();
    mainAudioBufferSystem.bufferPost.copySamplesFromHistoryBuffer(audioBlockPost, numOfNewSamples);
    
    FloatVectorOperations::copy (&rmsGainBuffer[0], &rmsGainBuffer[numOfNewPixelValues], width - numOfNewPixelValues);
    
    for (int i = 0; i < width - numOfNewPixelValues; i++)
    {
        historyBufferMinMaxPre[i]   = historyBufferMinMaxPre  [i + numOfNewPixelValues];
        historyBufferMinMaxPost[i]  = historyBufferMinMaxPost [i + numOfNewPixelValues];
    }
    
    for (auto i = 0; i < numOfNewPixelValues; ++i)
    {
        int index = i * numOfSamplesPerPixel;
        AudioBuffer<float> sampleGroupPre  (2, numOfSamplesPerPixel);
        AudioBuffer<float> sampleGroupPost (2, numOfSamplesPerPixel);
        
        for (auto ch = 0; ch < numOfChannels ; ++ch)
        {
            sampleGroupPre  .copyFrom (ch, 0, audioBlockPre,  ch, index, numOfSamplesPerPixel);
            sampleGroupPost .copyFrom (ch, 0, audioBlockPost, ch, index, numOfSamplesPerPixel);
        }
        FloatVectorOperations::add (sampleGroupPre.getWritePointer(0), sampleGroupPre.getReadPointer(1), numOfSamplesPerPixel);
        FloatVectorOperations::add (sampleGroupPost.getWritePointer(0), sampleGroupPost.getReadPointer(1), numOfSamplesPerPixel);
        
        Range<float> maxMinSampleGroupPre   = FloatVectorOperations::findMinAndMax (sampleGroupPre.getReadPointer(0),  numOfSamplesPerPixel);
        Range<float> maxMinSampleGroupPost  = FloatVectorOperations::findMinAndMax (sampleGroupPost.getReadPointer(0), numOfSamplesPerPixel);
        
        float newStartPre = ( (height / 2) - ( maxMinSampleGroupPre.getEnd() * (height / 2) * gain));
        float newEndPre = ( (height / 2) - ( maxMinSampleGroupPre.getStart() * (height / 2) * gain));
        float newStartPost = ( (height / 2) - ( maxMinSampleGroupPost.getEnd() * (height / 2) * gain));
        float newEndPost = ( (height / 2) - ( maxMinSampleGroupPost.getStart() * (height / 2) * gain));
        
        maxMinSampleGroupPre = Range<float> (newStartPre, newEndPre);
        maxMinSampleGroupPost = Range<float> (newStartPost, newEndPost);
        maxMinSampleGroupPre  .getIntersectionWith (Range<float>(0, height));
        maxMinSampleGroupPost .getIntersectionWith (Range<float>(0, height));
        
        historyBufferMinMaxPre  [ width - i - 1 ] = maxMinSampleGroupPre;
        historyBufferMinMaxPost [ width - i - 1 ] = maxMinSampleGroupPost;
        
        float rmsInBufferPre   = mainAudioBufferSystem.bufferPre  .getRMSMonoValueInSample(index, rmsWindowLength);
        float rmsInBufferPost  = mainAudioBufferSystem.bufferPost .getRMSMonoValueInSample(index, rmsWindowLength);
        
        double linGain = double(rmsInBufferPost) / double(rmsInBufferPre) ;
        if (isinf(linGain)) linGain = 0;
        double gainLog = 20 * log10 (linGain);
        double rmsValuePixel = ( gainLog / rmsGainCurbeRange ) * ( height / 2.f );
        rmsValuePixel = ( height / 2.f) - rmsValuePixel;
        if (! isPositiveAndBelow (rmsValuePixel, height))
        {
            if ( rmsValuePixel < 0 ) rmsValuePixel = 0;
            else                     rmsValuePixel = height - 1;
        }
        rmsGainBuffer [width - i - 1] = rmsValuePixel;
    }
}

void WaveformAnalyser::resized() {}

void WaveformAnalyser::createNewAxis()
{
    axisImage = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g (axisImage);
    int height = getHeight();
    int width = getWidth();
    int horitzCenter = height / 2;
    
    for (int i = 0; i < 50; i++)
    {
        float opacity = (float(i) + 20) / 50.f;
        g.setColour(Colours::black);
        g.setOpacity(isPositiveAndBelow(1 - opacity, 1) ? 1 - opacity : 0.f);
        g.drawVerticalLine(i, 0, height);
    }
    
    const int range = rangeAt.load();
    int rmsGainCurbeRange;
    int slope;
    switch (range)
    {
        case 1: rmsGainCurbeRange = 12; slope = 3;  break;
        case 2: rmsGainCurbeRange = 24; slope = 6;  break;
        case 3: rmsGainCurbeRange = 36; slope = 9;  break;
        default: rmsGainCurbeRange = 12; slope = 3; break;
    }
    for (int i = rmsGainCurbeRange - slope; i >= -(rmsGainCurbeRange - slope) ; i = i - slope)
    {
        float linedB = (height / 2) - ( i / float(rmsGainCurbeRange) ) * horitzCenter;
        g.setColour(Colours::grey);
        g.setOpacity(0.3);
        g.drawHorizontalLine (linedB, 30, width);
        g.setFont(11.f);
        if (i > 0 )
        {
            g.setColour(Colours::palegreen);
            g.drawText ("+" + String(i), 10, linedB - 10, 50, 20, Justification::left, true);
        }
        else if ( i == 0)
        {
            g.setColour(Colours::white);
            g.drawText (String(i), 10, linedB - 10, 50, 20, Justification::left, true);
        }
        else
        {
            g.setColour(Colours::palevioletred);
            g.drawText (String(i), 10, linedB - 10, 50, 20, Justification::left, true);
        }
    }
    
    axisNeedsUpdate.store(false);
}


//==============================================================================
// LEVEL METER
LevelMeter::LevelMeter(int sR, int bS, AudioBufferManagement& buffManag)
:
blockSizeAt(bS),
sampleRateAt(sR),
mainAudioBufferSystem(buffManag)
{
    setTopLeftPosition(1085, 89); //7, 263
    setSize (186, 696); // 714,281
    setVisible(true);
    setPaintingIsUnclipped(true);
    setOpaque(true);
    
    rmsWindowLength = sR * 0.4f;
    createNewAxis();
}

LevelMeter::~LevelMeter()
{
}

void LevelMeter::paint (Graphics& g)
{
    createFrame();
    g.drawImage (waveformImage, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
    g.drawImage (axisImage, 0, 0, getWidth(), getHeight(), 0, 0, getWidth(), getHeight());
}

void LevelMeter::createFrame()
{
    processData();
    const int width = getWidth();
    const int height = getHeight();
    
    waveformImage = Image(Image::PixelFormat::RGB, width, height, true);
    Graphics g (waveformImage);
    
    g.setColour (Colour (0xff0f0f1c));
    g.fillRoundedRectangle(0, 0, width, height, 8.0f);
    
    float maxRangeAbsolute = 60.0f;
    float topPartOffset = 10.f;
    float size0dB = height - topPartOffset;
    float numPixPerDb = size0dB / maxRangeAbsolute;
    float size0dBDiff = (27.0f * numPixPerDb) + topPartOffset;
    float finalPixelValue, rmsLevel, rmsLevelNorm;

    // LEFT CHANNEL FINAL LEVEL (PEAK & RMS)
    rmsLevel = 20 * log10 (setOfValuesToPaint.peakLevelPost.first ) + 3.f;
    rmsLevelNorm =  abs(rmsLevel) / maxRangeAbsolute; // ~-1 , ~0
    finalPixelValue = topPartOffset + ( rmsLevelNorm * size0dB );
    
    g.setColour(Colours::whitesmoke.darker());
    g.fillRect (Rectangle<float>::leftTopRightBottom (60.f, finalPixelValue, (width/2.f) - 1.0f , height));

    rmsLevel = 20 * log10 (setOfValuesToPaint.rmsLevelPost.first) + 3.f;
    rmsLevelNorm =  abs(rmsLevel) / maxRangeAbsolute; // ~-1 , ~0
    finalPixelValue = topPartOffset + ( rmsLevelNorm * size0dB );
    
    g.setColour(Colours::whitesmoke);
    g.fillRect (Rectangle<float>::leftTopRightBottom (60.f, finalPixelValue, (width/2.f) - 1.0f, height));
    
    // RIGHT CHANNEL FINAL LEVEL (PEAK & RMS)
    rmsLevel = 20 * log10 (setOfValuesToPaint.peakLevelPost.second ) + 3.f;
    rmsLevelNorm =  abs(rmsLevel) / maxRangeAbsolute; // ~-1 , ~0
    finalPixelValue = topPartOffset + ( rmsLevelNorm * size0dB );
    
    g.setColour(Colours::whitesmoke.darker());
    g.fillRect (Rectangle<float>::leftTopRightBottom ((width/2.f) + 1.0f, finalPixelValue, width - 60.0f , height));
    
    rmsLevel = 20 * log10 (setOfValuesToPaint.rmsLevelPost.second) + 3.f;
    rmsLevelNorm =  abs(rmsLevel) / maxRangeAbsolute; // ~-1 , ~0
    finalPixelValue = topPartOffset + ( rmsLevelNorm * size0dB );
    
    g.setColour(Colours::whitesmoke);
    g.fillRect (Rectangle<float>::leftTopRightBottom ((width/2.f) + 1.0f, finalPixelValue, width - 60.0f, height));
    
    // LEFT CHANNEL DIFFERENCE LEVEL (PEAK & RMS)
    rmsLevel = 20 * log10 (setOfValuesToPaint.peakLevelDifference.first );
    finalPixelValue = size0dBDiff - (rmsLevel * numPixPerDb);

    rmsLevel > 0 ?
        g.setColour(Colours::palegreen.darker()) :
        g.setColour(Colours::palevioletred.darker());
    rmsLevel > 0 ?
        g.fillRect (Rectangle<float>::leftTopRightBottom (33.f, finalPixelValue, 39.f , size0dBDiff)) :
        g.fillRect (Rectangle<float>::leftTopRightBottom (33.f, size0dBDiff, 39.f , finalPixelValue));

    rmsLevel = 20 * log10 (setOfValuesToPaint.rmsLevelDifference.first);
    finalPixelValue = size0dBDiff - (rmsLevel * numPixPerDb);

    rmsLevel > 0 ?
        g.setColour(Colours::palegreen) :
        g.setColour(Colours::palevioletred);
    rmsLevel > 0 ?
        g.fillRect (Rectangle<float>::leftTopRightBottom (40.f, finalPixelValue, 60.0f - 2.0f , size0dBDiff)) :
        g.fillRect (Rectangle<float>::leftTopRightBottom (40.f, size0dBDiff, 60.0f - 2.0f , finalPixelValue));

    // RIGHT CHANNEL DIFFERENCE LEVEL (PEAK & RMS)
    rmsLevel = 20 * log10 (setOfValuesToPaint.peakLevelDifference.second );
    finalPixelValue = size0dBDiff - (rmsLevel * numPixPerDb);

    rmsLevel > 0 ?
        g.setColour(Colours::palegreen.darker()) :
        g.setColour(Colours::palevioletred.darker());
    rmsLevel > 0 ?
        g.fillRect (Rectangle<float>::leftTopRightBottom (width - 40.f + 1, finalPixelValue, width - 40.f + 7.f , size0dBDiff)) :
        g.fillRect (Rectangle<float>::leftTopRightBottom (width - 40.f + 1, size0dBDiff, width - 40.f + 7.f , finalPixelValue));

    rmsLevel = 20 * log10 (setOfValuesToPaint.rmsLevelDifference.second );
    finalPixelValue = size0dBDiff - (rmsLevel * numPixPerDb);

    rmsLevel > 0 ?
        g.setColour(Colours::palegreen) :
        g.setColour(Colours::palevioletred);
    rmsLevel > 0 ?
        g.fillRect (Rectangle<float>::leftTopRightBottom (width - 60.0f + 2.f, finalPixelValue, width - 40.f , size0dBDiff)) :
        g.fillRect (Rectangle<float>::leftTopRightBottom (width - 60.0f + 2.f, size0dBDiff, width - 40.f , finalPixelValue));

}

void LevelMeter::processData()
{
    const int sampleRate = sampleRateAt.load();
    
    int numOfNewSamples = sampleRate * ( 43 / 1000.f );
    
    // RMS computing on both pre and post signals
    setOfValuesToPaint.rmsLevelPost.first   = mainAudioBufferSystem.bufferPost.getRMSChannelValueInSample(0, rmsWindowLength, 0);
    setOfValuesToPaint.rmsLevelPost.second  = mainAudioBufferSystem.bufferPost.getRMSChannelValueInSample(0, rmsWindowLength, 1);

    // RMS gain computing
    setOfValuesToPaint.rmsLevelDifference.first
        = setOfValuesToPaint.rmsLevelPost.first  / mainAudioBufferSystem.bufferPre.getRMSChannelValueInSample(0, rmsWindowLength, 0);
    setOfValuesToPaint.rmsLevelDifference.second
        = setOfValuesToPaint.rmsLevelPost.second / mainAudioBufferSystem.bufferPre.getRMSChannelValueInSample(0, rmsWindowLength, 1);
    
    // Peak computing for both pre and post signals
    AudioBuffer<float> audioBlockPre (2, numOfNewSamples );
    audioBlockPre.clear();
    mainAudioBufferSystem.bufferPre.copySamplesFromHistoryBuffer(audioBlockPre, numOfNewSamples);
    
    AudioBuffer<float> audioBlockPost (2, numOfNewSamples );
    audioBlockPost.clear();
    mainAudioBufferSystem.bufferPost.copySamplesFromHistoryBuffer(audioBlockPost, numOfNewSamples);
    
    Range<float> maxMinChannel1Post = audioBlockPost.findMinMax(0, 0, numOfNewSamples);
    Range<float> maxMinChannel2Post = audioBlockPost.findMinMax(1, 0, numOfNewSamples);
    Range<float> maxMinChannel1Pre  = audioBlockPre.findMinMax(0, 0, numOfNewSamples);
    Range<float> maxMinChannel2Pre  = audioBlockPre.findMinMax(1, 0, numOfNewSamples);

    float absMaxChannel1Post = abs(maxMinChannel1Post.getEnd()) > abs(maxMinChannel1Post.getStart()) ?
        abs(maxMinChannel1Post.getEnd()) :  abs(maxMinChannel1Post.getStart());
    float absMaxChannel2Post = abs(maxMinChannel2Post.getEnd()) > abs(maxMinChannel2Post.getStart()) ?
        abs(maxMinChannel2Post.getEnd()) :  abs(maxMinChannel2Post.getStart());
    float absMaxChannel1Pre = abs(maxMinChannel1Pre.getEnd()) > abs(maxMinChannel1Pre.getStart()) ?
        abs(maxMinChannel1Pre.getEnd()) :  abs(maxMinChannel1Pre.getStart());
    float absMaxChannel2Pre = abs(maxMinChannel2Pre.getEnd()) > abs(maxMinChannel2Pre.getStart()) ?
        abs(maxMinChannel2Pre.getEnd()) :  abs(maxMinChannel2Pre.getStart());
    
    // Peak gain computing
    float differenceChannel1 = absMaxChannel1Post / absMaxChannel1Pre;
    float differenceChannel2 = absMaxChannel2Post / absMaxChannel2Pre;

    // RMS gain peak holder computing
    setOfValuesToPaint.peakLevelPost.first   = absMaxChannel1Post > setOfValuesToPaint.oldPeakLevelPost.first ?
        absMaxChannel1Post : setOfValuesToPaint.oldPeakLevelPost.first;
    setOfValuesToPaint.peakLevelPost.second  = absMaxChannel2Post > setOfValuesToPaint.oldPeakLevelPost.second ?
    absMaxChannel2Post : setOfValuesToPaint.oldPeakLevelPost.second;
    
    setOfValuesToPaint.oldPeakLevelPost.first   = setOfValuesToPaint.peakLevelPost.first   * 0.90f;
    setOfValuesToPaint.oldPeakLevelPost.second  = setOfValuesToPaint.peakLevelPost.second  * 0.90f;
    
    // Peak gain peak holder computing
//    setOfValuesToPaint.oldPeakLevelDifference.first   = differenceChannel1 > setOfValuesToPaint.oldPeakLevelDifference.first ?
//        differenceChannel1 : setOfValuesToPaint.oldPeakLevelDifference.first;
//    setOfValuesToPaint.oldPeakLevelDifference.second  = differenceChannel2 > setOfValuesToPaint.oldPeakLevelDifference.second ?
//        differenceChannel2 : setOfValuesToPaint.oldPeakLevelDifference.second;
//
//    setOfValuesToPaint.oldPeakLevelDifference.first   = setOfValuesToPaint.oldPeakLevelDifference.first   * 0.90f;
//    setOfValuesToPaint.oldPeakLevelDifference.second  = setOfValuesToPaint.oldPeakLevelDifference.second  * 0.90f;
    
    setOfValuesToPaint.peakLevelDifference.first   = differenceChannel1;
    setOfValuesToPaint.peakLevelDifference.second  = differenceChannel2;
}

void LevelMeter::resized() {}

void LevelMeter::createNewAxis()
{
    axisImage = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g (axisImage);
    int height = getHeight();
    int width = getWidth();
    int horitzCenter = (height - 10.f) / 2.f;
    int topPartOffset = 10;
    float maxRangeAbsolute = 60.f;
    float numPixPerDb = float(height - topPartOffset) / maxRangeAbsolute;
//
    int gainLabelNumber = 27;
    for (int i = 0; i <= 54; i += 9)
    {
        // LINES
        g.setColour(Colours::dimgrey);
        if (i == 27)
        {
            g.setColour(Colours::lightgrey);
            g.fillRect (30.f, topPartOffset + float(numPixPerDb * i), width - 60.f, 2.f);
        }
        else g.drawHorizontalLine (topPartOffset + (numPixPerDb * i), 30, width - 30);
        
        // LEVEL LABELS
        String numToDraw;
        if (i == 0) numToDraw = "+0";
        else numToDraw = "-" + String(i);
        g.setColour(Colours::white);
        g.setFont(11.f);
        g.drawText( numToDraw, 10, topPartOffset + (numPixPerDb * i) - 5, 50, 20, Justification::topLeft, true);
        g.drawText( numToDraw, width - 24, topPartOffset + (numPixPerDb * i) - 5, 50, 20, Justification::topLeft, true);
        
        // GAIN LABELS
        String gainToDraw;
        if (i < 27)
        {
            g.setColour(Colours::palegreen);
            gainToDraw = "+" + String(gainLabelNumber);
            g.drawText( gainToDraw, 10, topPartOffset + (numPixPerDb * i) + 10, 50, 20, Justification::topLeft, true);
            g.drawText( gainToDraw, width - 24, topPartOffset + (numPixPerDb * i) + 10, 50, 20, Justification::topLeft, true);
        }
        if (i > 27)
        {
            g.setColour(Colours::palevioletred);
            gainToDraw = String(gainLabelNumber);
            g.drawText( gainToDraw, 10, topPartOffset + (numPixPerDb * i) + 10, 50, 20, Justification::topLeft, true);
            g.drawText( gainToDraw, width - 24, topPartOffset + (numPixPerDb * i) + 10, 50, 20, Justification::topLeft, true);
        }
        gainLabelNumber -= 9;
    }
    
    for (int i = 0; i <= 60; i += 3)
    {
        // SEMI LINES
        g.setColour(Colours::dimgrey);
        if ( i % 9 != 0)
        {
            g.drawHorizontalLine (topPartOffset + (numPixPerDb * i), 10, 25);
            g.drawHorizontalLine (topPartOffset + (numPixPerDb * i), width - 25, width - 10);
        }
    }

    
}


