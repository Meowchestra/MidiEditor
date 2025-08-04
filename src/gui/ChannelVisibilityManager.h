#ifndef CHANNELVISIBILITYMANAGER_H
#define CHANNELVISIBILITYMANAGER_H

/**
 * \brief Global channel visibility manager
 * 
 * This class provides corruption-proof channel visibility management
 * that doesn't depend on potentially corrupted MidiChannel objects.
 */
class ChannelVisibilityManager {
public:
    /**
     * \brief Gets the singleton instance
     * \return Reference to the global visibility manager
     */
    static ChannelVisibilityManager &instance();

    /**
     * \brief Checks if a channel is visible
     * \param channel The channel number (0-18)
     * \return True if the channel is visible, false otherwise
     */
    bool isChannelVisible(int channel);

    /**
     * \brief Sets the visibility of a channel
     * \param channel The channel number (0-18)
     * \param visible True to make the channel visible, false to hide it
     */
    void setChannelVisible(int channel, bool visible);

    /**
     * \brief Resets all channels to visible
     */
    void resetAllVisible();

private:
    ChannelVisibilityManager();

    /** \brief Channel visibility storage (corruption-proof) */
    bool channelVisibility[19];
};

#endif // CHANNELVISIBILITYMANAGER_H
