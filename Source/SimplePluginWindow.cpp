#include "SimplePluginWindow.h"

SimplePluginWindow::SimplePluginWindow (Component* const uiComp, SimplePluginWindow* owner_, const bool isGeneric_, AudioProcessorGraph::Node* node)
    : DocumentWindow (uiComp->getName(), Colours::lightblue,DocumentWindow::minimiseButton | DocumentWindow::closeButton),
    owner (owner_),
    node (node),
    isGeneric (isGeneric_)
{
    setUsingNativeTitleBar(true);
        
    setContentOwned (uiComp, true);
    
    setTopLeftPosition (node->properties.getWithDefault ("uiLastX", Random::getSystemRandom().nextInt (500)),
                      node->properties.getWithDefault ("uiLastY", Random::getSystemRandom().nextInt (500)));
    setVisible (true);
    
    setAlwaysOnTop(true);
    
}

SimplePluginWindow::~SimplePluginWindow()
{
    DBG("Deleting simple plugin window...");
}

void SimplePluginWindow::moved()
{
    node->properties.set ("uiLastX", getX());
    node->properties.set ("uiLastY", getY());
}

void SimplePluginWindow::closeButtonPressed()
{
    DBG("SimplePluginWindow::closeButtonPressed");
    setVisible(false);
}

void SimplePluginWindow::bringToFront()
{
    
    setVisible(true);
    toFront(false);
}

SimplePluginWindow* SimplePluginWindow::getWindowFor (AudioProcessorGraph::Node* node,
	bool useGenericView, SimplePluginWindow* owner)
{
    
    AudioProcessorEditor* ui = nullptr;
    
    if (! useGenericView)
    {
        ui = node->getProcessor()->createEditorIfNeeded();
        
        if (ui == nullptr)
            useGenericView = true;
    }
    
    if (useGenericView)
        ui = new GenericAudioProcessorEditor (node->getProcessor());
    
    if (ui != nullptr)
    {
        AudioPluginInstance* const plugin = dynamic_cast <AudioPluginInstance*> (node->getProcessor());
        
        if (plugin != nullptr)
            ui->setName (plugin->getName());
        
        return new SimplePluginWindow (ui, owner, useGenericView, node);
    }
    
    return nullptr;
}

