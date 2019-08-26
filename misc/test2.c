#include "./test.h"

int getNalu(CMS_DATA *vedio_data,unsigned char *buf,int len)
{
    memset(buf,0,1024*1024);
    unsigned char *nalu = vedio_data->data_buf;
    if(nalu[len + 0] != 0 || nalu[len + 1] != 0 || nalu[len + 2] != 0 || nalu[len + 3] != 1)
        return -1;
    int pos =  0;
    while(1)
    {
        if(len >= vedio_data->l){ break;}
        buf[pos] = nalu[len];
        if(nalu[pos-3] == 0 && nalu[pos-2] == 0 && nalu[pos-1] == 0 && nalu[pos] == 1)
        {
            len -= 4;
            break;
        }
        pos++;
        len++;
    }
    
    return len + 1;
}
int packet2Mp4(const char *outputFiles, CMS_HEAD *cms)
{
    unsigned char *buf = malloc(1024*1024);
    unsigned char *nalu = NULL;
    unsigned char naluType;
    int len = 0;
    MP4FileHandle handle = NULL;
    MP4TrackId videoId;
	int width = 1280;
	int height = 720;
	int rate = 15;
	int timeScale = 90000;
    int addStream = 1;

    handle = MP4Create(outputFiles, 0);
    if(handle == MP4_INVALID_FILE_HANDLE)
    {
		printf("ERROR:Create mp4 handle fialed.\n");
		return -1;
    }

    MP4SetTimeScale(handle, timeScale);
    int length = 0;
    CMS_DATA **vedio  = cms->vedio_data;
    while(length < cms->v_length){
        
        len = getNalu(vedio[length],buf,len);
        if(len >= cms->v_length){
            length++;
            len = 0;
        }
        if (buf[0] != 0 || buf[1] != 0 || buf[2] != 0 || buf[3] != 1){
            continue;
        }
        nalu = buf+4;
        naluType = nalu[0]&0x1F;
        
        switch (naluType)
        {
            case 0x07: // SPS
                printf("------------------------------------\n");
                printf("sps(%d)\n", len);
                if (addStream)
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

                    addStream = 0;
                }

                MP4AddH264SequenceParameterSet(handle, videoId, nalu, len);

                break;
            
            case 0x08: // PPS
                printf("pps(%d)\n", len);
                MP4AddH264PictureParameterSet(handle, videoId, nalu, len);
                break;

            default:
                printf("slice(%d)\n", len);
                buf[0] = (len>>24)&0xFF;
                buf[1] = (len>>16)&0xFF;
                buf[2] = (len>>8)&0xFF;
                buf[3] = (len>>0)&0xFF;

                MP4WriteSample(handle, videoId, buf, len+4, MP4_INVALID_DURATION, 0, 1);

                break;
        }
}

    free(buf);
    MP4Close(handle, 0);

    return 0;
}

int cms_parser(FILE *fp,CMS_HEAD *cms)
{
    char *boundary = NULL;
    char line_buf[1024];
    CMS_STATE state = CMS_FILE_HEADER;
    CMS_DATA *data;
    bool first = true;
    while(!feof(fp)){
        fgets(line_buf,1024,fp);
        switch(state){
        case CMS_FILE_HEADER:
            
            //printf("%d\n",__LINE__);
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
                //printf("%d\n",__LINE__);
                state = CMS_DATA1;
            }else{
                //出错处理
            }
            memset(line_buf,0,1024);
            break;
        case CMS_DATA1:
            // printf("%d\n",__LINE__);
            if(line_buf[0] == '\r' && line_buf[1] == '\n'){
                if(data->l != 0){
                    char *data_buf = malloc(data->l);
                    int n = -1,size = data->l;
                    n = fread(data_buf,1,size,fp);
                    while(n < size){
                        size -= n;
                        fseek(fp, 0, SEEK_END);
                        n =  fread(data_buf+n,1,size,fp);
                        if(n == 0){
                            if(feof(fp)){
                                break;
                            }else{
                                printf("读取数据错误\n");
                                break;
                            }
                        }
                    }
                    data->data_buf = data_buf;
                    printf("%c\n",data->f);
                    state = CMS_BOUNDARY;
                    first = true;
                }else{
                    //出错处理
                   
                }
            }else{
                if(first){
                    data = (CMS_DATA *)malloc(sizeof(CMS_DATA));
                    data->ts = 0;
                    data->l  = 0;
                    data->t  = 0;
                    first = false;
                }
               
                char *p     = line_buf;
                char *key   = strsep(&p, ":");
                char *value = p;
                if (strcmp(key, "f") == 0) {
                    if (strcmp(value, "i") == 0) {
                        data->f = 'i';
                        if(cms->v_length == 0){
                            cms->v_length++;
                            cms->vedio_data = data;
                        }else{
                            cms->v_length++;
                            cms->vedio_data = (CMS_DATA **)realloc(cms->vedio_data, sizeof(CMS_DATA *) * cms->v_length );
                            cms->vedio_data[cms->v_length] = data;
                        }
                        //
                    } else if (strcmp(value, "p") == 0) {
                        data->f = 'p';
                        if(cms->v_length == 0){
                            cms->v_length++;
                            cms->vedio_data = data;
                        }else{
                            cms->v_length++;
                            cms->vedio_data = (CMS_DATA **)realloc(cms->vedio_data, sizeof(CMS_DATA *) * cms->v_length );
                            cms->vedio_data[cms->v_length] = data;
                        }
                    } else if (strcmp(value, "a") == 0) {
                        data->f = 'a';
                        if(cms->a_length == 0){
                            cms->a_length++;
                            cms->audio_data = data;
                        }else{
                            cms->a_length++;
                            cms->audio_data = (CMS_DATA **)realloc(cms->audio_data, sizeof(CMS_DATA *) * cms->a_length );
                            cms->audio_data[cms->a_length] = data;
                        }
                    } else if (strcmp(value, "j") == 0) {
                        data->f = 'j';
                    }
                } else if (strcmp(key, "ts") == 0) {
                    data->ts = strtol(value, NULL, 10);
                } else if (strcmp(key, "l") == 0) {
                    data->l = strtol(value, NULL, 10);
                } else if (strcmp(key, "t") == 0) {
                    data->t = strtol(value, NULL, 10);
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
    
    CMS_HEAD cms;
    cms.v_length = 0;
    cms.a_length = 0;
    cms.vedio_data = (CMS_DATA **)malloc(sizeof(CMS_DATA *));
    cms.audio_data = (CMS_DATA **)malloc(sizeof(CMS_DATA *));
    
    cms_parser(fp,&cms);

    printf("vedio :%d  audio :%d\n",cms.v_length);
    packet2Mp4("ttt.mp4", &cms);
    

    return 0;
}
