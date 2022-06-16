/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GainControlAudioProcessor::GainControlAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),    
    parameters(*this, nullptr, "Parameters",
        {
            std::make_unique<juce::AudioParameterFloat>("gain",
                                                        "Gain",
                                                        -25.0f,
                                                        5.0f,
                                                        0.0f),
            std::make_unique<juce::AudioParameterFloat>("leftGain",
                                                        "LeftGain",
                                                        -25.0f,
                                                        5.0f,
                                                        0.0f),
            std::make_unique<juce::AudioParameterFloat>("rightGain",
                                                        "RightGain",
                                                        -25.0f,
                                                        5.0f,
                                                        0.0f),
            std::make_unique<juce::AudioParameterBool>("invertPhase",
                                                       "InvertPhase",
                                                       false),
            std::make_unique<juce::AudioParameterBool>("mono",
                                                       "Mono",
                                                       false),
            std::make_unique<juce::AudioParameterBool>("swapLR",
                                                       "SwapLR",
                                                       false)

        })
#endif
{
    gainParameter = parameters.getRawParameterValue("gain");
    leftGainParameter = parameters.getRawParameterValue("leftGain");
    rightGainParameter = parameters.getRawParameterValue("rightGain");
    phaseParameter = parameters.getRawParameterValue("invertPhase");
    monoParameter = parameters.getRawParameterValue("mono");
    swapLR_Parameter = parameters.getRawParameterValue("swapLR");
}

GainControlAudioProcessor::~GainControlAudioProcessor()
{
}

//==============================================================================
const juce::String GainControlAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool GainControlAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool GainControlAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool GainControlAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double GainControlAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int GainControlAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int GainControlAudioProcessor::getCurrentProgram()
{
    return 0;
}

void GainControlAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String GainControlAudioProcessor::getProgramName (int index)
{
    return {};
}

void GainControlAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void GainControlAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    auto phase = *phaseParameter < 0.5f ? 1.0f : -1.0f;
    previousGain = juce::Decibels::decibelsToGain<float>(*gainParameter) * phase;
    previousLeftGain = juce::Decibels::decibelsToGain<float>(*leftGainParameter) * phase;
    previousRightGain = juce::Decibels::decibelsToGain<float>(*rightGainParameter) * phase;
}

void GainControlAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool GainControlAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
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

void GainControlAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.

    auto phase = *phaseParameter < 0.5f ? 1.0f : -1.0f;
    auto currentGain = juce::Decibels::decibelsToGain<float>(*gainParameter) * phase;
    auto currentLeftGain = juce::Decibels::decibelsToGain<float>(*leftGainParameter);
    auto currentRightGain = juce::Decibels::decibelsToGain<float>(*rightGainParameter);

    if (gainsChanged(currentGain, currentLeftGain, currentRightGain)) {
        buffer.applyGainRamp(0, buffer.getNumSamples(), previousGain, currentGain);
        buffer.applyGainRamp(0, 0, buffer.getNumSamples(), previousLeftGain, currentLeftGain);
        buffer.applyGainRamp(1, 0, buffer.getNumSamples(), previousRightGain, currentRightGain);
        previousGain = currentGain;
        previousLeftGain = currentLeftGain;
        previousRightGain = currentRightGain;
    }else {
        buffer.applyGain(currentGain);
        buffer.applyGain(0, 0, buffer.getNumSamples(), currentLeftGain);
        buffer.applyGain(1, 0, buffer.getNumSamples(), currentRightGain);   
    }    

    auto* leftChannelData = buffer.getWritePointer(0);
    auto* rightChannelData = buffer.getWritePointer(1);

    int mono = *monoParameter < 0.5f ? 0 : 1;
    if (mono) {
        float aux;
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            aux = leftChannelData[sample];
            leftChannelData[sample] = (rightChannelData[sample] + leftChannelData[sample]) / 2;
            rightChannelData[sample] = (rightChannelData[sample] + aux) / 2;
        }
    }else {
        int swapLR = *swapLR_Parameter < 0.5f ? 0 : 1;
        if (swapLR && totalNumInputChannels == 2 && totalNumOutputChannels == 2) {            
            float aux;
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                aux = leftChannelData[sample];
                leftChannelData[sample] = rightChannelData[sample];
                rightChannelData[sample] = aux;
            }
        }
    }

}

//==============================================================================
bool GainControlAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* GainControlAudioProcessor::createEditor()
{
    //return new GainControlAudioProcessorEditor(*this);
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void GainControlAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void GainControlAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.

    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

float GainControlAudioProcessor::getGain() {
    return *gainParameter;
}

bool GainControlAudioProcessor::gainsChanged(float currentGain, float currentLeftGain, float currentRightGain) {
    if (previousGain != currentGain || previousLeftGain != currentLeftGain || previousRightGain != currentRightGain) return true;
    return false;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GainControlAudioProcessor();
}

