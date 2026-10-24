#pragma once
#include "PanelDriver.h"
#include "Board7BProfile.h"
class Board7BPanel:public PanelDriver{public:WT_OFFLINE_LINK bool begin(const Board7BProfile&);};
