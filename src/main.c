#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "mp4v2/mp4v2.h"
#include <faac.h>


#define CMS_FLAG_TRACK_FOUND 0x01
#define CMS_FLAG_FORMAT_FOUND 0x02

bool first = true; //判断是否是第一次读取数据,如果是则会创建视频和音频的通道
MP4TrackId videoId;
MP4TrackId audioId;
typedef enum{
    CMS_FILE_HEADER = 0,
    CMS_BOUNDARY,
    CMS_DATA_HEADER,

}CMS_STATE;

typedef enum{
    UNKNOW = -1,
    CMS_VEDIO = 0,
    CMS_AUDIO,
    CMS_JEPG,
}CMS_DATA_TYPE;



typedef struct{
    int width;
    int height;
    int video_rate;
    int timeScale;
    int audio_rate;
}header;

static short ALawDecompressTable[] =
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



int getNalu(FILE *File,unsigned char *buf,bool *end,long data_length)
{

    memset(buf,0,1024*1024);
    int pos;

    if ((pos = fread(buf, 1, 4, File)) <= 0) {
        return -1;
    }
    data_length -= 4;
    if (buf[0] != 0 || buf[1] != 0 || buf[2] != 0 || buf[3] != 1) {
        return -1;
    }
    pos = 4;
    while(1) {
        if(feof(File)){*end = true; break;}
        if(data_length == 0){        
            *end = true;
            break;
        }
        buf[pos] = fgetc(File);
        data_length--;
        if(buf[pos-3] == 0 && buf[pos-2] == 0 && buf[pos-1] == 0 && buf[pos] == 1)
        {
            fseek(File,-4,SEEK_CUR);
            data_length += 4;
            pos -= 4;
            break;
        }
        pos++;
    }
    return pos + 1;
}
int Write_Mp4 (FILE *File, header *head, MP4FileHandle handle,  long data_length)
{
    unsigned char *buf = malloc(1024*1024);
    unsigned char *nalu = NULL;
    unsigned char naluType;
    int len = 0;
    int length = 0;
    long ts;
    
    bool end   = false;
    while ( !end ) {

        len = getNalu( File, buf, &end, data_length );
        if ( len <= 0 ) { break; }
        if ( buf[0] != 0 || buf[1] != 0 || buf[2] != 0 || buf[3] != 1 ) {
            continue;
        }
        nalu = buf+4;
        naluType = nalu[0]&0x1F;
        
        switch ( naluType )
        {
        case 0x07: // SPS
            if ( !first ) { break; }
            
            if ( first )
            {
                printf("sps(%d)\n", len);
                videoId = MP4AddH264VideoTrack
                    (handle, 
                     head->timeScale,                  // 一秒钟多少timescale
                     8192,
                     //MP4_INVALID_DURATION,
                     //head->timeScale / head->video_rate,     // 每个帧有多少个timescale
                     head->width,                      // width
                     head->height,                     // height
                     nalu[1],                          // sps[1] AVCProfileIndication
                     nalu[2],                          // sps[2] profile_compat
                     nalu[3],                          // sps[3] AVCLevelIndication
                     3);                               // 4 bytes length before each NAL unit
                if ( videoId == MP4_INVALID_TRACK_ID )
                {
                    printf("创建视频 track 失败\n");
                    return -1;
                }
                audioId = MP4AddAudioTrack( handle, 8000, 1024, MP4_MPEG4_AUDIO_TYPE );
                if ( audioId == MP4_INVALID_TRACK_ID )
                {
                    printf("创建音频 track 失败\n");
                    return -1;
                }
                
            }
            first = true;
            MP4AddH264SequenceParameterSet( handle, videoId, nalu, len );
            
            break;
            
        case 0x08: // PPS
            if ( !first ) { break; }
            printf("pps(%d)\n", len);
            first = false;
            MP4AddH264PictureParameterSet( handle, videoId, nalu, len );
            MP4SetVideoProfileLevel( handle, 0x7F );
            MP4SetAudioProfileLevel( handle, 0x02 );
            
            break;
        case 0x06://SEI信息
            //printf( "SEI(%d)\n", len );
            break;
        case 0x05:
            //printf( "IDR slice(%d)\n", len );
            buf[0] = ( len >> 24)&0xFF;
            buf[1] = ( len >> 16)&0xFF;
            buf[2] = ( len >> 8 )&0xFF;
            buf[3] = ( len >> 0 )&0xFF;
            MP4WriteSample( handle, videoId, buf, len+4, /*ts*/MP4_INVALID_DURATION, 0, 1 );
            
            break;
        default:
            //printf("slice(%d)\n", len);
            buf[0] = ( len >> 24)&0xFF;
            buf[1] = ( len >> 16)&0xFF;
            buf[2] = ( len >> 8 )&0xFF;
            buf[3] = ( len >> 0 )&0xFF;
            MP4WriteSample( handle, videoId, buf, len+4, /*ts*/MP4_INVALID_DURATION, 0, 0 );
            
            break;
        }
}

    free(buf);


    return 0;
}



int cms_to_mp4( FILE *fp , char *Mp4File)
{
    header head     = { 0 };
    head.width      = 1280;
	head.height     = 720;
	head.video_rate = 15;
	head.timeScale  = 90000;
    
    MP4FileHandle handle    = NULL;
    handle = MP4Create( Mp4File, 0 );
    if ( handle == MP4_INVALID_FILE_HANDLE ) {
		printf( "ERROR:Create mp4 handle fialed.\n" );
		return -1;
    }
    MP4SetTimeScale( handle, head.timeScale );
    
    char  line_buf[1024];
    bool  isVedio       = true;
    char *boundary      = NULL;
    int   line_buf_ptr  = 0;
    int   boundary_ptr  = 0;
    long  data_length   = 0;
    int   boundary_size = 0;

    
    
    //faac
    faacEncHandle hEncoder        = { 0 };  //文件操作句柄
    unsigned long nSampleRate     = 8000;   //采样率
    unsigned int  nChannels       = 1;      //单双声道
    unsigned int  nPCMBitSize     = 16;     //pcm数据位数
    unsigned long nInputSamples   = 0;      //编码数据的长度
    unsigned long nMaxOutputBytes = 0;      //编码后数据的最大大小
    int nRet,pcm_ptr = 0;                   //
    //打开faac编码器
    hEncoder = faacEncOpen( nSampleRate, nChannels, &nInputSamples, &nMaxOutputBytes );
    if ( hEncoder == NULL ) { 
        printf( "faac编码器打开失败!\n" );
        return -1;
    }
    int nPCMBufferSize         = nInputSamples * nPCMBitSize/8;
    short int *pbPCMBuffer     = ( short int * )malloc( nPCMBufferSize );
    unsigned char *pbAACBuffer = ( unsigned char * )malloc(nMaxOutputBytes);
    printf( "每次解码pcm数据的大小[%d]\n", nPCMBufferSize );
    printf( "aac数据的大小[%d]\n", nMaxOutputBytes );
    /* getchar(); */
    //获取当前编码器的信息
    
    faacEncConfigurationPtr pConfiguration = { 0 };
    pConfiguration = faacEncGetCurrentConfiguration( hEncoder );
    
 	//输入数据类型
	pConfiguration->inputFormat   = FAAC_INPUT_16BIT;


    faacEncSetConfiguration( hEncoder, pConfiguration );
    


    
    uint8_t flag             = 0;
    CMS_STATE state          = CMS_FILE_HEADER;
    CMS_DATA_TYPE data_state = UNKNOW;
    char ch;
    
    while ( !feof( fp ) ) {
        ch = fgetc( fp );       
        switch ( state ) {
        case CMS_FILE_HEADER:
            if (ch == '\r') {
            } else if(ch == '\n') {
                if(line_buf_ptr == 0){
                    if (!boundary) {
                        printf("未识别到 boundary \n");
                        return -1;
                    } else if (!(flag & CMS_FLAG_FORMAT_FOUND)) {
                        printf("未识别到 format \n");
                        return -1;
                    }else if (!(flag & CMS_FLAG_TRACK_FOUND)) {
                        printf("未识别到 track \n");
                        return -1;
                    }else{
                        state = CMS_BOUNDARY;
                    }
                }else{
                    char *p     = line_buf;
                    char *key   = strsep(&p, ":");
                    char *value = p;
                    if (key && value) {
                        if ( value[0] == ' ' ) { value++; };
                        if ( strcmp(key, "boundary") == 0 ) {
                            char buf[100];
                            sprintf( buf, "\r\n--%s\r\n", value );
                            boundary = strdup( buf );
                            boundary_size = strlen( boundary );
                            boundary_ptr = 2;
                        } else if ( strcmp( key, "format" ) == 0 && strcmp(value, "cms") == 0 ) {
                            flag = flag | CMS_FLAG_FORMAT_FOUND;
                        } else if ( strcmp( key, "track" ) == 0) {
                            flag = flag | CMS_FLAG_TRACK_FOUND;
                            
                            
                        }
                    }
                    
                }
                line_buf_ptr = 0;
            }else{
                if(line_buf_ptr < sizeof(line_buf)) {
                    line_buf[line_buf_ptr] = ch;
                    line_buf_ptr++;
                    line_buf[line_buf_ptr] = 0;
                }else{
                    //出错处理
                    printf("line_buf_ptr超长\n");
                    return -1;
                }
            }
            break;
        case CMS_BOUNDARY:
            if ( ch == boundary[boundary_ptr] ) {
                boundary_ptr++;
            } else if ( boundary_ptr == boundary_size ) {
                state = CMS_DATA_HEADER;
                line_buf_ptr = 0;
                line_buf[0] = 0;
                boundary_ptr = 0;
            } else {
                if( ch == '\n' );
                {
                    
                    fseek( fp,-2,SEEK_CUR );
                    break;
                }
                printf("boundary error\n");
                return -1;
            }
            break;
        case CMS_DATA_HEADER:
            if ( ch == '\r' ) {
            } else if ( ch == '\n' ) {
                if ( line_buf_ptr == 0 ) {       
                    switch( data_state ) {
                    case CMS_VEDIO:
                        Write_Mp4(fp, &head, handle, data_length);
                        state = CMS_BOUNDARY;
                        break;
                    case CMS_AUDIO:{
                        int i;
                        for ( i = 0; i < 480; i++) {
                            unsigned char alaw = fgetc( fp );
                            pbPCMBuffer[pcm_ptr] = ALawDecompressTable[alaw];
                            pcm_ptr++;
                            if(pcm_ptr == nInputSamples) {
                                nRet = faacEncEncode( hEncoder, (int*) pbPCMBuffer, pcm_ptr, pbAACBuffer, nMaxOutputBytes );
                                MP4WriteSample( handle, audioId, pbAACBuffer, nRet, MP4_INVALID_DURATION, 0, 1 );
                                pcm_ptr = 0;
                                memset(pbPCMBuffer, 0, nPCMBufferSize);
                            }
                        
                        }    
                        /*
                        //lame库 pcm转mp3
                        int mp3Bytes = lame_encode_buffer(lame, pbPCMBuffer, NULL, 240,MP3Buffer,480);
                       
                        if(mp3Bytes > 0){
                            MP4WriteSample(handle, audioId, MP3Buffer, mp3Bytes, 56, 0, 1);
                            }
                        */
                        state = CMS_BOUNDARY;
                        break;
                    }
                    default:
                        state = CMS_BOUNDARY;
                        break;
                    
                    }
                } else {               
                    char *p     = line_buf;
                    char *key   = strsep(&p, ":");
                    char *value = p;
                    if ( strcmp(key, "f" ) == 0) {
                        if (strcmp(value, "i") == 0) {
                            data_state = CMS_VEDIO;
                        } else if (strcmp(value, "p") == 0) {
                            data_state = CMS_VEDIO;
                        } else if (strcmp(value, "a") == 0) {
                            data_state = CMS_AUDIO;
                        } else if (strcmp(value, "j") == 0) {
                        
                        }
                    } else if (strcmp(key, "ts") == 0) {
                     
                    } else if (strcmp(key, "l") == 0) {
                        data_length = strtol(value, NULL, 10);
                    } else if (strcmp(key, "t") == 0) {

                    }
                }
               
                line_buf_ptr = 0;
            } else {
                if (line_buf_ptr < sizeof(line_buf)) {
                    line_buf[line_buf_ptr] = ch;
                    line_buf_ptr++;
                    line_buf[line_buf_ptr] = 0;
                } else {
                    //出错处理
                    printf("line_buf_ptr超长\n");
                    return -1;
                }
            }
            break;
            
        }
    }
    //判断pbPCMBuffer是否还有数据未写入
    if ( pcm_ptr > 0 ) {
        
        nRet = faacEncEncode(hEncoder, (int*) pbPCMBuffer, pcm_ptr, pbAACBuffer, nMaxOutputBytes);
        MP4WriteSample(handle, audioId, pbAACBuffer, nRet, MP4_INVALID_DURATION, 0, 1);

    }
    MP4Close(handle, 0);
    free(pbAACBuffer);
    free(pbPCMBuffer);
    faacEncClose(hEncoder);
}


int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: test <source.cms> <target.mp4>\n");
        return 1;
    }
    FILE *fp = fopen(argv[1], "r");
    cms_to_mp4(fp,argv[2]);
    fclose(fp);
    return 0;
}
