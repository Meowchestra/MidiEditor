#include "ChannelVisibilityManager.h"

ChannelVisibilityManager::ChannelVisibilityManager() {
    // Initialize all channels as visible
    for (int i = 0; i < 19; i++) {
        channelVisibility[i] = true;
    }
}

ChannelVisibilityManager& ChannelVisibilityManager::instance() {
    static ChannelVisibilityManager instance;
    return instance;
}

bool ChannelVisibilityManager::isChannelVisible(int channel) {
    // Bounds checking
    if (channel < 0 || channel >= 19) {
        return true; // Default to visible for invalid channels
    }
    
    // Special inheritance: channels > 16 inherit from channel 16
    if (channel > 16) {
        return channelVisibility[16];
    }
    
    return channelVisibility[channel];
}

void ChannelVisibilityManager::setChannelVisible(int channel, bool visible) {
    // Bounds checking
    if (channel < 0 || channel >= 19) {
        return; // Ignore invalid channels
    }
    
    channelVisibility[channel] = visible;
}

void ChannelVisibilityManager::resetAllVisible() {
    for (int i = 0; i < 19; i++) {
        channelVisibility[i] = true;
    }
}
