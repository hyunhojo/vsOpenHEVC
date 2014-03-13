#ifndef AVCODEC_MPEG12DATA_H
#define AVCODEC_MPEG12DATA_H

#include <stdint.h>
#include "libavutil/rational.h"
#include "rl.h"



extern RLTable ff_rl_mpeg1;
extern RLTable ff_rl_mpeg2;

extern const uint8_t ff_mpeg12_mbAddrIncrTable[36][2];
extern const uint8_t ff_mpeg12_mbPatTable[64][2];

extern const uint8_t ff_mpeg12_mbMotionVectorTable[17][2];

extern const AVRational ff_mpeg12_frame_rate_tab[];

extern const float ff_mpeg1_aspect[16];
extern const AVRational ff_mpeg2_aspect[16];

#endif /* AVCODEC_MPEG12DATA_H */
