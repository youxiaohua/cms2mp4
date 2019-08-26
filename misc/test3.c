#include "./test.h"

int getNalu(FILE *File,unsigned char *buf,bool *end)
{
    memset(buf,0,1024*1024);
    int pos =  0;
    int len;

    if((len = fread(buf, 1, 4, File)) <= 0)
        return -1;
    
    if(buf[0] != 0 || buf[1] != 0 || buf[2] != 0 || buf[3] != 1)
        return -1;
    pos += 4;
    while(1)
    {
        if(feof(File)){break;}
        
        buf[pos] = fgetc(File);
        if(buf[pos-1] == '\r' && buf[pos] == '\n'){
            pos -= 2;
            fseek(File,-2,SEEK_CUR);
            break;
            *end = true;
        }
        if(buf[pos-3] == 0 && buf[pos-2] == 0 && buf[pos-1] == 0 && buf[pos] == 1)
        {
            fseek(File,-4,SEEK_CUR);
            pos -= 4;
            break;
        }
        pos++;
    }
    len = pos;
    return len + 1;
}
int packet2Mp4(FILE *inputFile,int flag)
{
    printf("packetMP4  %d\n",__LINE__);
    unsigned char *buf = malloc(1024*1024);
    unsigned char *nalu = NULL;
    unsigned char naluType;
    int len = 0;
    int length = 0;
    MP4TrackId videoId;
    bool end   = false;
    while(!end){

        len = getNalu(inputFile,buf,&end);
        if(len <= 0){break;}
        if (buf[0] != 0 || buf[1] != 0 || buf[2] != 0 || buf[3] != 1){
            continue;
        }
        nalu = buf+4;
        naluType = nalu[0]&0x1F;
        
        switch (naluType)
        {
        case 0x07: // SPS
            if(!first && flag){break;}
            printf("------------------------------------\n");
            printf("sps(%d)\n", len);
            if (first)
            {
                videoId = MP4AddH264VideoTrack
                    (handle, 
                     timeScale,              // 一秒钟多少timescale
                     timeScale/rate,    // 每个帧有多少个timescale
                     width,                  // width
                     height,                 // height
                     nalu[1],               // sps[1] AVCProfileIndication
                     nalu[2],               // sps[2] profile_compat
                     nalu[3],               // sps[3] AVCLevelIndication
                     3);                     // 4 bytes length before each NAL unit
                if (videoId == MP4_INVALID_TRACK_ID)
                {
                    printf("Error:Can't add track.\n");
                    return -1;
                }
                
                MP4SetVideoProfileLevel(handle, 0x7F);
                
                first = false;
            }
            
            MP4AddH264SequenceParameterSet(handle, videoId, nalu, len);
            
            break;
            
        case 0x08: // PPS
            if(!first && flag){break;}
            printf("pps(%d)\n", len);
            MP4AddH264PictureParameterSet(handle, videoId, nalu, len);
            break;
        case 0x06://SEI信息
            break;
        case 0x05:
            printf("slice(%d)\n", len);
            buf[0] = (len>>24)&0xFF;
            buf[1] = (len>>16)&0xFF;
            buf[2] = (len>>8)&0xFF;
            buf[3] = (len>>0)&0xFF;
            
            MP4WriteSample(handle, videoId, buf, len+4, MP4_INVALID_DURATION, 0, 1);

            break;
        default:
            printf("slice(%d)\n", len);
            buf[0] = (len>>24)&0xFF;
            buf[1] = (len>>16)&0xFF;
            buf[2] = (len>>8)&0xFF;
            buf[3] = (len>>0)&0xFF;
            
            MP4WriteSample(handle, videoId, buf, len+4, MP4_INVALID_DURATION, 0, 0);
            
            break;
        }
}

    free(buf);


    return 0;
}

int cms_parser(FILE *fp,CMS_HEAD *cms)
{
    char *boundary = NULL;
    char line_buf[1024];
    CMS_STATE state = CMS_FILE_HEADER;
    CMS_DATA *data;
    CMS_DATA_TYPE data_state = UNKNOW;
    while(!feof(fp)){
        fgets(line_buf,1024,fp);
        switch(state){
        case CMS_FILE_HEADER:
            printf("%d\n",__LINE__);
            if(line_buf[0] == '\r' && line_buf[1] == '\n'){
                state = CMS_BOUNDARY;
            }else{
                char *p     = line_buf;
                char *key   = strsep(&p, ":");
                char *value = p;
                if (key && value) {
                    if (value[0] == ' ') { value++; };
                    if (strcmp(key, "boundary") == 0) {
                        
                        char buf[100];
                        sprintf(buf, "--%s", value);
                        boundary = strdup(buf);
                    } else if (strcmp(key, "format") == 0 && strcmp(value, "cms") == 0) {
                        //flag = flag | CMS_FLAG_FORMAT_FOUND;
                    } else if (strcmp(key, "track") == 0) {
                        //flag = flag | CMS_FLAG_TRACK_FOUND;
                        // cms->track = strdup(value);
                    }
                }
            }
            memset(line_buf,0,1024);
            break;
        case CMS_BOUNDARY:
            if(line_buf[0] == '\r' && line_buf[1] == '\n'){
            }else if(strcmp(line_buf,boundary) == 0){
                printf("%d\n",__LINE__);
                state = CMS_DATA_HEADER;
            }else{
                //出错处理
            }
            memset(line_buf,0,1024);
            break;
        case CMS_DATA_HEADER:
            
            if(line_buf[0] == '\r' && line_buf[1] == '\n'){
                switch(data_state){
                case CMS_VEDIO_P:
                    printf("%d\n",__LINE__);
                    packet2Mp4(fp,0);
                    state = CMS_BOUNDARY;
                    break;
                case CMS_VEDIO_I:
                    printf("%d\n",__LINE__);
                    packet2Mp4(fp,1);
                    state = CMS_BOUNDARY;
                    break;
                    
                }
                
            }else{               
                char *p     = line_buf;
                char *key   = strsep(&p, ":");
                char *value = p;
                printf("%s   %s\n",key,value);
                if (strcmp(key, "f") == 0) {
                    if (strcmp(value, "i") == 0) {
                        //data->f = 'i';
                        printf("%d\n",__LINE__);
                        data_state = CMS_VEDIO_I;
                        
                    } else if (strcmp(value, "p") == 0) {
                        //data->f = 'p';
                        printf("%d\n",__LINE__);
                        data_state = CMS_VEDIO_P;

                    } else if (strcmp(value, "a") == 0) {
                       
                    } else if (strcmp(value, "j") == 0) {
                        //data->f = 'j';
                    }
                } else if (strcmp(key, "ts") == 0) {
                    //data->ts = strtol(value, NULL, 10);
                } else if (strcmp(key, "l") == 0) {
                    //data->l = strtol(value, NULL, 10);
                    // data_length = strtol(value, NULL, 10);
                } else if (strcmp(key, "t") == 0) {
                    //data->t = strtol(value, NULL, 10);
                }
            }
            memset(line_buf,0,1024);
            break;
            
        }
        
    }
    return 0;
}


int main(int argc, char *argv[])
{
    if(argc < 2){
        printf("Usage  test <filename> \n");
        return 0;
    }
    FILE *fp = fopen(argv[1],"r");
    
    width = 1280;
	height = 720;
	rate = 15;
	timeScale = 90000;
    handle = NULL;
    handle = MP4Create(argv[2], 0);
    if(handle == MP4_INVALID_FILE_HANDLE)
    {
		printf("ERROR:Create mp4 handle fialed.\n");
		return -1;
    }

    MP4SetTimeScale(handle, timeScale);
    
    CMS_HEAD cms;    
    cms.v_length = 0;
    cms.a_length = 0;
    cms.vedio_data = (CMS_DATA **)malloc(sizeof(CMS_DATA *));
    cms.audio_data = (CMS_DATA **)malloc(sizeof(CMS_DATA *));
    cms_parser(fp,&cms);

    MP4Close(handle, 0);

    return 0;
}
