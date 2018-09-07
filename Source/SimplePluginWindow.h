#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

class SimplePluginWindow  : public DocumentWindow,
                            public ReferenceCountedObject
{
    SimplePluginWindow (Component* const uiComp,
                        SimplePluginWindow* owner_,
						const bool isGeneric_,
                        AudioProcessorGraph::Node* node);

public:
    
    ~SimplePluginWindow();
    
    void moved();
    void closeButtonPressed();
    void bringToFront();
    
    static SimplePluginWindow* getWindowFor (AudioProcessorGraph::Node* node,
    bool useGenericView, SimplePluginWindow* owner);

private:
    
	SimplePluginWindow* owner;
	AudioProcessorGraph::Node* node;
    bool isGeneric;

    
};


