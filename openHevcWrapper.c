#include <stdio.h>
#include "openHevcWrapper.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"

typedef struct OpenHevcWrapperContext {
    AVCodec *codec;
    AVCodecContext *c;
    AVFrame *picture;
    AVPacket avpkt;
    AVCodecParserContext *parser;
} OpenHevcWrapperContext;

OpenHevc_Handle libOpenHevcInit(int nb_pthreads)
{
    OpenHevcWrapperContext * openHevcContext;

    /* register all the codecs */
    avcodec_register_all();

    openHevcContext = av_malloc(sizeof(OpenHevcWrapperContext));
    av_init_packet(&openHevcContext->avpkt);
    openHevcContext->codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    if (!openHevcContext->codec) {
        fprintf(stderr, "codec not found\n");
        return NULL;
    }
    openHevcContext->parser  = av_parser_init( openHevcContext->codec->id );
    openHevcContext->c       = avcodec_alloc_context3(openHevcContext->codec);
    openHevcContext->picture = avcodec_alloc_frame();
    
    
    if(openHevcContext->codec->capabilities&CODEC_CAP_TRUNCATED)
        openHevcContext->c->flags |= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

    /* For some codecs, such as msmpeg4 and mpeg4, width and height
         MUST be initialized there because this information is not
         available in the bitstream. */

    /* open it */
    if(nb_pthreads)	{
        if ((nb_pthreads & 256)  != 0)
            av_opt_set(openHevcContext->c, "thread_type", "frame", 0);
        else
            av_opt_set(openHevcContext->c, "thread_type", "slice", 0);
        av_opt_set_int(openHevcContext->c, "threads", nb_pthreads& 255, 0);
    }
    if (avcodec_open2(openHevcContext->c, openHevcContext->codec, NULL) < 0) {
        fprintf(stderr, "could not open codec\n");
        return NULL;
    }
    av_opt_set_int(openHevcContext->c->priv_data, "disable-au", 0, 0);
    return (OpenHevc_Handle) openHevcContext;
}

int libOpenHevcDecode(OpenHevc_Handle openHevcHandle, const unsigned char *buff, int au_len, int64_t pts)
{
    int got_picture, len;
    OpenHevcWrapperContext * openHevcContext = (OpenHevcWrapperContext *) openHevcHandle;
    openHevcContext->avpkt.size = au_len;
    openHevcContext->avpkt.data = buff;
    openHevcContext->avpkt.pts  = pts;
    len = avcodec_decode_video2(openHevcContext->c, openHevcContext->picture, &got_picture, &openHevcContext->avpkt);
    if (len < 0) {
        fprintf(stderr, "Error while decoding frame \n");
        return -1;
    }
    return got_picture;
}

void libOpenHevcGetPictureInfo(OpenHevc_Handle openHevcHandle, OpenHevc_FrameInfo *openHevcFrameInfo)
{
    OpenHevcWrapperContext * openHevcContext = (OpenHevcWrapperContext *) openHevcHandle;
    AVFrame *picture              = openHevcContext->picture;
    openHevcFrameInfo->nYPitch    = openHevcContext->c->width;
    openHevcFrameInfo->nUPitch    = openHevcContext->c->width>>1;
    openHevcFrameInfo->nVPitch    = openHevcContext->c->width>>1;

    openHevcFrameInfo->nBitDepth  = 8;

    openHevcFrameInfo->nWidth     = openHevcContext->c->width;
    openHevcFrameInfo->nHeight    = openHevcContext->c->height;

    openHevcFrameInfo->sample_aspect_ratio.num = picture->sample_aspect_ratio.num;
    openHevcFrameInfo->sample_aspect_ratio.den = picture->sample_aspect_ratio.den;
    openHevcFrameInfo->frameRate.num  = 0;
    openHevcFrameInfo->frameRate.den  = 0;
    openHevcFrameInfo->display_picture_number = picture->display_picture_number;
    openHevcFrameInfo->flag       = 0; //progressive, interlaced, interlaced top field first, interlaced bottom field first.
    openHevcFrameInfo->nTimeStamp = picture->pts;
}

void libOpenHevcGetPictureSize2(OpenHevc_Handle openHevcHandle, OpenHevc_FrameInfo *openHevcFrameInfo)
{
    OpenHevcWrapperContext * openHevcContext = (OpenHevcWrapperContext *) openHevcHandle;
    libOpenHevcGetPictureInfo(openHevcHandle, openHevcFrameInfo);
    openHevcFrameInfo->nYPitch = openHevcContext->picture->linesize[0];
    openHevcFrameInfo->nUPitch = openHevcContext->picture->linesize[1];
    openHevcFrameInfo->nVPitch = openHevcContext->picture->linesize[2];
}

int libOpenHevcGetOutput(OpenHevc_Handle openHevcHandle, int got_picture, OpenHevc_Frame *openHevcFrame)
{
    OpenHevcWrapperContext * openHevcContext = (OpenHevcWrapperContext *) openHevcHandle;
    if (got_picture) {
        openHevcFrame->pvY       = (void *) openHevcContext->picture->data[0];
        openHevcFrame->pvU       = (void *) openHevcContext->picture->data[1];
        openHevcFrame->pvV       = (void *) openHevcContext->picture->data[2];
        libOpenHevcGetPictureInfo(openHevcHandle, &openHevcFrame->frameInfo);
    }
    return 1;
}

int libOpenHevcGetOutputCpy(OpenHevc_Handle openHevcHandle, int got_picture, OpenHevc_Frame_cpy *openHevcFrame)
{
    int y;
    int y_offset, y_offset2;
    OpenHevcWrapperContext * openHevcContext = (OpenHevcWrapperContext *) openHevcHandle;
    if( got_picture ) {
        unsigned char *Y = (unsigned char *) openHevcFrame->pvY;
        unsigned char *U = (unsigned char *) openHevcFrame->pvU;
        unsigned char *V = (unsigned char *) openHevcFrame->pvV;
        y_offset = y_offset2 = 0;
        for(y = 0; y < openHevcContext->c->height; y++) {
            memcpy(&Y[y_offset2], &openHevcContext->picture->data[0][y_offset], openHevcContext->c->width);
            y_offset  += openHevcContext->picture->linesize[0];
            y_offset2 += openHevcContext->c->width;
        }
        y_offset = y_offset2 = 0;
        for(y = 0; y < openHevcContext->c->height/2; y++) {
            memcpy(&U[y_offset2], &openHevcContext->picture->data[1][y_offset], openHevcContext->c->width/2);
            memcpy(&V[y_offset2], &openHevcContext->picture->data[2][y_offset], openHevcContext->c->width/2);
            y_offset  += openHevcContext->picture->linesize[1];
            y_offset2 += openHevcContext->c->width / 2;
        }
        libOpenHevcGetPictureInfo(openHevcHandle, &openHevcFrame->frameInfo);
    }
    return 1;
}

void libOpenHevcSetCheckMD5(OpenHevc_Handle openHevcHandle, int val)
{
    OpenHevcWrapperContext * openHevcContext = (OpenHevcWrapperContext *) openHevcHandle;
    av_opt_set_int(openHevcContext->c->priv_data, "decode-checksum", val, 0);
}
void libOpenHevcSetDisableAU(OpenHevc_Handle openHevcHandle, int val)
{
    OpenHevcWrapperContext * openHevcContext = (OpenHevcWrapperContext *) openHevcHandle;
    av_opt_set_int(openHevcContext->c->priv_data, "disable-au", val, 0);
}

void libOpenHevcSetTemporalLayer_id(OpenHevc_Handle openHevcHandle, int val)
{
    OpenHevcWrapperContext * openHevcContext = (OpenHevcWrapperContext *) openHevcHandle;
    av_opt_set_int(openHevcContext->c->priv_data, "temporal-layer-id", val+1, 0);
}

void libOpenHevcSetNoCropping(OpenHevc_Handle openHevcHandle, int val)
{
    OpenHevcWrapperContext * openHevcContext = (OpenHevcWrapperContext *) openHevcHandle;
    av_opt_set_int(openHevcContext->c->priv_data, "no-cropping", val, 0);
}

void libOpenHevcClose(OpenHevc_Handle openHevcHandle)
{
    OpenHevcWrapperContext * openHevcContext = (OpenHevcWrapperContext *) openHevcHandle;
    avcodec_close(openHevcContext->c);
    av_free(openHevcContext->c);
    av_free(openHevcContext->picture);
    av_free(openHevcContext);

    // FIXME : temporally remove 
    //av_parser_close(openHevcContext->parser);
}

void libOpenHevcFlush(OpenHevc_Handle openHevcHandle)
{
    OpenHevcWrapperContext * openHevcContext = (OpenHevcWrapperContext *) openHevcHandle;
    openHevcContext->codec->flush(openHevcContext->c);
}

const char *libOpenHevcVersion(OpenHevc_Handle openHevcHandle)
{
    return "OpenHEVC v"NV_VERSION;
}

