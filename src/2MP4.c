#include "2MP4.h"



//读取到track信息时 获取视频的信息
int get_attribute(char *value, BOX_TKHD *tkhd){
    char *trak1 = value;
    char *trak  = strsep(&value, ";");
    if(trak != NULL) {
        trak1 = trak;
    }
    trak1 = strtok(trak1,",");
    while(trak1 != NULL) {
        if(strstr(trak1, "width") != NULL) {
            tkhd->width = sw32( (uint32_t)atoi( &trak1[6] ) );
            printf("width : %d", atoi( &trak1[6] ));
        }
        if(strstr(trak1, "height") != NULL) {
            tkhd->height = sw32( (uint32_t)atoi( &trak1[7] ) );
            printf("height : %d", atoi( &trak1[7] ));
        }
        trak1 = strtok(NULL, ",");
    }
}
uint32_t add_sample_size(uint32_t sample_count, uint32_t *sample_sizes, int DataSize) {
    if(sample_count == 0) {
        sample_sizes[0] = sw32(DataSize);
        return sw32(1);
    }
    sample_count = sw32(sample_count) + 1;
    sample_sizes = realloc(sample_sizes, sizeof(uint32_t) * sample_count);
    sample_sizes[sample_count - 1] = strtol(value, NULL, 10);
    return sw32(sample_count);
}
//读取到时间间隔时写入 stts中
void add_delta( uint32_t delta, STTS_ENTRY *stts_entry, BOX_STTS *stts ) {
    if( delta == 0 ){ return; }
    delta = sw32( delta );
    stts->entry_count = sw32( stts->entry_count ); //先转换成小端序  用完再转回大端序
    if( stts->entry_count == 0 ) {
        stts_entry->sample_count = sw32( 1 );
        stts_entry->sample_delta = delta;
        return;
    }
    int i, find = 0;
    
    for( i = 0; i < stts->entry_count; i++ ) {
        if( stts_entry[i].sample_delta == delta){
            stts_entry[i].sample_count = stts_entry->sample_count + sw32( 1 );
            find = 1;
        }
    }
    if( find == 0 ) {
        stts->entry_count = stts->entry_count +  1 ;
        stts_entry = (STTS_ENTRY *)realloc(stts_entry, sizeof(STTS_ENTRY) * stts->entry_count);
        stts_entry[stts->entry_count].sample_count = sw32( 1 );
        stts_entry[stts->entry_count].sample_delta = delta;
    }
    stts->entry_count = sw32( stts->entry_count );
}

/* mp4文件中的box是网络字节序 写入数据的时候需要先转换字节序 */
int cms2mp4(FILE *cmsFile, char *mp4Name){
    //一级box
    BOX_FTYP ftyp;
    memset(&stts, 0, sizeof(BOX_STTS));
    BOX      mdat;
    memset(&mdat, 0, sizeof(BOX));
    BOX      moov;
    memset(&moov, 0, sizeof(BOX));
    //二级 在moov下
    BOX_MVHD mvhd;
    memset(&mvhd, 0, sizeof(BOX_MVHD));
    BOX      trak;
    memset(&trak, 0, sizeof(BOX));
    //三级 在trak下
    BOX_TKHD tkhd;
    memset(&tkhd, 0, sizeof(BOX_TKHD));
    BOX      mdia;
    memset(&mdia, 0, sizeof(BOX));
    //四级 在mdia下
    BOX_MDHD mdhd;
    memset(&mdhd, 0, sizeof(BOX_MDHD));
    BOX_HDLR hdlr;
    memset(&hdlr, 0, sizeof(BOX_HDLR));
    BOX      minf;
    memset(&minf, 0, sizeof(BOX));
    //五级 在minf下
    BOX_VMHD vmhd;
    memset(&vmhd, 0, sizeof(BOX_VMHD));
    BOX      dinf;
    memset(&dinf, 0, sizeof(BOX));
    BOX      stbl;
    memset(&stbl, 0, sizeof(BOX));
    //六级 在stbl下
    BOX_STSD stsd;
    memset(&stsd, 0, sizeof(BOX_STSD));
    BOX_STTS stts;
    memset(&stts, 0, sizeof(BOX_STTS));
    BOX_STSS stss;
    memset(&stss, 0, sizeof(BOX_STSS));
    BOX_STSC stsc;
    memset(&stsc, 0, sizeof(BOX_STSC));
    BOX_STSZ stsz;
    memset(&stsz, 0, sizeof(BOX_STSZ));
    BOX_STCO stco;
    memset(&stco, 0, sizeof(BOX_STCO));
    //七级 在stsd下
    BOX_AVC1 avc1;
    memset(&avc1, 0, sizeof(BOX_AVC1));
    //八级 在avc1下
    BOX_AVCC avcC;
    memset(&avcC, 0, sizeof(BOX_AVCC));
    

    
    
    FILE *mp4File = fopen( mp4Name, "wb" );
//写入mp4头部
    
    memset( &ftyp, 0, sizeof( ftyp ) );
    ftyp.header.boxSize = sw32( sizeof( ftyp ) );
    strncpy( (char *)&ftyp.header.boxType, BOX_TYPE_FTYP, 4 );
    FTYP_BRANDS f_isom = { "isom" };
    FTYP_BRANDS f_iso2 = { "iso2" };
    FTYP_BRANDS f_avc1 = { "avc1" };
    FTYP_BRANDS f_mp41 = { "mp41" };
    ftyp.minor_version         = sw32( 512 );
    ftyp.major_brand           = f_isom;
    ftyp.compatible_brands[0]  = f_isom;
    ftyp.compatible_brands[1]  = f_iso2;
    ftyp.compatible_brands[2]  = f_avc1;
    ftyp.compatible_brands[3]  = f_mp41;
    fwrite(&ftyp, sizeof(ftyp), 1, mp4File);

//写入mdat

    uint8_t *sps = NULL;
    uint8_t *pps = NULL;

    //stts entry 记录时间戳间隔和
    STTS_ENTRY *stts_entry    = (STTS_ENTRY *)calloc(1, sizeof(STTS_ENTRY));
    //stsz sample size 记录每个sample的大小
    uint32_t   *sample_sizes  = (uint32_t *)calloc(1, sizeof(uint32_t));
    //stsc entry
    STSC_ENTRY *stsc_entry    = (STSC_ENTRY *)calloc(1, sizeof(STSC_ENTRY));
    //stss sample number 记录关键帧序号
    uint32_t   *sample_number = (uint32_t *)calloc(1, sizeof(uint32_t));
    //stco chunk_offset  记录chunk的偏移量
    uint32_t   *chunk_offset  = (uint32_t *)calloc(1, sizeof(uint32_t));
    
    
//--------------------------------
    uint8_t flag             = 0;
    CMS_STATE state          = CMS_FILE_HEADER;
    CMS_DATA_TYPE data_state = UNKNOW;
    int DataSize = 0;
    int  last = 0, now = 0;  
    char ch;   
    while ( !feof( cmsFile ) ) {
        ch = fgetc( cmsFile );       
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
                            get_attribute(value, &tkhd);
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
                    fseek(cmsFile, -2, SEEK_CUR);
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
                        fseek(cmsFile, DataSize * -1, SEEK_CUR);
                        //Write_Mp4(fp, &head, handle, data_length);
                        state = CMS_BOUNDARY;
                        break;
                    case CMS_AUDIO:{
                        fseek(cmsFile, 480, SEEK_CUR);
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
                            //
                        }
                    } else if (strcmp(key, "ts") == 0) {
                        last = now;
                        now  = strtol(value, NULL, 10);
                        uint32_t delta = now - last;
                        add_time(delta, stts_entry, &stts);
                    } else if (strcmp(key, "l") == 0) {
                        if(data_state == CMS_VEDIO) {
                            DataSize = strtol(value, NULL, 10);
                            stsz.sample_count = add_sample_size(stsz.sample_count, sample_sizes, DataSize);
                            printf("sample_cpunt: %d  sample_sizes[%d]: %d", stsz.sample_count, stsz.sample_count - 1, sample_sizes[stsz.sample_count - 1]);
                        }
                        
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
    

//
    return 0;
}
 

int main(){
    cms2mp4(NULL, "123.mp4");

    return 0;
}
