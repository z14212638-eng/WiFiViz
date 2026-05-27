#pragma once

#include "ppdu_visual_item.h"

struct ChartFilter
{
    int nodeId = -1;
    int linkId = -1;

    bool accepts(const PpduVisualItem& item) const
    {
        if (nodeId >= 0 && item.nodeId != static_cast<uint16_t>(nodeId))
        {
            return false;
        }
        if (linkId >= 0 && item.linkId != static_cast<uint8_t>(linkId))
        {
            return false;
        }
        return true;
    }
};
