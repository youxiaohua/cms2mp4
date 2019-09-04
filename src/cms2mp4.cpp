#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <faac.h>
#include <string.h>

#include "cms_parser.h"
#include "mp4v2/mp4v2.h"



typedef struct {

    int           Width;
    int           Height;
    int           VideoRate;
    int           AudioRate;
   
    int           VideoSize;
    MP4TrackId    VideoId;
    MP4TrackId    AudioId;
    MP4FileHandle handle;
}MP4_INFORMATION;

 

typedef struct {
    faacEncHandle handle;           //文件操作句柄
    unsigned long SampleRate;       //采样率
    unsigned int  Channels;         //单双声道
    unsigned int  PCMBitSize;       //pcm数据位数
    unsigned int  PCMBufferSize;     //pcm数据大小
    unsigned long InputSamples;     //编码数据的长度
    unsigned long MaxOutputBytes;   //编码后数据的最大大小
    short    int  *PCMBuffer;       //指向存放pcm数据的内存
    unsigned int  pcm_ptr;          

}FAAC_INFORMATION;

 unsigned short ALawDecompressTable[] =
{
    0xEA80, 0xEB80, 0xE880, 0xE980, 0xEE80, 0xEF80, 0xEC80, 0xED80,
    0xE280, 0xE380, 0xE080, 0xE180, 0xE680, 0xE780, 0xE480, 0xE580,
    0xF540, 0xF5C0, 0xF440, 0xF4C0, 0xF740, 0xF7C0, 0xF640, 0xF6C0,
    0xF140, 0xF1C0, 0xF040, 0xF0C0, 0xF340, 0xF3C0, 0xF240, 0xF2C0,
    0xAA00, 0xAE00, 0xA200, 0xA600, 0xBA00, 0xBE00, 0xB200, 0xB600,
    0x8A00, 0x8E00, 0x8200, 0x8600, 0x9A00, 0x9E00, 0x9200, 0x9600,
    0xD500, 0xD700, 0xD100, 0xD300, 0xDD00, 0xDF00, 0xD900, 0xDB00,
    0xC500, 0xC700, 0xC100, 0xC300, 0xCD00, 0xCF00, 0xC900, 0xCB00,
    0xFEA8, 0xFEB8, 0xFE88, 0xFE98, 0xFEE8, 0xFEF8, 0xFEC8, 0xFED8,
    0xFE28, 0xFE38, 0xFE08, 0xFE18, 0xFE68, 0xFE78, 0xFE48, 0xFE58,
    0xFFA8, 0xFFB8, 0xFF88, 0xFF98, 0xFFE8, 0xFFF8, 0xFFC8, 0xFFD8,
    0xFF28, 0xFF38, 0xFF08, 0xFF18, 0xFF68, 0xFF78, 0xFF48, 0xFF58,
    0xFAA0, 0xFAE0, 0xFA20, 0xFA60, 0xFBA0, 0xFBE0, 0xFB20, 0xFB60,
    0xF8A0, 0xF8E0, 0xF820, 0xF860, 0xF9A0, 0xF9E0, 0xF920, 0xF960,
    0xFD50, 0xFD70, 0xFD10, 0xFD30, 0xFDD0, 0xFDF0, 0xFD90, 0xFDB0,
    0xFC50, 0xFC70, 0xFC10, 0xFC30, 0xFCD0, 0xFCF0, 0xFC90, 0xFCB0,
    0x1580, 0x1480, 0x1780, 0x1680, 0x1180, 0x1080, 0x1380, 0x1280,
    0x1D80, 0x1C80, 0x1F80, 0x1E80, 0x1980, 0x1880, 0x1B80, 0x1A80,
    0x0AC0, 0x0A40, 0x0BC0, 0x0B40, 0x08C0, 0x0840, 0x09C0, 0x0940,
    0x0EC0, 0x0E40, 0x0FC0, 0x0F40, 0x0CC0, 0x0C40, 0x0DC0, 0x0D40,
    0x5600, 0x5200, 0x5E00, 0x5A00, 0x4600, 0x4200, 0x4E00, 0x4A00,
    0x7600, 0x7200, 0x7E00, 0x7A00, 0x6600, 0x6200, 0x6E00, 0x6A00,
    0x2B00, 0x2900, 0x2F00, 0x2D00, 0x2300, 0x2100, 0x2700, 0x2500,
    0x3B00, 0x3900, 0x3F00, 0x3D00, 0x3300, 0x3100, 0x3700, 0x3500,
    0x0158, 0x0148, 0x0178, 0x0168, 0x0118, 0x0108, 0x0138, 0x0128,
    0x01D8, 0x01C8, 0x01F8, 0x01E8, 0x0198, 0x0188, 0x01B8, 0x01A8,
    0x0058, 0x0048, 0x0078, 0x0068, 0x0018, 0x0008, 0x0038, 0x0028,
    0x00D8, 0x00C8, 0x00F8, 0x00E8, 0x0098, 0x0088, 0x00B8, 0x00A8,
    0x0560, 0x0520, 0x05E0, 0x05A0, 0x0460, 0x0420, 0x04E0, 0x04A0,
    0x0760, 0x0720, 0x07E0, 0x07A0, 0x0660, 0x0620, 0x06E0, 0x06A0,
    0x02B0, 0x0290, 0x02F0, 0x02D0, 0x0230, 0x0210, 0x0270, 0x0250,
    0x03B0, 0x0390, 0x03F0, 0x03D0, 0x0330, 0x0310, 0x0370, 0x0350,
};


//全局变量
bool ISVIDEO            = true;        //判断接受的数据是视频还是音频
bool FRIST_WRITE_VIDEO  = true;        //判断是否第一次写入视频数据
FAAC_INFORMATION *faac  = NULL;        //结构体存放faac库解码需要的信息
MP4_INFORMATION  *mp4v2 = NULL;        //结构体存放MP4v2库解码需要的信息
unsigned char *VideoData;              //指向一块用来存放接受视频数据的信息



int Get_Nalu(unsigned char *data, int surplus_len)
{
    int ptr = 0;
    if (data[ptr] == 0 && data[ptr + 1] == 0 && data[ptr + 2] == 0 && data[ptr + 3] == 1) {
        while (surplus_len > 0) {
            ptr++;
            surplus_len--;
            if (data[ptr] == 0 && data[ptr + 1] == 0 && data[ptr + 2] == 0 && data[ptr + 3] == 1) {
                return ptr;
            }
        }
        return ptr+1;
    } else {
        return -1;
    }
}

int Write_Mp4(unsigned char *data, int size)
{
    int start    = 0;
    while(size > 0) {
        int len = Get_Nalu(data, size);
        if (len == -1 ) {
            return len;            
        }
        unsigned char *nalu = data + 4;
        char NaluType = nalu[0]&0x1f;
        
        switch (NaluType)
        {
        case 0x07: {// SPS
            if ( FRIST_WRITE_VIDEO ) {
                printf("sps(%d)\n", len);
                mp4v2->VideoId = MP4AddH264VideoTrack
                    (mp4v2->handle, 
                     90000,                            // 一秒钟多少timescale
                     6000,
                     mp4v2->Width,                     // width
                     mp4v2->Height,                    // height 
                     nalu[1],                  // sps[1] AVCProfileIndication
                     nalu[2],                  // sps[2] profile_compat
                     nalu[3],                  // sps[3] AVCLevelIndication
                     3);                               // 4 bytes length before each NAL unit
                if ( mp4v2->VideoId == MP4_INVALID_TRACK_ID )
                {
                    printf("Create video track fail\n");
                    return -1;
                }
                
                MP4AddH264SequenceParameterSet( mp4v2->handle, mp4v2->VideoId, nalu, len );
                //mp4v2->AudioId = MP4AddAudioTrack( mp4v2->handle, mp4v2->AudioRate, 1024, MP4_MPEG4_AUDIO_TYPE );
                //MP4SetAudioProfileLevel( mp4v2->handle, 0x02 );
                
            }
                break;
        }
        case 0x08: // PPS
            if (FRIST_WRITE_VIDEO) {
                printf("pps(%d)\n", len);
                MP4AddH264PictureParameterSet( mp4v2->handle, mp4v2->VideoId, nalu, len );
                MP4SetVideoProfileLevel( mp4v2->handle, 0x7F );
                FRIST_WRITE_VIDEO = false;
            }
            break;
        case 0x06://SEI信息
            //printf( "SEI(%d)\n", len );
            break;
        case 0x05:
            //printf( "IDR slice(%d)\n", len );
            data[0] = ( len >> 24)&0xFF;
            data[1] = ( len >> 16)&0xFF;
            data[2] = ( len >> 8 )&0xFF;
            data[3] = ( len >> 0 )&0xFF;
            MP4WriteSample( mp4v2->handle, mp4v2->VideoId, data, len + 4 , MP4_INVALID_DURATION, 0, 1 );
            break;
        default:
            //printf("slice(%d)\n", len);
            data[0] = ( len >> 24)&0xFF;
            data[1] = ( len >> 16)&0xFF;
            data[2] = ( len >> 8 )&0xFF;
            data[3] = ( len >> 0 )&0xFF;
            MP4WriteSample( mp4v2->handle, mp4v2->VideoId, data, len + 4, MP4_INVALID_DURATION, 0, 0 );
            
            break;
            
        }
        data += len;
        size -= len;
    
    } 
}
int callback(CMS_Parser *parser, CMS_EVENT event, void *data, int size, void *context)
{
    switch (event) {
    case CMS_E_HEADER_FIELD      : {
        break;
    }
    case CMS_E_HEADER_END        :
        break;
    case CMS_E_PART_HEADER       :
        mp4v2->VideoSize = 0;
        break;
    case CMS_E_PART_HEADER_FIELD :{
        KEY_VALUE *kv = (KEY_VALUE *)data;
        if (strcmp(kv->value,"p") == 0 || strcmp(kv->value,"i") == 0) {
            ISVIDEO = true;
        } else if (strcmp(kv->value,"a") == 0) {
            
            ISVIDEO = false;
        }
        break;
    }
    case CMS_E_PART_HEADER_END   :
        break;
    case CMS_E_PART_END          :
        break;
    case CMS_E_CHUNK             : {
        if (ISVIDEO) {
            memcpy(VideoData + mp4v2->VideoSize, data, size);
            mp4v2->VideoSize += size;
            if (mp4v2->VideoSize == parser->part_length) {
                if(Write_Mp4(VideoData, parser->part_length) == -1){
                    printf("h264数据错误\n");
                    return -1;
                }
                memset(VideoData, 0, 1024 * 1024);
            }
        } else {
            /*int i;
            unsigned char *d = (unsigned char *)data;
            for ( i = 0; i < size; i++) {
                unsigned char alaw = d[i];
                faac->PCMBuffer[faac->pcm_ptr] = ALawDecompressTable[alaw];
                faac->pcm_ptr++;
                if(faac->pcm_ptr == faac->InputSamples) {
                    unsigned char AACBuffer[faac->MaxOutputBytes];
                    int nRet = faacEncEncode( faac->handle,
                                              (int*) faac->PCMBuffer,
                                              faac->pcm_ptr,
                                              AACBuffer,
                                              faac->MaxOutputBytes );
                    
                    MP4WriteSample( mp4v2->handle,
                                    mp4v2->AudioId,
                                    AACBuffer,
                                    nRet,
                                    MP4_INVALID_DURATION,
                                    0,
                                    1 );
                    faac->pcm_ptr = 0;
                    memset(faac->PCMBuffer, 0, faac->PCMBufferSize);
                }
                
                }*/
        }
            break;
    }
    case CMS_E_PARSE_FAIL        :
        //printf("CMS_E_PARSE_FAIL\n");
        break;
    }



}





int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: test <cms file> <mp4 file>\n");
        return 1;
     }

    VideoData = (unsigned char *)malloc(1024*1024);
    
    int CmsFile = open(argv[1], O_RDONLY);
    
    //mp4v2初始化
    mp4v2 = (MP4_INFORMATION *)malloc(sizeof(MP4_INFORMATION));
    mp4v2->handle = MP4Create( argv[2], 0 );
    if ( mp4v2->handle == MP4_INVALID_FILE_HANDLE ) {
		printf( "ERROR:Create mp4 handle fialed\n" );
		return -1;
    }
    MP4SetTimeScale(mp4v2->handle, 90000);
    
    //faac初始化
    faac = (FAAC_INFORMATION *)malloc(sizeof(FAAC_INFORMATION));
    faac->SampleRate = 8000;
    faac->Channels   = 1;
    faac->PCMBitSize = 16;
    faac->handle     = faacEncOpen( faac->SampleRate, faac->Channels, &faac->InputSamples, &faac->MaxOutputBytes );
    faac->PCMBufferSize = faac->InputSamples * faac->PCMBitSize / 8;
    faac->PCMBuffer     = (short int*)malloc(faac->PCMBufferSize);
    //faac编码器配置
    faacEncConfigurationPtr pConfiguration;
    pConfiguration              = faacEncGetCurrentConfiguration( faac->handle );
    pConfiguration->inputFormat = FAAC_INPUT_16BIT;
    faacEncSetConfiguration( faac->handle, pConfiguration );
    
    
    
    CMS_Parser *cms = new CMS_Parser(1024 * 1024);
    cms->set_callback(callback, NULL);
    char buf[4096];
    while (int len = read(CmsFile, buf, sizeof(buf))) {
        if(len > 0){
            cms->parse(buf, len);
        } else {
           
            break;
        }

    }

    
    MP4Close(mp4v2->handle, 0);
    free(VideoData);
    //free(pbPCMBuffer);
    faacEncClose(faac->handle);
    return 0;
}
